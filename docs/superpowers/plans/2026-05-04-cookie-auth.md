# Cookie-Based Auth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Wt's URL-embedded session tracking for auth state with an application-owned `HttpOnly; Secure; SameSite=Strict` browser cookie backed by a `session_token` DB table, so login survives page refreshes and new sessions independently of the Wt session lifecycle.

**Architecture:** On login, generate a 32-byte random token, SHA-256 hash it into the `session_token` table, and set a 30-day `HttpOnly; Secure; SameSite=Strict` cookie with the raw token. In the `altinf_app` constructor, before building the UI, read the cookie and restore `m_session` from the DB if the token is valid. On logout, delete the DB row and clear the cookie.

**Tech Stack:** Wt 4.13 (`WApplication::setCookie`, `WEnvironment::getCookie`, `Http::Cookie`), Wt Dbo / SQLite, OpenSSL (already used for SHA-256 in `user_db.cpp`), Catch2 v3.

---

## File Map

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `src/auth/session_token.hpp` | Dbo mapping for the `session_token` table |
| Modify | `src/auth/user_db.hpp` | Declare `create_session_token`, `verify_session_token`, `delete_session_token` |
| Modify | `src/auth/user_db.cpp` | Map class, add migration, implement the three methods |
| Modify | `src/altinf_app.hpp` | Add `m_session_token` member (`std::string`) |
| Modify | `src/altinf_app.cpp` | Read cookie in ctor, set cookie on login, remove cookie on logout, subscribe to hub if restored |
| Modify | `tests/test_user_db.cpp` | Tests for the three new `user_db` methods |

---

### Task 1: `session_token` Dbo model

**Files:**
- Create: `src/auth/session_token.hpp`

- [ ] **Step 1: Create the header**

```cpp
// src/auth/session_token.hpp
#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

struct session_token
{
	std::string token_hash;
	std::string username;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, token_hash, "token_hash");
		Wt::Dbo::field(a, username, "username");
	}
};
```

- [ ] **Step 2: Commit**

```bash
git add src/auth/session_token.hpp
git commit -m "feat: add session_token Dbo model"
```

---

### Task 2: `user_db` — DB methods for session tokens

**Files:**
- Modify: `src/auth/user_db.hpp`
- Modify: `src/auth/user_db.cpp`

- [ ] **Step 1: Write the failing tests** (add to end of `tests/test_user_db.cpp`)

```cpp
TEST_CASE("user_db - create_session_token returns 64-char hex token")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto tok = db.create_session_token("alice");
	CHECK(tok.size() == 64);
}

TEST_CASE("user_db - verify_session_token populates session_data")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::admin, "Alice");
	const auto   raw = db.create_session_token("alice");
	session_data out;
	REQUIRE(db.verify_session_token(raw, out));
	CHECK(out.logged_in);
	CHECK(out.username == "alice");
	CHECK(out.display_name == "Alice");
	CHECK(out.permissions == permission::admin);
}

TEST_CASE("user_db - verify_session_token bad token returns false")
{
	user_db      db{":memory:"};
	session_data out;
	CHECK(!db.verify_session_token(
	  "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef", out));
	CHECK(!out.logged_in);
}

TEST_CASE("user_db - delete_session_token invalidates it")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	const auto   raw = db.create_session_token("alice");
	session_data out;
	REQUIRE(db.verify_session_token(raw, out));
	db.delete_session_token(raw);
	CHECK(!db.verify_session_token(raw, out));
}

TEST_CASE("user_db - delete_session_token unknown token is a no-op")
{
	user_db db{":memory:"};
	db.create_user("alice", "pw", permission::none);
	db.create_session_token("alice");
	// should not throw
	db.delete_session_token(
	  "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef");
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build --parallel $(nproc) --target test_user_db 2>&1 | tail -5
```

Expected: compile error — `create_session_token`, `verify_session_token`, `delete_session_token` not declared.

- [ ] **Step 3: Declare the methods in `user_db.hpp`**

Add to the public section after `verify_api_token`:

```cpp
	// Returns the raw token to present once; stores only its SHA-256 hash.
	std::string create_session_token(const std::string& username);

	bool verify_session_token(const std::string& raw_token, session_data& out_session);

	void delete_session_token(const std::string& raw_token);
```

- [ ] **Step 4: Add `#include "session_token.hpp"` to `user_db.cpp`**

Add after the existing includes at the top of `src/auth/user_db.cpp`:

```cpp
#include "session_token.hpp"
```

- [ ] **Step 5: Map the class and add the migration in `user_db.cpp`**

In `user_db::user_db`, add `mapClass<session_token>` alongside the existing mappings:

```cpp
	m_dbo.mapClass<user>("user");
	m_dbo.mapClass<api_token>("api_token");
	m_dbo.mapClass<session_token>("session_token");
```

In the `catch(const Wt::Dbo::Exception&)` block, add after the existing `api_token` migration:

```cpp
		m_dbo.execute(
		  "CREATE TABLE IF NOT EXISTS \"session_token\" ("
		  "  \"id\" integer primary key autoincrement,"
		  "  \"version\" integer not null,"
		  "  \"token_hash\" text not null,"
		  "  \"username\" text not null"
		  ")");
```

- [ ] **Step 6: Implement the three methods** (add at end of `user_db.cpp`)

```cpp
std::string user_db::create_session_token(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(users.empty())
	{
		throw std::runtime_error{"Unknown user: " + username};
	}

	const auto raw_token = generate_raw_token();
	const auto hash      = sha256_hex(raw_token);

	auto tok                 = m_dbo.add(std::make_unique<session_token>());
	tok.modify()->token_hash = hash;
	tok.modify()->username   = username;

	return raw_token;
}

bool user_db::verify_session_token(const std::string& raw_token, session_data& out)
{
	const auto hash = sha256_hex(raw_token);

	Wt::Dbo::Transaction t{m_dbo};

	const auto tokens =
	  m_dbo.find<session_token>().where("token_hash = ?").bind(hash).resultList();
	if(tokens.empty())
	{
		return false;
	}

	const auto tok_username = (*tokens.begin())->username;

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(tok_username).resultList();
	if(users.empty())
	{
		return false;
	}

	const auto found = *users.begin();
	out.logged_in    = true;
	out.username     = tok_username;
	out.display_name = found->display_name;
	out.permissions  = found->permissions;

	return true;
}

void user_db::delete_session_token(const std::string& raw_token)
{
	const auto hash = sha256_hex(raw_token);

	Wt::Dbo::Transaction t{m_dbo};

	const auto tokens =
	  m_dbo.find<session_token>().where("token_hash = ?").bind(hash).resultList();
	for(const auto& tok: tokens)
	{
		Wt::Dbo::ptr<session_token> p = tok;
		p.remove();
	}
}
```

- [ ] **Step 7: Run tests to verify they pass**

```bash
cmake --build build --parallel $(nproc) --target test_user_db && \
  ctest --test-dir build -V -R test_user_db
```

Expected: all `user_db` tests pass, including the five new ones.

- [ ] **Step 8: Commit**

```bash
git add src/auth/session_token.hpp src/auth/user_db.hpp src/auth/user_db.cpp \
        tests/test_user_db.cpp
git commit -m "feat: add session_token table and user_db methods"
```

---

### Task 3: Wire the cookie into `altinf_app`

**Files:**
- Modify: `src/altinf_app.hpp`
- Modify: `src/altinf_app.cpp`

There is no Catch2 test for `altinf_app` (it depends on the Wt runtime), so verification is a build check plus manual smoke test.

- [ ] **Step 1: Add `m_session_token` to `altinf_app.hpp`**

Add after the `m_session` member:

```cpp
	session_data m_session;
	std::string  m_session_token; // raw token; empty if not cookie-authed
```

- [ ] **Step 2: Add the `Http::Cookie` include to `altinf_app.cpp`**

Add after the existing Wt includes at the top of `src/altinf_app.cpp`:

```cpp
#include <Wt/Http/Cookie.h>
```

- [ ] **Step 3: Restore session from cookie in the constructor**

In `altinf_app::altinf_app`, add the cookie restore block immediately after the three DB objects are constructed and before `root()->setStyleClass(...)`:

```cpp
	// Restore auth from persistent cookie if present.
	if(const auto* raw = env.getCookie("altinf_session"))
	{
		if(m_user_db->verify_session_token(*raw, m_session))
		{
			m_session_token = *raw;
		}
	}
```

- [ ] **Step 4: Set the cookie on login**

In `handle_path`, find the `"/login"` branch. Replace the lambda:

```cpp
	else if(path == "/login")
	{
		m_content->addNew<login_page>(*m_user_db, m_session, [this] {
			const auto raw_token = m_user_db->create_session_token(m_session.username);
			m_session_token      = raw_token;
			Http::Cookie c{"altinf_session", raw_token};
			c.setHttpOnly(true);
			c.setSecure(true);
			c.setSameSite(Http::Cookie::SameSite::Strict);
			c.setMaxAge(std::chrono::seconds{30 * 24 * 3600});
			setCookie(c);
			m_nav->update();
			register_with_hub();
			setInternalPath("/", true);
		});
	}
```

- [ ] **Step 5: Clear the cookie on logout**

In `handle_path`, find the `"/logout"` branch. Replace it:

```cpp
	else if(path == "/logout")
	{
		live_hub::instance().unsubscribe("user:" + m_session.username, sessionId());
		if(!m_session_token.empty())
		{
			m_user_db->delete_session_token(m_session_token);
			removeCookie(Http::Cookie{"altinf_session"});
			m_session_token.clear();
		}
		m_session = session_data{};
		m_nav->update();
		setInternalPath("/", true);
	}
```

- [ ] **Step 6: Subscribe to live hub if session was restored from cookie**

At the end of the constructor, after `handle_path(internalPath())`, add:

```cpp
	if(m_session.logged_in && !m_session_token.empty())
	{
		register_with_hub();
	}
```

- [ ] **Step 7: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -20
```

Expected: build succeeds with no errors or warnings.

- [ ] **Step 8: Smoke test**

Start the server, log in, refresh the page. Confirm you remain logged in. Navigate to `/logout`, confirm you are logged out and the cookie is cleared (check browser devtools → Application → Cookies).

- [ ] **Step 9: Run all test suites**

```bash
ctest --test-dir build -V
```

Expected: all suites pass.

- [ ] **Step 10: Commit**

```bash
git add src/altinf_app.hpp src/altinf_app.cpp
git commit -m "feat: restore and persist auth via HttpOnly session cookie"
```
