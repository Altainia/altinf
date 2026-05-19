# Phase 2: Notification Dispatch, User Preferences, and Team Settings UI

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire up four notification event types (`task_available`, `task_unassigned`, `task_abandoned`, `task_coassignee_changed`), add a user notification preferences page at `/settings`, and build a team settings page at `/board/{team_id}/settings` that also takes over team rename from the org manage page.

**Architecture:** Notification logic lives in a new standalone helper module `kanban_notifications` (two free functions called from `task_editor_form_widget` after assignee mutations). The two settings pages follow the same `WContainerWidget`-subclass pattern used everywhere else in the app. The `/settings` route serves per-org notification pref toggles; `/board/{team_id}/settings` serves team-name rename and the four `allow_*` permission toggles.

**Tech Stack:** C++17, Wt 4.x (Wt::Dbo ORM on SQLite), Catch2 v3. Build: `cmake --build build --parallel $(nproc)`. C++ tests: `cd build && ctest --output-on-failure`. Wt include path: the CMakeLists uses `${CMAKE_SOURCE_DIR}/src` as the include root, so `#include "kanban/foo.hpp"` resolves to `src/kanban/foo.hpp`.

---

## File Map

| File | Change |
|---|---|
| `src/org/org.hpp` | Add four inline payload helpers |
| `src/kanban/kanban_notifications.hpp` | Create: two free-function declarations |
| `src/kanban/kanban_notifications.cpp` | Create: implement both helpers |
| `src/kanban/task_editor_form_widget.cpp` | Replace inline notification push with helper calls |
| `src/pages/notifications_page.cpp` | Add four rendering branches |
| `src/pages/settings_page.hpp` | Create: `settings_page` class |
| `src/pages/settings_page.cpp` | Create: per-org pref toggle UI |
| `src/widgets/nav_bar.cpp` | Add "Settings" link for logged-in users |
| `src/altinf_app.hpp` | No change (no new page pointer needed) |
| `src/altinf_app.cpp` | Add `#include` for two new pages; register two new routes |
| `src/pages/team_settings_page.hpp` | Create: `team_settings_page` class |
| `src/pages/team_settings_page.cpp` | Create: rename + permission toggles |
| `src/pages/kanban_board_page.cpp` | Add "Settings" link for leads |
| `src/pages/kanban_team_page.cpp` | Remove rename button and its handler |
| `tests/test_kanban_notifications.cpp` | Create: unit tests for both helper functions |
| `tests/CMakeLists.txt` | Register `test_kanban_notifications` target |

---

## Task 1: Payload helpers in `src/org/org.hpp`

**Files:**
- Modify: `src/org/org.hpp`

- [ ] **Step 1: Append four inline payload helpers to org.hpp**

Add after the existing `make_task_assigned_payload` function (around line 180):

```cpp
inline std::string make_task_available_payload(long long          task_id,
                                               const std::string& task_title,
                                               long long          team_id,
                                               const std::string& team_name)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"}";
}

inline std::string make_task_unassigned_payload(long long          task_id,
                                                const std::string& task_title,
                                                long long          team_id,
                                                const std::string& team_name)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"}";
}

inline std::string make_task_abandoned_payload(long long          task_id,
                                               const std::string& task_title,
                                               long long          team_id,
                                               const std::string& team_name,
                                               const std::string& abandoned_by)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"" +
	       ",\"abandoned_by\":\"" + abandoned_by + "\"}";
}

inline std::string make_task_coassignee_changed_payload(long long          task_id,
                                                        const std::string& task_title,
                                                        long long          team_id,
                                                        const std::string& team_name,
                                                        const std::string& changed_user,
                                                        const std::string& action)
{
	return "{\"task_id\":" + std::to_string(task_id) +
	       ",\"task_title\":\"" + task_title + "\"" +
	       ",\"team_id\":" + std::to_string(team_id) +
	       ",\"team_name\":\"" + team_name + "\"" +
	       ",\"changed_user\":\"" + changed_user + "\"" +
	       ",\"action\":\"" + action + "\"}";
}
```

- [ ] **Step 2: Build to confirm compiles clean**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -10
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add src/org/org.hpp
git commit -m "$(cat <<'EOF'
feat(org): add task_available/unassigned/abandoned/coassignee_changed payload helpers

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: `kanban_notifications` module — stub + failing tests

**Files:**
- Create: `src/kanban/kanban_notifications.hpp`
- Create: `src/kanban/kanban_notifications.cpp` (stub bodies)
- Create: `tests/test_kanban_notifications.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create `src/kanban/kanban_notifications.hpp`**

```cpp
#pragma once

#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <string>
#include <vector>

// Call after a successful add_assignee().
// Fires task_assigned to added_user (if actor != added_user) and
// task_coassignee_changed to all other current assignees who opt in.
void notify_assignee_added(kanban_db&         db,
                           org_db&            odb,
                           long long          task_id,
                           long long          team_id,
                           long long          org_id,
                           const std::string& added_user,
                           const std::string& actor);

// Call after a successful remove_assignee().
// remaining_assignees: result of db.assignees_for_task(task_id) after removal.
// Fires task_unassigned, task_abandoned, task_available, task_coassignee_changed
// according to spec rules (see kanban_notifications.cpp).
void notify_assignee_removed(kanban_db&                       db,
                             org_db&                          odb,
                             long long                        task_id,
                             long long                        team_id,
                             long long                        org_id,
                             const std::string&               removed_user,
                             const std::string&               actor,
                             const std::vector<std::string>&  remaining_assignees);
```

- [ ] **Step 2: Create `src/kanban/kanban_notifications.cpp` with stub bodies**

```cpp
#include "kanban_notifications.hpp"

void notify_assignee_added(kanban_db&,
                           org_db&,
                           long long,
                           long long,
                           long long,
                           const std::string&,
                           const std::string&)
{
	// stub — implemented in Task 3
}

void notify_assignee_removed(kanban_db&,
                             org_db&,
                             long long,
                             long long,
                             long long,
                             const std::string&,
                             const std::string&,
                             const std::vector<std::string>&)
{
	// stub — implemented in Task 3
}
```

- [ ] **Step 3: Add `test_kanban_notifications` to `tests/CMakeLists.txt`**

Append after the `test_org_db` block:

```cmake
# test_kanban_notifications
add_executable(test_kanban_notifications tests/test_kanban_notifications.cpp
  ${CMAKE_SOURCE_DIR}/src/kanban/kanban_db.cpp
  ${CMAKE_SOURCE_DIR}/src/kanban/kanban_notifications.cpp
  ${CMAKE_SOURCE_DIR}/src/org/org_db.cpp
  ${CMAKE_SOURCE_DIR}/src/widgets/live_hub.cpp)
target_include_directories(test_kanban_notifications PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_kanban_notifications PRIVATE Catch2::Catch2WithMain Wt::Wt wtdbo wtdbosqlite3)
catch_discover_tests(test_kanban_notifications)
```

- [ ] **Step 4: Create `tests/test_kanban_notifications.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "kanban/kanban_db.hpp"
#include "kanban/kanban_notifications.hpp"
#include "org/org_db.hpp"
#include "org/org.hpp"

static kanban_task_entry make_task(long long team_id, const std::string& title)
{
	kanban_task_entry e;
	e.team_id    = team_id;
	e.title      = title;
	e.status     = "todo";
	e.sort_order = 0;
	return e;
}

// ---- notify_assignee_added ----

TEST_CASE("notify_assignee_added: task_assigned fires when lead adds member")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "bob", "lead"); // bob is now assigned

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "bob", "lead");

	const auto notifs = odb.notifications_for_user("bob");
	REQUIRE(notifs.size() == 1);
	CHECK(notifs[0].type == "task_assigned");
}

TEST_CASE("notify_assignee_added: no task_assigned when self-assigning")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "alice");

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "alice", "alice");

	CHECK(odb.notifications_for_user("alice").empty());
}

TEST_CASE("notify_assignee_added: coassignee_changed fires to other current assignees")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	kdb.add_member(team_id, "carol");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob",   "lead");
	kdb.add_assignee(task_id, "carol", "lead"); // carol just added

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "carol", "lead");

	const auto alice_notifs = odb.notifications_for_user("alice");
	CHECK(!alice_notifs.empty());
	CHECK(alice_notifs[0].type == "task_coassignee_changed");

	const auto bob_notifs = odb.notifications_for_user("bob");
	CHECK(!bob_notifs.empty());
	CHECK(bob_notifs[0].type == "task_coassignee_changed");

	// carol is the added user — no coassignee_changed to herself
	const auto carol_notifs = odb.notifications_for_user("carol");
	// carol may get task_assigned (lead added her) but not task_coassignee_changed
	const bool got_coassignee = std::any_of(carol_notifs.begin(), carol_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_coassignee_changed"; });
	CHECK(!got_coassignee);
}

TEST_CASE("notify_assignee_added: pref disabled suppresses coassignee_changed")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob",   "lead");

	user_org_pref_entry pref;
	pref.username                 = "alice";
	pref.org_id                   = org_id;
	pref.notify_task_available    = true;
	pref.notify_task_unassigned   = true;
	pref.notify_coassignee_changed = false; // opted out
	pref.notify_task_abandoned    = true;
	odb.set_user_org_pref(pref);

	notify_assignee_added(kdb, odb, task_id, team_id, org_id, "bob", "lead");

	// alice opted out — no coassignee_changed
	const auto alice_notifs = odb.notifications_for_user("alice");
	const bool got_coassignee = std::any_of(alice_notifs.begin(), alice_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_coassignee_changed"; });
	CHECK(!got_coassignee);
}

// ---- notify_assignee_removed ----

TEST_CASE("notify_assignee_removed: task_unassigned fires when lead removes member")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob",   "lead");
	kdb.remove_assignee(task_id, "bob", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "lead", remaining);

	const auto notifs = odb.notifications_for_user("bob");
	REQUIRE(!notifs.empty());
	CHECK(notifs[0].type == "task_unassigned");
}

TEST_CASE("notify_assignee_removed: no task_unassigned on self-remove")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob",   "lead");
	kdb.remove_assignee(task_id, "bob", "bob"); // self-remove
	const auto remaining = kdb.assignees_for_task(task_id);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "bob", remaining);

	const auto notifs = odb.notifications_for_user("bob");
	const bool got_unassigned = std::any_of(notifs.begin(), notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_unassigned"; });
	CHECK(!got_unassigned);
}

TEST_CASE("notify_assignee_removed: task_unassigned pref disabled suppresses it")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob",   "lead");
	kdb.remove_assignee(task_id, "bob", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	user_org_pref_entry pref;
	pref.username                 = "bob";
	pref.org_id                   = org_id;
	pref.notify_task_available    = true;
	pref.notify_task_unassigned   = false; // opted out
	pref.notify_coassignee_changed = true;
	pref.notify_task_abandoned    = true;
	odb.set_user_org_pref(pref);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "lead", remaining);

	const auto notifs = odb.notifications_for_user("bob");
	const bool got_unassigned = std::any_of(notifs.begin(), notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_unassigned"; });
	CHECK(!got_unassigned);
}

TEST_CASE("notify_assignee_removed: task_available fires when last assignee removed by lead")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead");
	const auto remaining = kdb.assignees_for_task(task_id); // empty

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	const auto bob_notifs = odb.notifications_for_user("bob");
	const bool got_available = std::any_of(bob_notifs.begin(), bob_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(got_available);
}

TEST_CASE("notify_assignee_removed: task_abandoned + task_available on self-remove as sole assignee")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "alice"); // self-remove as sole
	const auto remaining = kdb.assignees_for_task(task_id); // empty

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "alice", remaining);

	const auto bob_notifs = odb.notifications_for_user("bob");
	const bool got_abandoned = std::any_of(bob_notifs.begin(), bob_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_abandoned"; });
	const bool got_available = std::any_of(bob_notifs.begin(), bob_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(got_abandoned);
	CHECK(got_available);
}

TEST_CASE("notify_assignee_removed: no task_abandoned when lead removes sole assignee")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead"); // lead removes alice
	const auto remaining = kdb.assignees_for_task(task_id); // empty

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	const auto bob_notifs = odb.notifications_for_user("bob");
	const bool got_abandoned = std::any_of(bob_notifs.begin(), bob_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_abandoned"; });
	CHECK(!got_abandoned); // only self-remove triggers task_abandoned
}

TEST_CASE("notify_assignee_removed: task_available pref disabled suppresses it")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead");
	const auto remaining = kdb.assignees_for_task(task_id);

	user_org_pref_entry pref;
	pref.username                 = "bob";
	pref.org_id                   = org_id;
	pref.notify_task_available    = false; // opted out
	pref.notify_task_unassigned   = true;
	pref.notify_coassignee_changed = true;
	pref.notify_task_abandoned    = true;
	odb.set_user_org_pref(pref);

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	const auto bob_notifs = odb.notifications_for_user("bob");
	const bool got_available = std::any_of(bob_notifs.begin(), bob_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(!got_available);
}

TEST_CASE("notify_assignee_removed: coassignee_changed fires to remaining assignees")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	kdb.add_member(team_id, "bob");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.add_assignee(task_id, "bob",   "lead");
	kdb.remove_assignee(task_id, "bob", "lead");
	const auto remaining = kdb.assignees_for_task(task_id); // ["alice"]

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "bob", "lead", remaining);

	const auto alice_notifs = odb.notifications_for_user("alice");
	const bool got_coassignee = std::any_of(alice_notifs.begin(), alice_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_coassignee_changed"; });
	CHECK(got_coassignee);
}

TEST_CASE("notify_assignee_removed: excluded from self-notification")
{
	kanban_db       kdb{":memory:"};
	org_db          odb{":memory:"};
	const long long org_id  = 1;
	const long long team_id = kdb.create_team("T", org_id);
	kdb.add_member(team_id, "alice");
	const long long task_id = kdb.add_task(make_task(team_id, "Work"), "lead");
	kdb.add_assignee(task_id, "alice", "lead");
	kdb.remove_assignee(task_id, "alice", "lead");
	const auto remaining = kdb.assignees_for_task(task_id); // empty

	notify_assignee_removed(kdb, odb, task_id, team_id, org_id, "alice", "lead", remaining);

	// alice is the removed user — she should not get task_available
	const auto alice_notifs = odb.notifications_for_user("alice");
	const bool got_available = std::any_of(alice_notifs.begin(), alice_notifs.end(),
	  [](const notification_entry& n) { return n.type == "task_available"; });
	CHECK(!got_available);
}
```

- [ ] **Step 5: Build and confirm tests fail (stubs do nothing)**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_kanban_notifications 2>&1 | tail -30
```

Expected: compilation succeeds; most tests FAIL because stubs push no notifications.

---

## Task 3: `kanban_notifications` — real implementation

**Files:**
- Modify: `src/kanban/kanban_notifications.cpp`

- [ ] **Step 1: Replace stub bodies with real implementation**

```cpp
#include "kanban_notifications.hpp"

#include "org/org.hpp"
#include "widgets/live_hub.hpp"

void notify_assignee_added(kanban_db&         db,
                           org_db&            odb,
                           long long          task_id,
                           long long          team_id,
                           long long          org_id,
                           const std::string& added_user,
                           const std::string& actor)
{
	const auto        task_opt  = db.find_task(task_id);
	const auto        team_opt  = db.find_team(team_id);
	const std::string task_title = task_opt ? task_opt->title : "";
	const std::string team_name  = team_opt ? team_opt->name  : "";

	// task_assigned → added_user when someone else assigned them
	if(added_user != actor)
	{
		odb.push_notification(
		  added_user, "task_assigned",
		  make_task_assigned_payload(task_id, task_title, team_id, team_name));
		live_hub::instance().broadcast("user:" + added_user);
	}

	// task_coassignee_changed → other current assignees (excludes added_user)
	const auto all_assignees = db.assignees_for_task(task_id);
	for(const auto& u: all_assignees)
	{
		if(u == added_user)
		{
			continue;
		}
		const auto pref = odb.get_user_org_pref(u, org_id);
		if(pref.notify_coassignee_changed)
		{
			odb.push_notification(
			  u, "task_coassignee_changed",
			  make_task_coassignee_changed_payload(
			    task_id, task_title, team_id, team_name, added_user, "added"));
			live_hub::instance().broadcast("user:" + u);
		}
	}
}

void notify_assignee_removed(kanban_db&                       db,
                             org_db&                          odb,
                             long long                        task_id,
                             long long                        team_id,
                             long long                        org_id,
                             const std::string&               removed_user,
                             const std::string&               actor,
                             const std::vector<std::string>&  remaining_assignees)
{
	const auto        task_opt  = db.find_task(task_id);
	const auto        team_opt  = db.find_team(team_id);
	const std::string task_title = task_opt ? task_opt->title : "";
	const std::string team_name  = team_opt ? team_opt->name  : "";

	// task_unassigned → removed_user when a lead removed them (not self)
	if(actor != removed_user)
	{
		const auto pref = odb.get_user_org_pref(removed_user, org_id);
		if(pref.notify_task_unassigned)
		{
			odb.push_notification(
			  removed_user, "task_unassigned",
			  make_task_unassigned_payload(task_id, task_title, team_id, team_name));
			live_hub::instance().broadcast("user:" + removed_user);
		}
	}

	// task_abandoned + task_available → team members when task now has no assignees
	if(remaining_assignees.empty())
	{
		const auto members = db.members_for_team(team_id);
		for(const auto& u: members)
		{
			if(u == removed_user)
			{
				continue; // don't notify the person who left
			}
			const auto pref = odb.get_user_org_pref(u, org_id);

			// task_abandoned fires only when the removed user removed themselves
			if(actor == removed_user && pref.notify_task_abandoned)
			{
				odb.push_notification(
				  u, "task_abandoned",
				  make_task_abandoned_payload(
				    task_id, task_title, team_id, team_name, removed_user));
				live_hub::instance().broadcast("user:" + u);
			}

			// task_available fires for any removal that leaves the task unassigned
			if(pref.notify_task_available)
			{
				odb.push_notification(
				  u, "task_available",
				  make_task_available_payload(task_id, task_title, team_id, team_name));
				live_hub::instance().broadcast("user:" + u);
			}
		}
	}

	// task_coassignee_changed → remaining assignees
	for(const auto& u: remaining_assignees)
	{
		const auto pref = odb.get_user_org_pref(u, org_id);
		if(pref.notify_coassignee_changed)
		{
			odb.push_notification(
			  u, "task_coassignee_changed",
			  make_task_coassignee_changed_payload(
			    task_id, task_title, team_id, team_name, removed_user, "removed"));
			live_hub::instance().broadcast("user:" + u);
		}
	}
}
```

- [ ] **Step 2: Build and run notification tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_kanban_notifications 2>&1 | tail -20
```

Expected: all tests pass.

- [ ] **Step 3: Run all C++ tests**

```bash
cd build && ctest --output-on-failure 2>&1 | tail -10
```

Expected: all suites pass.

- [ ] **Step 4: Commit**

```bash
git add src/kanban/kanban_notifications.hpp src/kanban/kanban_notifications.cpp \
        tests/test_kanban_notifications.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(kanban): add notification helper module for assignee add/remove events

notify_assignee_added/removed dispatch task_assigned, task_available,
task_unassigned, task_abandoned, task_coassignee_changed with pref filtering.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Wire notification helpers into `task_editor_form_widget.cpp`

**Files:**
- Modify: `src/kanban/task_editor_form_widget.cpp`

Context: there are three assignee-mutation sites in the widget:
1. **"Add" button handler** (around line 455): lead adds a member via the combo — currently has an inline `push_notification` call.
2. **"Assign to me" button handler** (around line 420): non-lead self-assigns.
3. **`do_remove` lambda** (around line 366): chip ✕ button removes an assignee.

All three need to call the appropriate helper after the DB mutation. The inline `push_notification` in the "Add" handler is replaced entirely.

- [ ] **Step 1: Add `#include "kanban_notifications.hpp"` to task_editor_form_widget.cpp**

Add after the existing `#include "org/org.hpp"` line:

```cpp
#include "kanban/kanban_notifications.hpp"
```

- [ ] **Step 2: Update the "Add" button handler — replace inline notification push**

Find the block starting with `if(m_db.add_assignee(m_task_id, new_user, m_username))` (inside the `add_btn->clicked()` lambda, around line 455). Replace the entire block:

```cpp
// BEFORE (lines ~455–469):
if(m_db.add_assignee(m_task_id, new_user, m_username))
{
    if(new_user != m_username)
    {
        const auto team_opt = m_db.find_team(m_team_id);
        const auto task_opt = m_db.find_task(m_task_id);
        m_odb.push_notification(
          new_user, "task_assigned", make_task_assigned_payload(m_task_id, task_opt ? task_opt->title : "", m_team_id, team_opt ? team_opt->name : ""));
        live_hub::instance().broadcast("user:" + new_user);
    }
    live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
    (*rebuild_chips_fn)();
    m_add_member_combo->setCurrentIndex(0);
}
```

With:

```cpp
if(m_db.add_assignee(m_task_id, new_user, m_username))
{
    notify_assignee_added(
      m_db, m_odb, m_task_id, m_team_id, m_org_id, new_user, m_username);
    live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
    (*rebuild_chips_fn)();
    m_add_member_combo->setCurrentIndex(0);
}
```

- [ ] **Step 3: Update the "Assign to me" button handler**

Find the `self_btn->clicked()` lambda (around line 418). Replace:

```cpp
// BEFORE:
self_btn->clicked().connect([this, rebuild_chips_fn]() {
    if(m_db.add_assignee(m_task_id, m_username, m_username))
    {
        live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
        (*rebuild_chips_fn)();
    }
});
```

With:

```cpp
self_btn->clicked().connect([this, rebuild_chips_fn]() {
    if(m_db.add_assignee(m_task_id, m_username, m_username))
    {
        notify_assignee_added(
          m_db, m_odb, m_task_id, m_team_id, m_org_id, m_username, m_username);
        live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
        (*rebuild_chips_fn)();
    }
});
```

- [ ] **Step 4: Update the `do_remove` lambda to call `notify_assignee_removed`**

Find the `do_remove` lambda (around line 366):

```cpp
// BEFORE:
auto do_remove = [this, user, rebuild_chips_fn]() {
    m_db.remove_assignee(m_task_id, user, m_username);
    live_hub::instance().broadcast(
      "team:" + std::to_string(m_team_id));
    (*rebuild_chips_fn)();
};
```

Replace with:

```cpp
auto do_remove = [this, user, rebuild_chips_fn]() {
    m_db.remove_assignee(m_task_id, user, m_username);
    const auto remaining = m_db.assignees_for_task(m_task_id);
    notify_assignee_removed(
      m_db, m_odb, m_task_id, m_team_id, m_org_id,
      user, m_username, remaining);
    live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
    (*rebuild_chips_fn)();
};
```

- [ ] **Step 5: Build and run all tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure 2>&1 | tail -10
```

Expected: all tests pass, no compilation errors.

- [ ] **Step 6: Commit**

```bash
git add src/kanban/task_editor_form_widget.cpp
git commit -m "$(cat <<'EOF'
feat(ui): wire notification helpers into assignee chip widget

Replaces inline task_assigned push with notify_assignee_added; adds
notify_assignee_removed after chip removal.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Add four notification type rendering branches to `notifications_page.cpp`

**Files:**
- Modify: `src/pages/notifications_page.cpp`

The new branches go before the `// ── unknown (future types) ──` catch-all at line 298. Insert the four blocks right before the `else` on that line.

- [ ] **Step 1: Add the four rendering branches**

Find the comment `// ── unknown (future types) ──` and insert before it:

```cpp
		// ── task_available ────────────────────────────────────────────────────
		else if(n.type == "task_available")
		{
			const long long   task_id    = json_long(n.payload, "task_id");
			const std::string task_title = json_str(n.payload, "task_title");
			const long long   team_id    = json_long(n.payload, "team_id");
			const std::string team_name  = json_str(n.payload, "team_name");

			body->addNew<Wt::WText>("Task \"", Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WAnchor>(
			      Wt::WLink{Wt::LinkType::InternalPath,
			                "/board/" + std::to_string(team_id) + "/task/" +
			                  std::to_string(task_id) + "/edit"},
			      task_title)
			  ->setStyleClass("notif-link");
			body->addNew<Wt::WText>(
			      "\" in team " + team_name +
			        " is now unassigned \xe2\x80\x94 " + n.created_at,
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── task_unassigned ───────────────────────────────────────────────────
		else if(n.type == "task_unassigned")
		{
			const long long   task_id    = json_long(n.payload, "task_id");
			const std::string task_title = json_str(n.payload, "task_title");
			const long long   team_id    = json_long(n.payload, "team_id");
			const std::string team_name  = json_str(n.payload, "team_name");

			body->addNew<Wt::WText>("You were removed from task \"",
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WAnchor>(
			      Wt::WLink{Wt::LinkType::InternalPath,
			                "/board/" + std::to_string(team_id) + "/task/" +
			                  std::to_string(task_id) + "/edit"},
			      task_title)
			  ->setStyleClass("notif-link");
			body->addNew<Wt::WText>(
			      "\" in team " + team_name + " \xe2\x80\x94 " + n.created_at,
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── task_abandoned ────────────────────────────────────────────────────
		else if(n.type == "task_abandoned")
		{
			const long long   task_id      = json_long(n.payload, "task_id");
			const std::string task_title   = json_str(n.payload, "task_title");
			const long long   team_id      = json_long(n.payload, "team_id");
			const std::string team_name    = json_str(n.payload, "team_name");
			const std::string abandoned_by = json_str(n.payload, "abandoned_by");

			body->addNew<Wt::WText>(abandoned_by + " abandoned task \"",
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WAnchor>(
			      Wt::WLink{Wt::LinkType::InternalPath,
			                "/board/" + std::to_string(team_id) + "/task/" +
			                  std::to_string(task_id) + "/edit"},
			      task_title)
			  ->setStyleClass("notif-link");
			body->addNew<Wt::WText>(
			      "\" in team " + team_name + " \xe2\x80\x94 " + n.created_at,
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── task_coassignee_changed ───────────────────────────────────────────
		else if(n.type == "task_coassignee_changed")
		{
			const long long   task_id      = json_long(n.payload, "task_id");
			const std::string task_title   = json_str(n.payload, "task_title");
			const long long   team_id      = json_long(n.payload, "team_id");
			const std::string team_name    = json_str(n.payload, "team_name");
			const std::string changed_user = json_str(n.payload, "changed_user");
			const std::string action       = json_str(n.payload, "action");

			const std::string verb =
			  (action == "added") ? " was added to" : " was removed from";
			body->addNew<Wt::WText>(changed_user + verb + " task \"",
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WAnchor>(
			      Wt::WLink{Wt::LinkType::InternalPath,
			                "/board/" + std::to_string(team_id) + "/task/" +
			                  std::to_string(task_id) + "/edit"},
			      task_title)
			  ->setStyleClass("notif-link");
			body->addNew<Wt::WText>(
			      "\" in team " + team_name + " \xe2\x80\x94 " + n.created_at,
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -10
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add src/pages/notifications_page.cpp
git commit -m "$(cat <<'EOF'
feat(ui): render task_available/unassigned/abandoned/coassignee_changed notifications

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: User notification preferences page (`/settings`)

**Files:**
- Create: `src/pages/settings_page.hpp`
- Create: `src/pages/settings_page.cpp`
- Modify: `src/widgets/nav_bar.cpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Create `src/pages/settings_page.hpp`**

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

class settings_page: public Wt::WContainerWidget
{
public:
	settings_page(org_db& odb, const session_data& session);
};
```

- [ ] **Step 2: Create `src/pages/settings_page.cpp`**

```cpp
#include "settings_page.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WText.h>

#include "org/org.hpp"

settings_page::settings_page(org_db& odb, const session_data& session)
{
	setStyleClass("page settings-page");
	addNew<Wt::WText>("<h1>Settings</h1>", Wt::TextFormat::UnsafeXHTML);

	const auto orgs = odb.orgs_for_user(session.username);
	if(orgs.empty())
	{
		addNew<Wt::WText>(
		  "You are not a member of any organizations.", Wt::TextFormat::Plain)
		  ->setStyleClass("settings-empty");
		return;
	}

	for(const auto& org: orgs)
	{
		auto* section = addNew<Wt::WContainerWidget>();
		section->setStyleClass("settings-org-section");
		section->addNew<Wt::WText>("<h2>" + org.name + "</h2>",
		                            Wt::TextFormat::UnsafeXHTML);

		const long long org_id   = org.id;
		const auto      username = session.username;
		const auto      pref     = odb.get_user_org_pref(username, org_id);

		struct { const char* label; bool current; } toggles[] = {
		  {"New task available",    pref.notify_task_available},
		  {"Removed from task",     pref.notify_task_unassigned},
		  {"Co-assignee changed",   pref.notify_coassignee_changed},
		  {"Task abandoned",        pref.notify_task_abandoned},
		};

		auto* available_cb    = section->addNew<Wt::WCheckBox>("New task available");
		auto* unassigned_cb   = section->addNew<Wt::WCheckBox>("Removed from task");
		auto* coassignee_cb   = section->addNew<Wt::WCheckBox>("Co-assignee changed");
		auto* abandoned_cb    = section->addNew<Wt::WCheckBox>("Task abandoned");

		available_cb->setChecked(pref.notify_task_available);
		unassigned_cb->setChecked(pref.notify_task_unassigned);
		coassignee_cb->setChecked(pref.notify_coassignee_changed);
		abandoned_cb->setChecked(pref.notify_task_abandoned);

		for(auto* cb: {available_cb, unassigned_cb, coassignee_cb, abandoned_cb})
		{
			cb->setStyleClass("settings-pref-check");
		}

		available_cb->changed().connect(
		  [&odb, username, org_id, available_cb] {
			  auto p                   = odb.get_user_org_pref(username, org_id);
			  p.notify_task_available  = available_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
		unassigned_cb->changed().connect(
		  [&odb, username, org_id, unassigned_cb] {
			  auto p                    = odb.get_user_org_pref(username, org_id);
			  p.notify_task_unassigned  = unassigned_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
		coassignee_cb->changed().connect(
		  [&odb, username, org_id, coassignee_cb] {
			  auto p                       = odb.get_user_org_pref(username, org_id);
			  p.notify_coassignee_changed  = coassignee_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
		abandoned_cb->changed().connect(
		  [&odb, username, org_id, abandoned_cb] {
			  auto p                   = odb.get_user_org_pref(username, org_id);
			  p.notify_task_abandoned  = abandoned_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
	}
}
```

- [ ] **Step 3: Add "Settings" link to `src/widgets/nav_bar.cpp`**

In `nav_bar::update()`, find the `// ── Notification bell ──` comment (around line 139). Insert the Settings link immediately before it:

```cpp
	// ── Settings ──────────────────────────────────────────────────────────────
	m_auth_area->addNew<Wt::WAnchor>(
	             Wt::WLink{Wt::LinkType::InternalPath, "/settings"}, "Settings")
	  ->setStyleClass("nav-link");

	// ── Notification bell ─────────────────────────────────────────────────────
```

- [ ] **Step 4: Register the `/settings` route in `src/altinf_app.cpp`**

Add `#include "pages/settings_page.hpp"` after the existing page includes (around line 27).

In `handle_path()`, add a new branch before the `// ── Accounts ──` block (around line 520):

```cpp
	// ── Settings ──────────────────────────────────────────────────────────────
	else if(path == "/settings")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		m_content->addNew<settings_page>(*m_org_db, m_session);
	}
```

- [ ] **Step 5: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -10
```

Expected: no errors.

- [ ] **Step 6: Commit**

```bash
git add src/pages/settings_page.hpp src/pages/settings_page.cpp \
        src/widgets/nav_bar.cpp src/altinf_app.cpp
git commit -m "$(cat <<'EOF'
feat(ui): add /settings page for per-org notification preferences

Four checkboxes per org, saving immediately via org_db::set_user_org_pref.
Settings link added to nav bar for logged-in users.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Team settings page (`/board/{team_id}/settings`) and rename migration

**Files:**
- Create: `src/pages/team_settings_page.hpp`
- Create: `src/pages/team_settings_page.cpp`
- Modify: `src/altinf_app.cpp`
- Modify: `src/pages/kanban_board_page.cpp`
- Modify: `src/pages/kanban_team_page.cpp`

- [ ] **Step 1: Create `src/pages/team_settings_page.hpp`**

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"

#include <Wt/WContainerWidget.h>

class team_settings_page: public Wt::WContainerWidget
{
public:
	team_settings_page(kanban_db&          db,
	                   const session_data& session,
	                   long long           team_id);
};
```

- [ ] **Step 2: Create `src/pages/team_settings_page.cpp`**

```cpp
#include "team_settings_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "widgets/live_hub.hpp"

team_settings_page::team_settings_page(kanban_db&          db,
                                       const session_data& session,
                                       long long           team_id)
{
	setStyleClass("page team-settings-page");

	const auto team = db.find_team(team_id);
	if(!team)
	{
		addNew<Wt::WText>("Team not found.", Wt::TextFormat::Plain);
		return;
	}

	const std::string board_url = "/board/" + std::to_string(team_id);

	addNew<Wt::WText>("<h1>Team Settings \xe2\x80\x94 " + team->name + "</h1>",
	                  Wt::TextFormat::UnsafeXHTML);

	addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, board_url},
	  "\xe2\x86\x90 Back to board")
	  ->setStyleClass("editor-btn editor-btn-cancel");

	// ── Team name ──────────────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Team name</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* name_row = addNew<Wt::WContainerWidget>();
	name_row->setStyleClass("settings-field-row");

	auto* name_input = name_row->addNew<Wt::WLineEdit>();
	name_input->setText(team->name);
	name_input->setStyleClass("editor-field");

	auto* save_btn = name_row->addNew<Wt::WPushButton>("Save");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect(
	  [&db, team_id, name_input] {
		  const std::string n = name_input->text().toUTF8();
		  if(!n.empty())
		  {
			  db.rename_team(team_id, n);
			  live_hub::instance().broadcast("team:" + std::to_string(team_id));
		  }
	  });

	// ── Member permissions ─────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Member permissions</h2>", Wt::TextFormat::UnsafeXHTML);

	const auto settings = db.settings_for_team(team_id);

	auto* move_cb     = addNew<Wt::WCheckBox>(
	  "Members can move tasks between columns");
	auto* self_un_cb  = addNew<Wt::WCheckBox>(
	  "Members can self-assign unassigned tasks");
	auto* self_as_cb  = addNew<Wt::WCheckBox>(
	  "Members can self-assign already-assigned tasks");
	auto* abandon_cb  = addNew<Wt::WCheckBox>(
	  "Members can abandon tasks");

	move_cb->setChecked(settings.allow_member_move_columns);
	self_un_cb->setChecked(settings.allow_self_assign_unassigned);
	self_as_cb->setChecked(settings.allow_self_assign_assigned);
	abandon_cb->setChecked(settings.allow_abandon);

	for(auto* cb: {move_cb, self_un_cb, self_as_cb, abandon_cb})
	{
		cb->setStyleClass("settings-pref-check");
	}

	const std::string actor = session.username;

	move_cb->changed().connect(
	  [&db, team_id, actor, move_cb] {
		  auto s                       = db.settings_for_team(team_id);
		  s.allow_member_move_columns  = move_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
	self_un_cb->changed().connect(
	  [&db, team_id, actor, self_un_cb] {
		  auto s                           = db.settings_for_team(team_id);
		  s.allow_self_assign_unassigned   = self_un_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
	self_as_cb->changed().connect(
	  [&db, team_id, actor, self_as_cb] {
		  auto s                         = db.settings_for_team(team_id);
		  s.allow_self_assign_assigned   = self_as_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
	abandon_cb->changed().connect(
	  [&db, team_id, actor, abandon_cb] {
		  auto s              = db.settings_for_team(team_id);
		  s.allow_abandon     = abandon_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
}
```

- [ ] **Step 3: Add "Settings" link to the board page header**

In `src/pages/kanban_board_page.cpp`, find the existing `manage_team` check (around line 73):

```cpp
	if(caps.has_any(team_cap::manage_team))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, team_url + "/manage"},
		     "Manage Team")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
	}
```

Add a Settings link immediately after it:

```cpp
	if(caps.has_any(team_cap::manage_team))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, team_url + "/settings"},
		     "Settings")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
	}
```

- [ ] **Step 4: Register the `/board/{team_id}/settings` route in `src/altinf_app.cpp`**

Add `#include "pages/team_settings_page.hpp"` after the existing page includes.

In the `/board/{team_id}` handler, find the `else if(suffix == "/manage")` block (around line 409) and add a new branch immediately after it:

```cpp
		else if(suffix == "/settings")
		{
			if(!caps.has_any(team_cap::manage_team))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<team_settings_page>(
			  *m_kanban_db, m_session, team_id);
		}
```

- [ ] **Step 5: Remove rename from `src/pages/kanban_team_page.cpp`**

In `kanban_team_page::build_team_block()`, find the team rename block (around lines 333–348):

```cpp
	auto* name_input = hdr->addNew<Wt::WLineEdit>();
	name_input->setText(team.name);
	name_input->setStyleClass("editor-field");

	auto* rename_btn = hdr->addNew<Wt::WPushButton>("Rename");
	rename_btn->setStyleClass("editor-btn");
	rename_btn->clicked().connect(
	  [this, tid = team.id, name_input] {
		  const std::string n = name_input->text().toUTF8();
		  if(!n.empty())
		  {
			  m_kdb.rename_team(tid, n);
			  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
			  live_hub::instance().broadcast("team:" + std::to_string(tid));
		  }
	  });
```

Replace with a read-only team name heading:

```cpp
	hdr->addNew<Wt::WText>(team.name, Wt::TextFormat::Plain)
	  ->setStyleClass("kb-team-name-label");
```

- [ ] **Step 6: Build and run all tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure 2>&1 | tail -10
```

Expected: all tests pass, no compilation errors.

- [ ] **Step 7: Commit**

```bash
git add src/pages/team_settings_page.hpp src/pages/team_settings_page.cpp \
        src/altinf_app.cpp src/pages/kanban_board_page.cpp \
        src/pages/kanban_team_page.cpp
git commit -m "$(cat <<'EOF'
feat(ui): add /board/{team_id}/settings page; move team rename there

Team rename removed from org manage page. New settings page handles
rename + four member-permission toggles. Settings link in board header
for leads.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review Checklist

### Spec coverage

| Requirement | Task |
|---|---|
| `task_available` notification payload helper | Task 1 |
| `task_unassigned` notification payload helper | Task 1 |
| `task_abandoned` notification payload helper | Task 1 |
| `task_coassignee_changed` notification payload helper | Task 1 |
| `notify_assignee_added` helper (task_assigned + coassignee_changed) | Tasks 2–3 |
| `notify_assignee_removed` helper (unassigned + abandoned + available + coassignee) | Tasks 2–3 |
| `user_org_pref` opt-out filtering in both helpers | Tasks 2–3 |
| No notifications from `maybe_clear_assignees_for_done` | Unchanged — that path already has no notification calls |
| Replace inline task_assigned push in "Add" button handler | Task 4 |
| Add `notify_assignee_removed` to chip remove handler | Task 4 |
| Add `notify_assignee_added` to self-assign button handler | Task 4 |
| Render `task_available` in notifications_page | Task 5 |
| Render `task_unassigned` in notifications_page | Task 5 |
| Render `task_abandoned` in notifications_page | Task 5 |
| Render `task_coassignee_changed` in notifications_page | Task 5 |
| `/settings` page with per-org notification pref toggles | Task 6 |
| "Settings" link in nav bar | Task 6 |
| `/board/{team_id}/settings` page with rename + permission toggles | Task 7 |
| "Settings" link in board header for leads | Task 7 |
| Team rename removed from org manage page | Task 7 |

### Type consistency check

- `notify_assignee_added` signature in `.hpp`, stub `.cpp`, real `.cpp`, and call sites in `task_editor_form_widget.cpp` all match: `(kanban_db&, org_db&, long long, long long, long long, const std::string&, const std::string&)` ✓
- `notify_assignee_removed` signature consistent across all sites: adds `const std::vector<std::string>&` ✓
- Payload helper return types all `std::string`, parameter types consistent with `make_task_assigned_payload` ✓
- `settings_page` constructor `(org_db&, const session_data&)` matches call in `altinf_app.cpp` ✓
- `team_settings_page` constructor `(kanban_db&, const session_data&, long long)` matches call in `altinf_app.cpp` ✓

### Placeholder scan

No TBD, TODO, or "similar to Task N" references. All code blocks are complete. ✓
