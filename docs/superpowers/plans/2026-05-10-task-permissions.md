# Task Permissions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ad-hoc `is_lead` boolean with a `team_cap::flags` capability bitfield so that team members can edit non-archived tasks, org-only members get read-only board/task access, and archived tasks remain invisible to non-leads.

**Architecture:** A new header-only `team_cap.hpp` defines capabilities using `alt::flags<bit>` (same pattern as `permission.hpp`). A new `resolve_team_caps()` method on `altinf_app` computes caps once per route entry. `kanban_board_page` and `kanban_task_editor_page` receive `team_cap::flags` instead of `bool is_lead` and derive all UI locking from `caps.has_any(...)` callsites.

**Tech Stack:** C++23, Wt 4.x, `alt::flags<bit>` (`/usr/local/include/alt/flags.hpp`), Catch2 v3 (unit tests), Playwright (E2E).

---

## File Map

| Action | File |
|---|---|
| **Create** | `src/kanban/team_cap.hpp` |
| **Create** | `tests/test_team_cap.cpp` |
| **Create** | `e2e/specs/task-permissions.spec.ts` |
| **Modify** | `tests/CMakeLists.txt` |
| **Modify** | `src/altinf_app.hpp` |
| **Modify** | `src/altinf_app.cpp` |
| **Modify** | `src/pages/kanban_board_page.hpp` |
| **Modify** | `src/pages/kanban_board_page.cpp` |
| **Modify** | `src/pages/kanban_task_editor_page.hpp` |
| **Modify** | `src/pages/kanban_task_editor_page.cpp` |
| **Modify** | `src/kanban/kanban_db.hpp` |
| **Modify** | `src/kanban/kanban_db.cpp` |

---

## Task 1: `team_cap.hpp` + Catch2 unit tests

**Files:**
- Create: `src/kanban/team_cap.hpp`
- Create: `tests/test_team_cap.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing tests**

Create `tests/test_team_cap.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "kanban/team_cap.hpp"

TEST_CASE("team_cap - empty flags grants nothing")
{
	const team_cap::flags none{};
	CHECK(!none.has_any(team_cap::view_board));
	CHECK(!none.has_any(team_cap::edit_task_fields));
	CHECK(!none.has_any(team_cap::comment));
	CHECK(none.empty());
}

TEST_CASE("team_cap - org_viewer_caps has only view_board")
{
	CHECK(team_cap::org_viewer_caps.has_any(team_cap::view_board));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::edit_task_fields));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::self_assign));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::comment));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::view_archived));
	CHECK(!team_cap::org_viewer_caps.has_any(team_cap::create_task));
}

TEST_CASE("team_cap - team_member_caps has edit and comment but not lead caps")
{
	CHECK(team_cap::team_member_caps.has_any(team_cap::view_board));
	CHECK(team_cap::team_member_caps.has_any(team_cap::edit_task_fields));
	CHECK(team_cap::team_member_caps.has_any(team_cap::self_assign));
	CHECK(team_cap::team_member_caps.has_any(team_cap::comment));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::view_archived));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::reassign_task));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::create_task));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::archive_task));
	CHECK(!team_cap::team_member_caps.has_any(team_cap::manage_team));
}

TEST_CASE("team_cap - team_lead_caps is a superset of team_member_caps")
{
	CHECK(team_cap::team_lead_caps.has_all(team_cap::team_member_caps));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::view_archived));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::reassign_task));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::create_task));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::archive_task));
	CHECK(team_cap::team_lead_caps.has_any(team_cap::manage_team));
}

TEST_CASE("team_cap - org_lead_caps equals team_lead_caps")
{
	CHECK(team_cap::org_lead_caps == team_cap::team_lead_caps);
}

TEST_CASE("team_cap - admin_caps has all defined capabilities")
{
	CHECK(team_cap::admin_caps.has_any(team_cap::view_board));
	CHECK(team_cap::admin_caps.has_any(team_cap::view_archived));
	CHECK(team_cap::admin_caps.has_any(team_cap::edit_task_fields));
	CHECK(team_cap::admin_caps.has_any(team_cap::self_assign));
	CHECK(team_cap::admin_caps.has_any(team_cap::reassign_task));
	CHECK(team_cap::admin_caps.has_any(team_cap::create_task));
	CHECK(team_cap::admin_caps.has_any(team_cap::archive_task));
	CHECK(team_cap::admin_caps.has_any(team_cap::manage_team));
	CHECK(team_cap::admin_caps.has_any(team_cap::comment));
}
```

- [ ] **Step 2: Run the test to confirm it fails (header missing)**

```bash
cmake --build build -t test_team_cap 2>&1 | tail -10
```

Expected: build error — `"kanban/team_cap.hpp": No such file or directory`

- [ ] **Step 3: Create `src/kanban/team_cap.hpp`**

```cpp
#pragma once

#include <alt/flags.hpp>
#include <cstdint>

namespace team_cap
{
	enum class bit : uint32_t {};
	using flags = alt::flags<bit>;

	inline constexpr flags view_board       = flags::from_value(1u << 0);
	inline constexpr flags view_archived    = flags::from_value(1u << 1);
	inline constexpr flags edit_task_fields = flags::from_value(1u << 2);
	inline constexpr flags self_assign      = flags::from_value(1u << 3);
	inline constexpr flags reassign_task    = flags::from_value(1u << 4);
	inline constexpr flags create_task      = flags::from_value(1u << 5);
	inline constexpr flags archive_task     = flags::from_value(1u << 6);
	inline constexpr flags manage_team      = flags::from_value(1u << 7);
	inline constexpr flags comment          = flags::from_value(1u << 8);

	inline constexpr flags org_viewer_caps  = view_board;
	inline constexpr flags team_member_caps = view_board | edit_task_fields | self_assign | comment;
	inline constexpr flags team_lead_caps   = team_member_caps | view_archived | reassign_task
	                                        | create_task | archive_task | manage_team;
	inline constexpr flags org_lead_caps    = team_lead_caps;
	inline constexpr flags admin_caps       = ~flags{};
}
```

- [ ] **Step 4: Register the test in `tests/CMakeLists.txt`**

Add after the existing `test_permissions` block (which is also header-only):

```cmake
# test_team_cap — header-only, no extra libs
add_executable(test_team_cap tests/test_team_cap.cpp)
target_include_directories(test_team_cap PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_team_cap PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_team_cap)
```

- [ ] **Step 5: Build and run the tests**

```bash
cmake --build build -t test_team_cap && ctest --test-dir build -R test_team_cap -V
```

Expected: all 6 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/kanban/team_cap.hpp tests/test_team_cap.cpp tests/CMakeLists.txt
git commit -m "feat: add team_cap::flags capability bitfield with unit tests"
```

---

## Task 2: `resolve_team_caps` on `altinf_app`

**Files:**
- Modify: `src/altinf_app.hpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Add the include and declaration to `src/altinf_app.hpp`**

Add `#include "kanban/team_cap.hpp"` to the existing include block (after `kanban/kanban_db.hpp`):

```cpp
#include "kanban/kanban_db.hpp"
#include "kanban/team_cap.hpp"
```

Add the declaration alongside `resolve_is_org_lead`:

```cpp
bool            resolve_is_org_lead(long long org_id);
team_cap::flags resolve_team_caps(long long team_id, long long org_id);
```

- [ ] **Step 2: Add the implementation to `src/altinf_app.cpp`**

Place it immediately after the existing `resolve_is_org_lead` implementation (around line 155):

```cpp
team_cap::flags altinf_app::resolve_team_caps(long long team_id, long long org_id)
{
	if(m_session.permissions.has_any(permission::admin))
	{
		return team_cap::admin_caps;
	}
	if(!m_session.logged_in)
	{
		return {};
	}
	if(m_org_db->is_org_lead(org_id, m_session.username))
	{
		return team_cap::org_lead_caps;
	}
	if(m_kanban_db->is_team_lead(team_id, m_session.username))
	{
		return team_cap::team_lead_caps;
	}
	if(m_kanban_db->is_member(team_id, m_session.username))
	{
		return team_cap::team_member_caps;
	}
	if(m_org_db->is_org_member(org_id, m_session.username))
	{
		return team_cap::org_viewer_caps;
	}
	return {};
}
```

- [ ] **Step 3: Verify the build is clean**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep -E "error:|warning:" | head -20
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add src/altinf_app.hpp src/altinf_app.cpp
git commit -m "feat: add resolve_team_caps to altinf_app"
```

---

## Task 3: Update `kanban_board_page` + routing call sites

**Files:**
- Modify: `src/pages/kanban_board_page.hpp`
- Modify: `src/pages/kanban_board_page.cpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Update `src/pages/kanban_board_page.hpp`**

Add the include and swap the parameter/member types:

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/kanban_board_widget.hpp"
#include "kanban/gantt_view_widget.hpp"
#include "kanban/team_cap.hpp"

#include <Wt/WContainerWidget.h>

#include <map>
#include <memory>
#include <string>

class kanban_board_page: public Wt::WContainerWidget
{
public:
    kanban_board_page(kanban_db&          db,
                      const session_data& session,
                      long long           team_id,
                      team_cap::flags     caps,
                      bool                show_gantt);

    ~kanban_board_page() override;

private:
    kanban_db&                       m_db;
    long long                        m_team_id{0};
    long long                        m_org_id{0};
    team_cap::flags                  m_caps;
    bool                             m_show_gantt{false};
    std::string                      m_username;
    std::string                      m_session_id;
    std::shared_ptr<bool>            m_alive{std::make_shared<bool>(true)};
    std::map<long long, std::string> m_type_colors;
    kanban_board_widget*             m_board_widget{nullptr};
    gantt_view_widget*               m_gantt_widget{nullptr};

    void refresh();
};
```

- [ ] **Step 2: Update `src/pages/kanban_board_page.cpp`**

Replace the full constructor and `refresh()` with the caps-aware versions. The key changes are: `bool is_lead` → `team_cap::flags caps`, `m_is_lead{is_lead}` → `m_caps{caps}`, and split the `if(m_is_lead)` header-button block into three separate per-capability checks.

Constructor signature and init:
```cpp
kanban_board_page::kanban_board_page(kanban_db&          db,
                                     const session_data& session,
                                     long long           team_id,
                                     team_cap::flags     caps,
                                     bool                show_gantt):
  m_db{db},
  m_team_id{team_id},
  m_caps{caps},
  m_show_gantt{show_gantt},
  m_username{session.username}
```

Replace the `if(m_is_lead)` block (which renders New Task / Manage Team / Archived buttons) with:

```cpp
	if(caps.has_any(team_cap::create_task))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, team_url + "/task/new"},
		     "+ New Task")
		  ->setStyleClass("editor-btn kb-new-btn");
	}

	if(caps.has_any(team_cap::manage_team))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, team_url + "/manage"},
		     "Manage Team")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
	}

	if(caps.has_any(team_cap::view_archived))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, team_url + "/archive"},
		     "Archived")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
	}
```

Replace `m_is_lead` with `m_caps.has_any(team_cap::edit_task_fields)` in the `kanban_board_widget` constructor call and its drag callback:

```cpp
	m_board_widget = addNew<kanban_board_widget>(
	  tasks,
	  m_caps.has_any(team_cap::edit_task_fields),
	  m_type_colors,
	  [this](long long tid, const std::string& status, int sort) {
		  if(!m_caps.has_any(team_cap::edit_task_fields))
		  {
			  return;
		  }
		  m_db.update_task_status(tid, status, sort, m_username);
		  live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
		  m_board_widget->refresh(
		    m_db.tasks_for_team(m_team_id),
		    m_caps.has_any(team_cap::edit_task_fields),
		    m_type_colors);
	  },
	  [this](long long tid) {
		  Wt::WApplication::instance()->setInternalPath(
		    "/board/" + std::to_string(m_team_id) + "/task/" +
		      std::to_string(tid) + "/edit",
		    true);
	  });
```

Update `kanban_board_page::refresh()`:

```cpp
void kanban_board_page::refresh()
{
	m_type_colors.clear();
	for(const auto& ty: m_db.types_for_org(m_org_id))
	{
		m_type_colors[ty.id] = ty.color;
	}

	const auto tasks = m_db.tasks_for_team(m_team_id);
	if(m_show_gantt && m_gantt_widget)
	{
		m_gantt_widget->refresh(tasks, m_type_colors);
	}
	else if(m_board_widget)
	{
		m_board_widget->refresh(
		  tasks,
		  m_caps.has_any(team_cap::edit_task_fields),
		  m_type_colors);
	}
}
```

- [ ] **Step 3: Update the board/gantt call sites in `src/altinf_app.cpp`**

Replace the three-line `is_org_lead` / `is_team_lead` / `is_lead` block and the two `can_view_board` checks with a single `caps` computation:

```cpp
		const auto caps = resolve_team_caps(team_id, team->org_id);

		const std::string suffix = suffix_after_id(path, 7);

		if(suffix.empty() || suffix == "/")
		{
			if(!caps.has_any(team_cap::view_board))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<kanban_board_page>(
			  *m_kanban_db, m_session, team_id, caps, false);
		}
		else if(suffix == "/gantt")
		{
			if(!caps.has_any(team_cap::view_board))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<kanban_board_page>(
			  *m_kanban_db, m_session, team_id, caps, true);
		}
```

Leave the rest of the routing block unchanged for now (Task 4 handles the remaining routes).

- [ ] **Step 4: Verify the build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep -E "error:" | head -20
```

Expected: no errors.

- [ ] **Step 5: Commit**

```bash
git add src/pages/kanban_board_page.hpp src/pages/kanban_board_page.cpp src/altinf_app.cpp
git commit -m "feat: kanban_board_page uses team_cap::flags instead of bool is_lead"
```

---

## Task 4: Update `kanban_task_editor_page` + routing call sites

**Files:**
- Modify: `src/pages/kanban_task_editor_page.hpp`
- Modify: `src/pages/kanban_task_editor_page.cpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Update `src/pages/kanban_task_editor_page.hpp`**

Add include, swap parameter and member:

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/team_cap.hpp"
#include "org/org_db.hpp"

#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WDateEdit.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WTabWidget.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class kanban_task_editor_page: public Wt::WContainerWidget
{
public:
    kanban_task_editor_page(kanban_db&                          db,
                            org_db&                             odb,
                            long long                           team_id,
                            const session_data&                 session,
                            team_cap::flags                     caps,
                            const kanban_task_entry*            existing,
                            const std::vector<std::string>&     members,
                            const std::vector<task_type_entry>& types,
                            std::function<void()>               on_save);

    ~kanban_task_editor_page() override;

private:
    kanban_db&               m_db;
    org_db&                  m_odb;
    long long                m_team_id;
    std::string              m_username;
    team_cap::flags          m_caps;
    const kanban_task_entry* m_existing;
    std::function<void()>    m_on_save;

    long long             m_task_id{0};
    std::string           m_session_id;
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

    Wt::WLineEdit*    m_title{nullptr};
    Wt::WTextArea*    m_description{nullptr};
    Wt::WComboBox*    m_status{nullptr};
    Wt::WComboBox*    m_assigned_to{nullptr};
    Wt::WDateEdit*    m_start_date{nullptr};
    Wt::WDateEdit*    m_end_date{nullptr};
    long long                          m_type_id{0};
    std::vector<Wt::WContainerWidget*> m_type_chips;
    Wt::WText*                         m_status_msg{nullptr};
    Wt::WPushButton*                   m_save_btn{nullptr};
    Wt::WPushButton*                   m_del_btn{nullptr};
    Wt::WContainerWidget*              m_history_panel{nullptr};
    Wt::WContainerWidget*              m_comment_list{nullptr};
    Wt::WContainerWidget*              m_comment_compose{nullptr};

    std::vector<std::string> m_assignee_values;

    static const std::vector<std::string> k_status_vals;
    static const std::vector<std::string> k_status_labels;

    void save();
    void mark_stale();
    void rebuild_history();
    void rebuild_comments();
};
```

- [ ] **Step 2: Update the constructor in `src/pages/kanban_task_editor_page.cpp`**

Change the signature and init, and compute `can_edit`/`can_assign`/`locked_out` from caps:

```cpp
kanban_task_editor_page::kanban_task_editor_page(
  kanban_db&                          db,
  org_db&                             odb,
  long long                           team_id,
  const session_data&                 session,
  team_cap::flags                     caps,
  const kanban_task_entry*            existing,
  const std::vector<std::string>&     members,
  const std::vector<task_type_entry>& types,
  std::function<void()>               on_save):
  m_db{db},
  m_odb{odb},
  m_team_id{team_id},
  m_username{session.username},
  m_caps{caps},
  m_existing{existing},
  m_on_save{std::move(on_save)}
{
	setStyleClass("page kb-editor-page");

	const bool        is_new           = (existing == nullptr);
	const std::string current_assignee = existing ? existing->assigned_to : "";

	const bool can_edit   = caps.has_any(team_cap::edit_task_fields)
	                     && (!existing || !existing->is_archived);
	const bool can_assign = caps.has_any(team_cap::reassign_task)
	                     && (!existing || !existing->is_archived);
	const bool locked_out = !can_assign
	                     && !current_assignee.empty()
	                     && current_assignee != session.username;
```

- [ ] **Step 3: Update field locking in the constructor**

Replace every `is_lead`-based lock with caps-based logic. Apply these changes throughout the constructor body:

**Title** (was `!is_lead && existing != nullptr`):
```cpp
	m_title->setReadOnly(!can_edit && existing != nullptr);
```

**Description** — was unrestricted; now locked for non-editors:
```cpp
	m_description->setReadOnly(!can_edit);
```

**Status** — was unrestricted; now disabled for non-editors:
```cpp
	m_status->setDisabled(!can_edit);
```

**Assignee dropdown** (was gated on `is_lead`):
```cpp
	if(can_assign)
	{
		for(const auto& m: members)
		{
			m_assignee_values.push_back(m);
			m_assigned_to->addItem(m);
		}
	}
	else
	{
		m_assignee_values.push_back(session.username);
		m_assigned_to->addItem(session.username);
	}
```

**Dates** — were unrestricted; now locked for non-editors:
```cpp
	m_start_date->setReadOnly(!can_edit);
	m_end_date->setReadOnly(!can_edit);
```

**Type chips** — add `setDisabled(!can_edit)` to each chip widget after creating it. After the `add_chip` lambda, add:
```cpp
	if(!can_edit)
	{
		for(auto* chip: m_type_chips)
		{
			chip->setDisabled(true);
		}
	}
```

**Save button** — hide if user cannot edit existing tasks and this is an existing task:
```cpp
	m_save_btn =
	  btn_row->addNew<Wt::WPushButton>(is_new ? "Create Task" : "Save Changes");
	m_save_btn->setStyleClass("editor-btn");
	m_save_btn->clicked().connect([this] { save(); });

	if(!can_edit && !is_new)
	{
		m_save_btn->hide();
	}
```

**Archive/Unarchive button** (was `!is_new && is_lead`):
```cpp
	if(!is_new && caps.has_any(team_cap::archive_task))
	{
		// ... existing archive/unarchive button code unchanged ...
	}
```

**Comment section** — only render compose if user can comment:
```cpp
	if(existing)
	{
		auto* comment_section = form->addNew<Wt::WContainerWidget>();
		comment_section->setStyleClass("kb-comment-section");
		comment_section->addNew<Wt::WText>("<h2>Comments</h2>", Wt::TextFormat::UnsafeXHTML);
		m_comment_list = comment_section->addNew<Wt::WContainerWidget>();
		m_comment_list->setStyleClass("kb-comment-list");
		if(caps.has_any(team_cap::comment))
		{
			m_comment_compose = comment_section->addNew<Wt::WContainerWidget>();
			m_comment_compose->setStyleClass("kb-comment-compose");
		}
	}
```

- [ ] **Step 4: Update `rebuild_comments` — `can_act` check**

Replace:
```cpp
			const bool can_act = (c.author == m_username) || m_is_lead;
```
With:
```cpp
			const bool can_act = m_caps.has_any(team_cap::comment) &&
			                     ((c.author == m_username) ||
			                      m_caps.has_any(team_cap::manage_team));
```

- [ ] **Step 5: Update `save()` — server-side guards**

Add a guard at the top of `save()` before any existing logic:

```cpp
void kanban_task_editor_page::save()
{
	// Server-side guard — prevents editing if UI was bypassed.
	if(m_existing &&
	   (!m_caps.has_any(team_cap::edit_task_fields) || m_existing->is_archived))
	{
		m_status_msg->setText("You do not have permission to edit this task.");
		return;
	}
```

Replace the `m_is_lead` assignment check (was `if(!m_is_lead && ...)`):
```cpp
	if(!m_caps.has_any(team_cap::reassign_task) && new_assignee != old_assignee)
	{
		if(new_assignee != "" && new_assignee != m_username)
		{
			m_status_msg->setText("You cannot assign this task to another user.");
			return;
		}
		if(!old_assignee.empty() && old_assignee != m_username)
		{
			m_status_msg->setText("This task is already assigned to someone else.");
			return;
		}
	}
```

- [ ] **Step 6: Update task call sites in `src/altinf_app.cpp`**

`caps` is already computed earlier in the routing block (Task 3). Update the three remaining routes:

**`/task/new`** (was `!is_lead`):
```cpp
		else if(suffix == "/task/new")
		{
			if(!caps.has_any(team_cap::create_task))
			{
				show_forbidden();
				return;
			}
			const auto members = m_kanban_db->members_for_team(team_id);
			const auto types   = m_kanban_db->types_for_org(team->org_id);
			m_content->addNew<kanban_task_editor_page>(
			  *m_kanban_db, *m_org_db, team_id, m_session, caps, nullptr, members, types,
			  [this, team_id] {
				  setInternalPath("/board/" + std::to_string(team_id), true);
			  });
		}
```

**`/task/{id}/edit`** — replace `can_view_board` call and add archived guard, pass `caps`:
```cpp
		else if(suffix.starts_with("/task/") && suffix.ends_with("/edit"))
		{
			const auto task_id_opt = parse_id(suffix, 6);
			if(!task_id_opt)
			{
				show_not_found();
				return;
			}
			const auto opt = m_kanban_db->find_task(*task_id_opt);
			if(!opt || opt->team_id != team_id)
			{
				show_not_found("Task not found.");
				return;
			}
			if(!caps.has_any(team_cap::view_board))
			{
				show_forbidden();
				return;
			}
			if(opt->is_archived && !caps.has_any(team_cap::view_archived))
			{
				show_not_found("Task not found.");
				return;
			}
			m_edit_task        = opt;
			const auto members = m_kanban_db->members_for_team(team_id);
			const auto types   = m_kanban_db->types_for_org(team->org_id);
			m_content->addNew<kanban_task_editor_page>(
			  *m_kanban_db, *m_org_db, team_id, m_session, caps, &(*m_edit_task),
			  members, types,
			  [this, team_id] {
				  setInternalPath("/board/" + std::to_string(team_id), true);
			  });
		}
```

**`/archive`** (was `!is_lead`):
```cpp
		else if(suffix == "/archive")
		{
			if(!caps.has_any(team_cap::view_archived))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<kanban_archive_page>(
			  *m_kanban_db, m_session, team_id);
		}
```

**`/manage`** (was `!is_lead`):
```cpp
		else if(suffix == "/manage")
		{
			if(!caps.has_any(team_cap::manage_team))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<kanban_team_page>(
			  *m_org_db, *m_kanban_db, *m_user_db, team->org_id, m_session,
			  "/board/" + std::to_string(team_id));
		}
```

- [ ] **Step 7: Verify the build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep -E "error:" | head -20
```

Expected: no errors.

- [ ] **Step 8: Commit**

```bash
git add src/pages/kanban_task_editor_page.hpp src/pages/kanban_task_editor_page.cpp src/altinf_app.cpp
git commit -m "feat: kanban_task_editor_page uses team_cap::flags; org-only viewers get read-only access"
```

---

## Task 5: Remove `can_view_board` and `can_edit_board` from `kanban_db`

**Files:**
- Modify: `src/kanban/kanban_db.hpp`
- Modify: `src/kanban/kanban_db.cpp`

- [ ] **Step 1: Remove declarations from `src/kanban/kanban_db.hpp`**

Delete these two lines (around line 73):
```cpp
	bool can_view_board(long long team_id, const std::string& username,
	                    permission::flags perms, bool is_org_lead);
	bool can_edit_board(long long team_id, const std::string& username,
	                    permission::flags perms, bool is_org_lead, bool is_team_lead);
```

If the `permission.hpp` include is only used for these two declarations, remove it too. Check for other usages first:

```bash
grep -n "permission::" src/kanban/kanban_db.hpp
```

If no other usages, remove:
```cpp
#include "auth/permission.hpp"
```

- [ ] **Step 2: Remove implementations from `src/kanban/kanban_db.cpp`**

Delete the `// ---- Permission helpers ----` comment and both function bodies (around line 610–635 in the original file).

If `permission.hpp` is included in `kanban_db.cpp` only for these functions, remove that include too. Verify first:
```bash
grep -n "permission::" src/kanban/kanban_db.cpp
```

- [ ] **Step 3: Verify the build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep -E "error:" | head -20
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add src/kanban/kanban_db.hpp src/kanban/kanban_db.cpp
git commit -m "refactor: remove can_view_board/can_edit_board from kanban_db (replaced by team_cap)"
```

---

## Task 6: E2E tests for task permissions

**Files:**
- Create: `e2e/specs/task-permissions.spec.ts`

The tests need three fresh users (`perm_member`, `perm_orgonly`, `perm_outsider`), an org (`PermOrg`), and a team (`PermTeam`). `perm_member` joins the team; `perm_orgonly` joins the org only; `perm_outsider` joins nothing.

- [ ] **Step 1: Start the server**

```bash
cmake --build build --parallel $(nproc)
./build/altinf \
  --docroot ./build/resources \
  --http-address 0.0.0.0 --http-port 8080 \
  --https-address 0.0.0.0 --https-port 8443 \
  --ssl-certificate ./certs/cert.pem \
  --ssl-private-key ./certs/key.pem \
  --ssl-tmp-dh ./certs/dh.pem &
```

- [ ] **Step 2: Create `e2e/specs/task-permissions.spec.ts`**

```typescript
import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG  = 'PermOrg';
const TEAM = 'PermTeam';

// URLs captured during setup so tests can use them.
let boardUrl  = '';
let taskUrl   = '';
let archivedTaskUrl = '';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function createUser(page: Page, username: string, password: string) {
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await page.locator('.account-new-btn').click();
  await page.locator('input[placeholder="Username (required)"]').fill(username);
  await page.locator('input[placeholder="Password (required)"]').fill(password);
  await page.locator('input[placeholder="Confirm password"]').fill(password);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
}

async function inviteToOrg(page: Page, username: string) {
  await page.locator('.kb-member-input').fill(username);
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText(`Invite sent to ${username}`);
}

async function acceptInvite(page: Page, username: string, password: string) {
  const ctx  = await page.context().browser()!.newContext();
  const p    = await ctx.newPage();
  await loginAs(p, username, password);
  await p.locator('.nav-bell-link').click();
  await expect(p.locator('.notifications-page')).toBeVisible();
  await p.getByRole('button', { name: 'Accept' }).click();
  await ctx.close();
}

async function goToBoard(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

async function openTask(page: Page, title: string) {
  await page.locator('.kb-card', { hasText: title }).first().locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await (await browser.newContext()).newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create the three test users.
  await createUser(page, 'perm_member',  'permpass');
  await createUser(page, 'perm_orgonly', 'permpass');
  await createUser(page, 'perm_outsider','permpass');

  // Create org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();

  // Open manage page; create team; invite perm_member and perm_orgonly.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  await page.locator('input[placeholder="Team name"]').fill(TEAM);
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  await inviteToOrg(page, 'perm_member');
  await inviteToOrg(page, 'perm_orgonly');

  // Both users accept their org invites.
  await acceptInvite(page, 'perm_member',  'permpass');
  await acceptInvite(page, 'perm_orgonly', 'permpass');

  // Add perm_member to the team (not as lead).
  // The "Add to team" drop-down shows org members not yet on the team.
  const addSelect = page.locator('.kb-team-block select').first();
  await addSelect.selectOption({ label: 'perm_member' });
  await page.locator('.kb-team-block .kb-member-add-row .editor-btn-cancel').click();
  await expect(page.locator('.kb-team-block .kb-member-row', { hasText: 'perm_member' })).toBeVisible();

  // Navigate to the board as admin; create two tasks.
  await page.locator('.kb-back-link').click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();

  boardUrl = page.url();

  // Create PermTask (stays active).
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('PermTask');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Open PermTask editor and capture its URL.
  await page.locator('.kb-card', { hasText: 'PermTask' }).first().locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  taskUrl = page.url();
  await page.goBack();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Create PermArchived, then archive it.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('PermArchived');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.locator('.kb-card', { hasText: 'PermArchived' }).first().locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  archivedTaskUrl = page.url();
  await page.locator('.editor-btn-danger', { hasText: 'Archive' }).click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.context().close();
});

// ── Team member (non-lead) tests ──────────────────────────────────────────────

test('perm_member: sees the board', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await goToBoard(page);
  await expect(page.locator('.kb-board')).toBeVisible();
  await ctx.close();
});

test('perm_member: no "New Task" button on board', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await goToBoard(page);
  await expect(page.locator('.kb-new-btn')).not.toBeVisible();
  await ctx.close();
});

test('perm_member: task fields are editable', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(taskUrl);
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  // Title is writable.
  await expect(page.locator('input[placeholder="Task title"]')).not.toHaveAttribute('readonly');
  // Save button is visible.
  await expect(
    page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')
  ).toBeVisible();
  // No Archive button.
  await expect(page.locator('.editor-btn-danger', { hasText: 'Archive' })).not.toBeVisible();
  await ctx.close();
});

test('perm_member: can save a title change', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(taskUrl);
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('PermTask-Edited');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(page.locator('.kb-card', { hasText: 'PermTask-Edited' })).toBeVisible();
  await ctx.close();
});

test('perm_member: archived task URL returns not-found', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(archivedTaskUrl);
  // The task editor should NOT load.
  await expect(page.locator('.kb-editor-page')).not.toBeVisible();
  await ctx.close();
});

// ── Org-only member (read-only) tests ─────────────────────────────────────────

test('perm_orgonly: sees the board', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await goToBoard(page);
  await expect(page.locator('.kb-board')).toBeVisible();
  await ctx.close();
});

test('perm_orgonly: no "New Task" or "Manage Team" buttons', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await goToBoard(page);
  await expect(page.locator('.kb-new-btn')).not.toBeVisible();
  await expect(page.locator('.kb-manage-link')).not.toBeVisible();
  await ctx.close();
});

test('perm_orgonly: task fields are read-only and no save button', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await page.goto(taskUrl);
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  // Title is read-only.
  await expect(page.locator('input[placeholder="Task title"]')).toHaveAttribute('readonly', '');
  // Save button hidden.
  await expect(
    page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)')
  ).not.toBeVisible();
  // No comment compose area.
  await expect(page.locator('.kb-comment-compose')).not.toBeVisible();
  await ctx.close();
});

test('perm_orgonly: archived task URL returns not-found', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await page.goto(archivedTaskUrl);
  await expect(page.locator('.kb-editor-page')).not.toBeVisible();
  await ctx.close();
});

// ── Non-member tests ──────────────────────────────────────────────────────────

test('perm_outsider: board URL is forbidden', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_outsider', 'permpass');
  await page.goto(boardUrl);
  // The board widget must not be visible.
  await expect(page.locator('.kb-board')).not.toBeVisible();
  await ctx.close();
});
```

- [ ] **Step 3: Run the E2E tests**

```bash
cd e2e && npx playwright test task-permissions.spec.ts --reporter=list
```

Expected: all 11 tests pass. If any fail, diagnose with `--headed` or `--debug` to observe the UI.

- [ ] **Step 4: Run all three test suites**

```bash
# Catch2
ctest --test-dir build -V

# JS unit tests
cd tests/js && node test_gantt.js

# Playwright (full suite)
cd e2e && npx playwright test --reporter=list
```

Expected: all suites pass.

- [ ] **Step 5: Commit**

```bash
git add e2e/specs/task-permissions.spec.ts
git commit -m "test(e2e): add task permissions tests for team member and org-only access"
```
