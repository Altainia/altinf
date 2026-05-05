# Navbar Login Link, Login Autofocus, Org Enter Key — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Login" navbar link for logged-out users, auto-focus the username field on the login page, and make Enter in the invite/team-name inputs trigger the respective form action.

**Architecture:** Three independent, single-file changes to the C++ Wt UI layer, each paired with a new Playwright E2E test written first (TDD). No new files, no new C++ classes, no cross-file dependencies.

**Tech Stack:** C++ / Wt widget library, Playwright E2E (`e2e/`), CMake build.

---

## File Map

| File | Change |
|------|--------|
| `e2e/specs/login.spec.ts` | Add 3 new tests: nav login link visible, link navigates to login, autofocus on load |
| `src/widgets/nav_bar.cpp` | Add login `WAnchor` to `m_auth_area` when not logged in |
| `src/pages/login_page.cpp` | Call `m_username->setFocus()` after creating the username field |
| `e2e/specs/live-org-manage.spec.ts` | Add 2 new tests: Enter in invite input, Enter in team-name input |
| `src/pages/kanban_team_page.cpp` | Hoist invite and create lambdas; connect `enterPressed()` to each |

---

## Task 1: Navbar Login Link

**Files:**
- Test: `e2e/specs/login.spec.ts`
- Modify: `src/widgets/nav_bar.cpp:45–48`

- [ ] **Step 1: Write the failing tests**

Append to `e2e/specs/login.spec.ts`:

```typescript
test('logged-out user sees login link in navbar', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('.nav-login')).toBeVisible();
  await expect(page.locator('.nav-login')).toHaveText('Login');
});

test('navbar login link navigates to login page', async ({ page }) => {
  await page.goto('/');
  await page.locator('.nav-login').click();
  await expect(page.locator('.login-form')).toBeVisible();
});
```

- [ ] **Step 2: Run to confirm they fail**

```bash
cd e2e && npx playwright test specs/login.spec.ts
```

Expected: the two new tests FAIL with "Locator not visible" — `.nav-login` does not exist yet.

- [ ] **Step 3: Implement the navbar change**

In `src/widgets/nav_bar.cpp`, replace lines 45–48:

```cpp
	if(!m_session.logged_in)
	{
		return;
	}
```

with:

```cpp
	if(!m_session.logged_in)
	{
		m_auth_area->addNew<Wt::WAnchor>(
		             Wt::WLink{Wt::LinkType::InternalPath, "/login"}, "Login")
		  ->setStyleClass("nav-link nav-login");
		return;
	}
```

- [ ] **Step 4: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: build succeeds with no errors.

- [ ] **Step 5: Run login tests to confirm they pass**

```bash
cd e2e && npx playwright test specs/login.spec.ts
```

Expected: all tests in `login.spec.ts` PASS, including the two new ones.

- [ ] **Step 6: Commit**

```bash
git add src/widgets/nav_bar.cpp e2e/specs/login.spec.ts
git commit -m "feat: show login link in navbar when logged out"
```

---

## Task 2: Login Page Autofocus

**Files:**
- Test: `e2e/specs/login.spec.ts`
- Modify: `src/pages/login_page.cpp:21–24`

- [ ] **Step 1: Write the failing test**

Append to `e2e/specs/login.spec.ts`:

```typescript
test('login page auto-focuses username field', async ({ page }) => {
  await page.goto('/?_=/login');
  await expect(page.locator('input[placeholder="Username"]')).toBeFocused();
});
```

- [ ] **Step 2: Run to confirm it fails**

```bash
cd e2e && npx playwright test specs/login.spec.ts --grep "auto-focuses"
```

Expected: FAIL — the username input is not focused.

- [ ] **Step 3: Implement the autofocus**

In `src/pages/login_page.cpp`, add `m_username->setFocus();` after the `setStyleClass` call on the username field. The block should look like:

```cpp
	m_username = form->addNew<Wt::WLineEdit>();
	m_username->setPlaceholderText("Username");
	m_username->setStyleClass("login-field");
	m_username->setFocus();
```

- [ ] **Step 4: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: build succeeds with no errors.

- [ ] **Step 5: Run login tests to confirm they pass**

```bash
cd e2e && npx playwright test specs/login.spec.ts
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/pages/login_page.cpp e2e/specs/login.spec.ts
git commit -m "feat: auto-focus username field on login page load"
```

---

## Task 3: Org Management Enter Key

**Files:**
- Test: `e2e/specs/live-org-manage.spec.ts`
- Modify: `src/pages/kanban_team_page.cpp:73–93` (invite form) and `116–127` (create team form)

- [ ] **Step 1: Write the failing tests**

Append to `e2e/specs/live-org-manage.spec.ts` (after the last existing test, still inside the file):

```typescript
test('org manage: pressing Enter in invite input sends invite', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create a fresh user 'frank' for this test.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await page.locator('.account-new-btn').click();
  await page.locator('input[placeholder="Username (required)"]').fill('frank');
  await page.locator('input[placeholder="Password (required)"]').fill('frankpass');
  await page.locator('input[placeholder="Confirm password"]').fill('frankpass');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  await goToManage(page);
  await page.locator('.kb-member-input').fill('frank');
  await page.locator('.kb-member-input').press('Enter');
  await expect(page.locator('.editor-status')).toContainText('Invite sent to frank');

  await ctx.close();
});

test('org manage: pressing Enter in team name input creates team', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToManage(page);

  await page.locator('input[placeholder="Team name"]').fill('EnterKeyTeam');
  await page.locator('input[placeholder="Team name"]').press('Enter');
  await expect(page.locator('.kb-team-block input[value="EnterKeyTeam"]')).toBeVisible();

  await ctx.close();
});
```

- [ ] **Step 2: Run to confirm they fail**

```bash
cd e2e && npx playwright test specs/live-org-manage.spec.ts --grep "pressing Enter"
```

Expected: both new tests FAIL — pressing Enter in these fields currently does nothing, so the status message and team block never appear.

- [ ] **Step 3: Implement the invite Enter key**

In `src/pages/kanban_team_page.cpp`, replace lines 73–93:

```cpp
	invite_btn->clicked().connect(
	  [this] {
		  const std::string u = m_invite_input->text().toUTF8();
		  if(u.empty())
		  {
			  m_invite_msg->setText("Enter a username.");
			  return;
		  }
		  if(!m_udb.username_exists(u))
		  {
			  m_invite_msg->setText("User \"" + u + "\" does not exist.");
			  return;
		  }
		  m_odb.invite_to_org(m_org_id, u, m_invite_lead->isChecked());
		  live_hub::instance().broadcast("user:" + u);
		  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
		  m_invite_input->setText("");
		  m_invite_lead->setChecked(false);
		  m_invite_msg->setText("Invite sent to " + u + ".");
		  refresh_pending();
	  });
```

with:

```cpp
	auto do_invite = [this] {
		const std::string u = m_invite_input->text().toUTF8();
		if(u.empty())
		{
			m_invite_msg->setText("Enter a username.");
			return;
		}
		if(!m_udb.username_exists(u))
		{
			m_invite_msg->setText("User \"" + u + "\" does not exist.");
			return;
		}
		m_odb.invite_to_org(m_org_id, u, m_invite_lead->isChecked());
		live_hub::instance().broadcast("user:" + u);
		live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
		m_invite_input->setText("");
		m_invite_lead->setChecked(false);
		m_invite_msg->setText("Invite sent to " + u + ".");
		refresh_pending();
	};
	invite_btn->clicked().connect(do_invite);
	m_invite_input->enterPressed().connect(do_invite);
```

- [ ] **Step 4: Implement the create-team Enter key**

In the same file, replace lines 116–127:

```cpp
	new_team_btn->clicked().connect(
	  [this] {
		  const std::string name = m_new_team_input->text().toUTF8();
		  if(name.empty())
		  {
			  return;
		  }
		  m_kdb.create_team(name, m_org_id);
		  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
		  m_new_team_input->setText("");
		  refresh_teams();
	  });
```

with:

```cpp
	auto do_create = [this] {
		const std::string name = m_new_team_input->text().toUTF8();
		if(name.empty())
		{
			return;
		}
		m_kdb.create_team(name, m_org_id);
		live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
		m_new_team_input->setText("");
		refresh_teams();
	};
	new_team_btn->clicked().connect(do_create);
	m_new_team_input->enterPressed().connect(do_create);
```

- [ ] **Step 5: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: build succeeds with no errors.

- [ ] **Step 6: Run org manage tests to confirm they pass**

```bash
cd e2e && npx playwright test specs/live-org-manage.spec.ts
```

Expected: all tests in `live-org-manage.spec.ts` PASS, including the two new ones.

- [ ] **Step 7: Commit**

```bash
git add src/pages/kanban_team_page.cpp e2e/specs/live-org-manage.spec.ts
git commit -m "feat: trigger invite/create on Enter key in org management forms"
```

---

## Final: Full Test Suite

- [ ] **Run all three suites**

```bash
# Playwright E2E
cd e2e && npx playwright test

# JS unit tests
cd tests/js && npm test

# C++ unit tests (Catch2)
cd build && ctest --output-on-failure
```

Expected: all suites pass with no regressions.
