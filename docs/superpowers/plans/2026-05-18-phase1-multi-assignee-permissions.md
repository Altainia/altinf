# Phase 1: Multi-Assignee, Team Settings, and Task Permissions

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single-assignee task model with multi-assignee, add configurable team permission settings, restrict task-detail editing to leads, and allow org-member read-only board access.

**Architecture:** The `assigned_to TEXT` column is abandoned in place (existing DBs ignore it via Wt::Dbo's explicit field mapping); a new `task_assignee` junction table owns the relationship. A `team_settings` table stores per-team or org-wide-default capability toggles. Capability enforcement splits across two layers: the `team_cap` bit-flags (role-based, compile-time) and `team_settings_entry` (configurable, fetched per request). The page layer merges both before constructing widgets.

**Tech Stack:** C++17, Wt 4.x (Wt::Dbo ORM on SQLite), Catch2 v3 for unit tests. Build: `cmake --build build --parallel $(nproc)`. C++ tests: `cd build && ctest --output-on-failure`.

---

## File Map

| File | Change |
|------|--------|
| `src/kanban/kanban.hpp` | Add `task_assignee_record`; remove `assigned_to` from `kanban_task_record::persist()`; replace `assigned_to: string` with `assignees: vector<string>` in `kanban_task_entry`; add `team_settings_record`, `team_settings_entry`, `team_settings_event_record` |
| `src/kanban/kanban_db.hpp` | Remove `self_assign()`; add `add_assignee()`, `remove_assignee()`, `assignees_for_task()`; add `settings_for_team()`, `set_team_settings()`; add private `maybe_clear_assignees_for_done()` |
| `src/kanban/kanban_db.cpp` | Map + migrate `task_assignee`; implement all new DB functions; update `to_entry()`, `find_task()`, `tasks_for_team()`, `archived_tasks_for_team()`; update `update_task()` + `update_task_status()` to call `maybe_clear_assignees_for_done()` |
| `src/kanban/team_cap.hpp` | Add `edit_task_details = flags::from_value(1u << 9)`; add it to `team_lead_caps` and `org_lead_caps` |
| `src/org/org.hpp` | Add `user_org_pref_record`, `user_org_pref_entry` |
| `src/org/org_db.hpp` | Add `get_user_org_pref()`, `set_user_org_pref()` |
| `src/org/org_db.cpp` | Map + migrate `user_org_pref`; implement both functions |
| `src/kanban/task_editor_form_widget.hpp` | Add `team_settings_entry settings` constructor param; add `m_settings` member; add `m_status_vals_used` member; replace `m_assignee_edit/display/values` with multi-assignee members |
| `src/kanban/task_editor_form_widget.cpp` | Refactor title/desc/dates to use `edit_task_details` cap; filter "Done" from status dropdown for non-leads; replace assignee combo with chip-list widget (immediate add/remove + notification on add); update `save()` to omit assignee logic |
| `src/kanban/kanban_board_widget.hpp` | Replace `bool can_edit` with `bool can_move_columns, bool can_move_done` |
| `src/kanban/kanban_board_widget.cpp` | Serialize `assignees` JSON array; pass move-capability flags into JS |
| `src/pages/kanban_board_page.hpp` | Add `team_settings_entry settings` constructor param |
| `src/pages/kanban_board_page.cpp` | Compute `can_move_columns/can_move_done` from caps + settings; validate `on_move` callback server-side |
| `src/pages/kanban_task_editor_page.hpp` | Add `team_settings_entry settings` constructor param |
| `src/pages/kanban_task_editor_page.cpp` | Pass `settings` through to `task_editor_form_widget` |
| `src/kanban/task_popup_widget.hpp` | Add `team_settings_entry settings` constructor param |
| `src/kanban/task_popup_widget.cpp` | Pass `settings` through to `task_editor_form_widget` |
| `src/altinf_app.cpp` | After `resolve_team_caps()`, call `settings_for_team()`; pass settings to all board/editor constructors |
| `tests/test_kanban_db.cpp` | Add tests for `add_assignee`, `remove_assignee`, `assignees_for_task`, Done-column clearing, team settings CRUD + audit, `tasks_for_team` populates assignees |
| `tests/test_org_db.cpp` | Add tests for `get_user_org_pref` defaults and `set_user_org_pref` upsert |
| `tests/test_team_cap.cpp` | Add test: `edit_task_details` not in member caps, is in lead caps |

---

## Task 1: Multi-assignee — failing tests

**Files:**
- Test: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Append failing tests to test_kanban_db.cpp**

Add after the existing self_assign tests:

```cpp
// ---- multi-assignee ----

TEST_CASE("kanban_db - add_assignee: success on unassigned task")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");

    CHECK(db.add_assignee(id, "alice", "alice"));
    const auto a = db.assignees_for_task(id);
    REQUIRE(a.size() == 1);
    CHECK(a[0] == "alice");
}

TEST_CASE("kanban_db - add_assignee: two users")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    db.add_member(tid, "bob");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");

    CHECK(db.add_assignee(id, "alice", "alice"));
    CHECK(db.add_assignee(id, "bob",   "alice"));
    const auto a = db.assignees_for_task(id);
    CHECK(a.size() == 2);
}

TEST_CASE("kanban_db - add_assignee: idempotent")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");

    db.add_assignee(id, "alice", "alice");
    CHECK(!db.add_assignee(id, "alice", "alice")); // already assigned → false
    CHECK(db.assignees_for_task(id).size() == 1);
}

TEST_CASE("kanban_db - add_assignee: rejects done task")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work", "done"), "alice");

    CHECK(!db.add_assignee(id, "alice", "alice"));
}

TEST_CASE("kanban_db - remove_assignee: success")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    db.add_member(tid, "bob");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.add_assignee(id, "alice", "alice");
    db.add_assignee(id, "bob",   "alice");

    CHECK(db.remove_assignee(id, "bob", "alice"));
    const auto a = db.assignees_for_task(id);
    REQUIRE(a.size() == 1);
    CHECK(a[0] == "alice");
}

TEST_CASE("kanban_db - remove_assignee: false when not assigned")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id = db.add_task(make_task(tid, "Work"), "alice");

    CHECK(!db.remove_assignee(id, "alice", "alice"));
}

TEST_CASE("kanban_db - tasks_for_team populates assignees")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    db.add_member(tid, "bob");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.add_assignee(id, "alice", "alice");
    db.add_assignee(id, "bob",   "alice");

    const auto tasks = db.tasks_for_team(tid);
    REQUIRE(!tasks.empty());
    CHECK(tasks[0].assignees.size() == 2);
}

TEST_CASE("kanban_db - find_task populates assignees")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.add_assignee(id, "alice", "alice");

    const auto t = db.find_task(id);
    REQUIRE(t.has_value());
    REQUIRE(t->assignees.size() == 1);
    CHECK(t->assignees[0] == "alice");
}
```

- [ ] **Step 2: Confirm tests fail to compile (self_assign/add_assignee not yet declared)**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -20
```

Expected: compilation errors about `add_assignee`, `remove_assignee`, `assignees_for_task` not being members of `kanban_db`.

---

## Task 2: Multi-assignee — data model (kanban.hpp + kanban_db.hpp)

**Files:**
- Modify: `src/kanban/kanban.hpp`
- Modify: `src/kanban/kanban_db.hpp`

- [ ] **Step 1: Add task_assignee_record to kanban.hpp**

After `kanban_task_record`, add:

```cpp
struct task_assignee_record
{
    long long   task_id{0};
    std::string username;

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, task_id,  "task_id");
        Wt::Dbo::field(a, username, "username");
    }
};
```

- [ ] **Step 2: Remove assigned_to from kanban_task_record::persist() and kanban_task_entry**

In `kanban_task_record::persist()`, delete the line:
```cpp
Wt::Dbo::field(a, assigned_to, "assigned_to");
```
Leave the `assigned_to` data member declared (so the struct compiles) but it will no longer be mapped to the DB. This means old DB files keep the column but Wt::Dbo ignores it; new DB files created from `createTables()` won't have it.

In `kanban_task_entry`, replace:
```cpp
std::string assigned_to;
```
with:
```cpp
std::vector<std::string> assignees;
```

Add `#include <vector>` at the top of `kanban.hpp` if not already present.

- [ ] **Step 3: Update kanban_db.hpp — remove self_assign, add new functions**

Remove:
```cpp
bool self_assign(long long task_id, const std::string& username);
```

Add in its place:
```cpp
// Returns false if the task is in "done", is archived, or username is already assigned.
bool add_assignee(long long task_id, const std::string& username, const std::string& actor);
// Returns false if username is not currently an assignee or task is archived.
bool remove_assignee(long long task_id, const std::string& username, const std::string& actor);
std::vector<std::string> assignees_for_task(long long task_id);
```

Add a private helper declaration:
```cpp
private:
    // ...existing private members...
    void maybe_clear_assignees_for_done(long long task_id,
                                         const std::string& new_status,
                                         const std::string& actor);
```

- [ ] **Step 4: Build to check for compile errors**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -40
```

Expected: errors about `assigned_to` references in `.cpp` files (task_editor_form_widget.cpp, kanban_db.cpp). These will be fixed in subsequent tasks. The header changes themselves should be clean.

---

## Task 3: Multi-assignee — DB implementation (kanban_db.cpp)

**Files:**
- Modify: `src/kanban/kanban_db.cpp`

- [ ] **Step 1: Map task_assignee_record in the constructor**

In `kanban_db::kanban_db()`, add after the existing `mapClass` calls:

```cpp
m_dbo.mapClass<task_assignee_record>("task_assignee");
```

- [ ] **Step 2: Add migration for task_assignee table**

After the existing migrations in the constructor, add:

```cpp
migrate(
  "CREATE TABLE IF NOT EXISTS task_assignee ("
  "id integer primary key autoincrement,"
  " version integer not null default 0,"
  " task_id integer not null default 0,"
  " username text not null default '')");
```

- [ ] **Step 3: Implement assignees_for_task()**

```cpp
std::vector<std::string> kanban_db::assignees_for_task(long long task_id)
{
    Wt::Dbo::Transaction     t{m_dbo};
    const auto               rows =
      m_dbo.find<task_assignee_record>()
        .where("task_id = ?")
        .bind(task_id)
        .orderBy("username")
        .resultList();
    std::vector<std::string> out;
    for(const auto& r: rows)
    {
        out.push_back(r->username);
    }
    return out;
}
```

- [ ] **Step 4: Implement add_assignee()**

```cpp
bool kanban_db::add_assignee(long long          task_id,
                              const std::string& username,
                              const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           tasks =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(task_id).resultList();
    if(tasks.empty())
    {
        return false;
    }
    const Wt::Dbo::ptr<kanban_task_record> task_ptr = *tasks.begin();
    if(task_ptr->status == "done" || task_ptr->is_archived)
    {
        return false;
    }

    const auto existing =
      m_dbo.find<task_assignee_record>()
        .where("task_id = ? AND username = ?")
        .bind(task_id)
        .bind(username)
        .resultList();
    if(!existing.empty())
    {
        return false; // already assigned
    }

    auto r          = m_dbo.add(std::make_unique<task_assignee_record>());
    r.modify()->task_id  = task_id;
    r.modify()->username = username;

    record_event(task_id, actor, "updated",
                 {{"assignees", {}, username + " added"}});
    return true;
}
```

- [ ] **Step 5: Implement remove_assignee()**

```cpp
bool kanban_db::remove_assignee(long long          task_id,
                                 const std::string& username,
                                 const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           tasks =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(task_id).resultList();
    if(tasks.empty())
    {
        return false;
    }
    if((*tasks.begin())->is_archived)
    {
        return false;
    }

    const auto rows =
      m_dbo.find<task_assignee_record>()
        .where("task_id = ? AND username = ?")
        .bind(task_id)
        .bind(username)
        .resultList();
    if(rows.empty())
    {
        return false;
    }

    (*rows.begin()).remove();
    record_event(task_id, actor, "updated",
                 {{"assignees", username + " removed", {}}});
    return true;
}
```

- [ ] **Step 6: Add maybe_clear_assignees_for_done() private helper**

```cpp
void kanban_db::maybe_clear_assignees_for_done(long long          task_id,
                                                const std::string& new_status,
                                                const std::string& actor)
{
    if(new_status != "done")
    {
        return;
    }
    const auto rows =
      m_dbo.find<task_assignee_record>()
        .where("task_id = ?")
        .bind(task_id)
        .resultList();
    if(rows.empty())
    {
        return;
    }
    std::string removed_names;
    for(const auto& r: rows)
    {
        if(!removed_names.empty())
        {
            removed_names += ", ";
        }
        removed_names += r->username;
    }
    for(const auto& r: rows)
    {
        Wt::Dbo::ptr<task_assignee_record> row = r;
        row.remove();
    }
    record_event(task_id, actor, "updated",
                 {{"assignees", removed_names, "(done — assignees cleared)"}});
}
```

- [ ] **Step 7: Call maybe_clear_assignees_for_done from update_task_status()**

Find `kanban_db::update_task_status()`. After the existing status-change detection and `record_event` call, add — still inside the transaction:

```cpp
maybe_clear_assignees_for_done(id, status, actor);
```

The full function should look like:
```cpp
void kanban_db::update_task_status(long long          id,
                                    const std::string& status,
                                    int                sort_order,
                                    const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
    if(results.empty())
    {
        return;
    }
    Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
    const std::string                old_status = p->status;
    p.modify()->status     = status;
    p.modify()->sort_order = sort_order;
    if(old_status != status)
    {
        record_event(id, actor, "updated", {{"status", old_status, status}});
    }
    maybe_clear_assignees_for_done(id, status, actor);
}
```

- [ ] **Step 8: Call maybe_clear_assignees_for_done from update_task()**

In `kanban_db::update_task()`, find where field changes are recorded and the status field is compared. After the existing `record_event` call (at the end of the function, still inside the transaction), add:

```cpp
maybe_clear_assignees_for_done(e.id, e.status, actor);
```

- [ ] **Step 9: Update to_entry() for kanban_task — remove assigned_to**

The static `to_entry(const Wt::Dbo::ptr<kanban_task_record>& p)` currently sets `e.assigned_to`. Remove that line. The `assignees` field defaults to an empty vector and is populated by callers.

- [ ] **Step 10: Update find_task() to populate assignees**

```cpp
std::optional<kanban_task_entry> kanban_db::find_task(long long id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
    if(results.empty())
    {
        return std::nullopt;
    }
    auto entry      = to_entry(*results.begin());
    entry.assignees = assignees_for_task(id);  // NOTE: nested txn — ok in Wt::Dbo
    return entry;
}
```

Wait — `assignees_for_task` opens its own transaction. In Wt::Dbo with SQLite, nested transactions use the same connection and are fine as long as the outer transaction is still open and you're not flushing. Instead, inline the query:

```cpp
std::optional<kanban_task_entry> kanban_db::find_task(long long id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
    if(results.empty())
    {
        return std::nullopt;
    }
    auto entry = to_entry(*results.begin());
    const auto arows =
      m_dbo.find<task_assignee_record>()
        .where("task_id = ?")
        .bind(id)
        .orderBy("username")
        .resultList();
    for(const auto& r: arows)
    {
        entry.assignees.push_back(r->username);
    }
    return entry;
}
```

- [ ] **Step 11: Update tasks_for_team() to populate assignees**

```cpp
std::vector<kanban_task_entry> kanban_db::tasks_for_team(long long team_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>()
        .where("team_id = ? AND is_archived = 0")
        .bind(team_id)
        .orderBy("sort_order, id")
        .resultList();
    std::vector<kanban_task_entry> out;
    for(const auto& p: results)
    {
        auto entry = to_entry(p);
        const auto arows =
          m_dbo.find<task_assignee_record>()
            .where("task_id = ?")
            .bind(p.id())
            .orderBy("username")
            .resultList();
        for(const auto& r: arows)
        {
            entry.assignees.push_back(r->username);
        }
        out.push_back(std::move(entry));
    }
    return out;
}
```

- [ ] **Step 12: Apply the same assignee population to archived_tasks_for_team()**

Same pattern as Step 11 but for the `is_archived = 1` query.

- [ ] **Step 13: Remove self_assign() implementation**

Delete the entire `kanban_db::self_assign()` function body from `kanban_db.cpp`.

Update any existing test that calls `self_assign()` — replace with `add_assignee()`:

In `tests/test_kanban_db.cpp`, find the `self_assign` tests and update them:

```cpp
TEST_CASE("kanban_db - add_assignee records history event")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.add_assignee(id, "alice", "alice");

    const auto hist = db.history_for_task(id);
    const bool has_event =
      std::any_of(hist.begin(), hist.end(), [](const task_event_entry& e) {
          return e.event_type == "updated" &&
                 std::any_of(e.changes.begin(), e.changes.end(),
                             [](const task_field_change_entry& c) {
                                 return c.field_name == "assignees";
                             });
      });
    CHECK(has_event);
}

TEST_CASE("kanban_db - add_assignee: fails on archived task")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.archive_task(id, "alice");

    CHECK(!db.add_assignee(id, "alice", "alice"));
}
```

- [ ] **Step 14: Build and run tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_kanban_db
```

Expected: all kanban_db tests pass. Fix any compilation errors in the UI files that touch `assigned_to` — for now, replace `assigned_to` references in the form widget with empty string or `assignees` as needed to keep it compiling (these get properly refactored in Task 9).

- [ ] **Step 15: Commit**

```bash
git add src/kanban/kanban.hpp src/kanban/kanban_db.hpp src/kanban/kanban_db.cpp tests/test_kanban_db.cpp
git commit -m "$(cat <<'EOF'
feat(kanban): replace single assigned_to with multi-assignee junction table

Removes self_assign(); adds add_assignee/remove_assignee/assignees_for_task.
Moving a task to "done" now clears all assignees automatically.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Done-column test coverage

**Files:**
- Test: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Add Done-column assignee-clearing tests**

```cpp
TEST_CASE("kanban_db - update_task_status to done clears assignees")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    db.add_member(tid, "bob");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.add_assignee(id, "alice", "alice");
    db.add_assignee(id, "bob",   "alice");

    db.update_task_status(id, "done", 0, "alice");
    CHECK(db.assignees_for_task(id).empty());
}

TEST_CASE("kanban_db - update_task_status to done records assignee-cleared event")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.add_assignee(id, "alice", "alice");

    db.update_task_status(id, "done", 0, "alice");

    const auto hist = db.history_for_task(id);
    const bool has_clear =
      std::any_of(hist.begin(), hist.end(), [](const task_event_entry& e) {
          return std::any_of(e.changes.begin(), e.changes.end(),
                             [](const task_field_change_entry& c) {
                                 return c.field_name == "assignees" &&
                                        c.new_value.find("done") != std::string::npos;
                             });
      });
    CHECK(has_clear);
}

TEST_CASE("kanban_db - update_task_status non-done does not clear assignees")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work"), "alice");
    db.add_assignee(id, "alice", "alice");

    db.update_task_status(id, "in_progress", 0, "alice");
    CHECK(db.assignees_for_task(id).size() == 1);
}

TEST_CASE("kanban_db - add_assignee rejects task already in done status")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    const long long id = db.add_task(make_task(tid, "Work", "done"), "alice");

    CHECK(!db.add_assignee(id, "alice", "alice"));
}
```

- [ ] **Step 2: Run tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_kanban_db
```

Expected: all new tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/test_kanban_db.cpp
git commit -m "$(cat <<'EOF'
test(kanban): verify Done-column clears assignees in all paths

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Team settings — failing tests

**Files:**
- Test: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Append team-settings tests**

```cpp
// ---- team settings ----

TEST_CASE("kanban_db - settings_for_team returns all-true defaults when no row exists")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 42);

    const auto s = db.settings_for_team(tid);
    CHECK(s.allow_member_move_columns);
    CHECK(s.allow_self_assign_unassigned);
    CHECK(s.allow_self_assign_assigned);
    CHECK(s.allow_abandon);
}

TEST_CASE("kanban_db - set_team_settings persists team-specific override")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 42);

    team_settings_entry s;
    s.org_id                      = 42;
    s.team_id                     = tid;
    s.allow_member_move_columns   = false;
    s.allow_self_assign_unassigned = true;
    s.allow_self_assign_assigned  = true;
    s.allow_abandon               = true;
    db.set_team_settings(s, "lead");

    const auto back = db.settings_for_team(tid);
    CHECK(!back.allow_member_move_columns);
    CHECK(back.allow_self_assign_unassigned);
}

TEST_CASE("kanban_db - org-wide default applies when no team row")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 42);

    team_settings_entry org_default;
    org_default.org_id                    = 42;
    org_default.team_id                   = 0; // 0 = org-wide
    org_default.allow_member_move_columns = false;
    org_default.allow_self_assign_unassigned = true;
    org_default.allow_self_assign_assigned   = true;
    org_default.allow_abandon                = true;
    db.set_team_settings(org_default, "lead");

    const auto s = db.settings_for_team(tid); // no team-specific row
    CHECK(!s.allow_member_move_columns);
}

TEST_CASE("kanban_db - team-specific row overrides org default")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 42);

    team_settings_entry org_default;
    org_default.org_id                    = 42;
    org_default.team_id                   = 0;
    org_default.allow_member_move_columns = false;
    org_default.allow_self_assign_unassigned = false;
    org_default.allow_self_assign_assigned   = false;
    org_default.allow_abandon                = false;
    db.set_team_settings(org_default, "lead");

    team_settings_entry team_override;
    team_override.org_id                    = 42;
    team_override.team_id                   = tid;
    team_override.allow_member_move_columns = true;
    team_override.allow_self_assign_unassigned = true;
    team_override.allow_self_assign_assigned   = true;
    team_override.allow_abandon                = true;
    db.set_team_settings(team_override, "lead");

    const auto s = db.settings_for_team(tid);
    CHECK(s.allow_member_move_columns);
    CHECK(s.allow_abandon);
}

TEST_CASE("kanban_db - set_team_settings records audit event")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 42);

    team_settings_entry s;
    s.org_id                    = 42;
    s.team_id                   = tid;
    s.allow_member_move_columns = false;
    s.allow_self_assign_unassigned = true;
    s.allow_self_assign_assigned   = true;
    s.allow_abandon                = true;
    db.set_team_settings(s, "lead");

    const auto events = db.settings_events_for_team(tid);
    REQUIRE(!events.empty());
    CHECK(events[0].actor == "lead");
    const bool found_move =
      std::any_of(events.begin(), events.end(), [](const team_settings_event_entry& e) {
          return e.field_name == "allow_member_move_columns" &&
                 e.new_value == "0";
      });
    CHECK(found_move);
}
```

- [ ] **Step 2: Verify fails to compile**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -20
```

Expected: errors about `team_settings_entry`, `settings_for_team`, `set_team_settings`, `settings_events_for_team` not found.

---

## Task 6: Team settings — data model + DB

**Files:**
- Modify: `src/kanban/kanban.hpp`
- Modify: `src/kanban/kanban_db.hpp`
- Modify: `src/kanban/kanban_db.cpp`

- [ ] **Step 1: Add team settings structs to kanban.hpp**

```cpp
struct team_settings_record
{
    long long org_id{0};
    long long team_id{0}; // 0 = org-wide default
    int allow_member_move_columns{1};
    int allow_self_assign_unassigned{1};
    int allow_self_assign_assigned{1};
    int allow_abandon{1};

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, org_id,                      "org_id");
        Wt::Dbo::field(a, team_id,                     "team_id");
        Wt::Dbo::field(a, allow_member_move_columns,   "allow_member_move_columns");
        Wt::Dbo::field(a, allow_self_assign_unassigned,"allow_self_assign_unassigned");
        Wt::Dbo::field(a, allow_self_assign_assigned,  "allow_self_assign_assigned");
        Wt::Dbo::field(a, allow_abandon,               "allow_abandon");
    }
};

struct team_settings_entry
{
    long long org_id{0};
    long long team_id{0};
    bool allow_member_move_columns{true};
    bool allow_self_assign_unassigned{true};
    bool allow_self_assign_assigned{true};
    bool allow_abandon{true};
};

struct team_settings_event_record
{
    long long   org_id{0};
    long long   team_id{0};
    std::string actor;
    std::string occurred_at;
    std::string field_name;
    std::string old_value;
    std::string new_value;

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, org_id,       "org_id");
        Wt::Dbo::field(a, team_id,      "team_id");
        Wt::Dbo::field(a, actor,        "actor");
        Wt::Dbo::field(a, occurred_at,  "occurred_at");
        Wt::Dbo::field(a, field_name,   "field_name");
        Wt::Dbo::field(a, old_value,    "old_value");
        Wt::Dbo::field(a, new_value,    "new_value");
    }
};

struct team_settings_event_entry
{
    long long   id{0};
    long long   org_id{0};
    long long   team_id{0};
    std::string actor;
    std::string occurred_at;
    std::string field_name;
    std::string old_value;
    std::string new_value;
};
```

- [ ] **Step 2: Add function signatures to kanban_db.hpp**

In the public section, under the "Members" group:

```cpp
// Team settings
team_settings_entry              settings_for_team(long long team_id);
void                             set_team_settings(const team_settings_entry& s,
                                                   const std::string& actor);
std::vector<team_settings_event_entry> settings_events_for_team(long long team_id);
```

- [ ] **Step 3: Map and migrate in kanban_db.cpp constructor**

Add after the `task_assignee_record` mapClass:

```cpp
m_dbo.mapClass<team_settings_record>("team_settings");
m_dbo.mapClass<team_settings_event_record>("team_settings_event");
```

Add migrations:

```cpp
migrate(
  "CREATE TABLE IF NOT EXISTS team_settings ("
  "id integer primary key autoincrement,"
  " version integer not null default 0,"
  " org_id integer not null default 0,"
  " team_id integer not null default 0,"
  " allow_member_move_columns integer not null default 1,"
  " allow_self_assign_unassigned integer not null default 1,"
  " allow_self_assign_assigned integer not null default 1,"
  " allow_abandon integer not null default 1)");

migrate(
  "CREATE TABLE IF NOT EXISTS team_settings_event ("
  "id integer primary key autoincrement,"
  " version integer not null default 0,"
  " org_id integer not null default 0,"
  " team_id integer not null default 0,"
  " actor text not null default '',"
  " occurred_at text not null default '',"
  " field_name text not null default '',"
  " old_value text not null default '',"
  " new_value text not null default '')");
```

- [ ] **Step 4: Implement settings_for_team()**

```cpp
team_settings_entry kanban_db::settings_for_team(long long team_id)
{
    Wt::Dbo::Transaction t{m_dbo};

    // Try team-specific row first.
    const auto team_rows =
      m_dbo.find<team_settings_record>()
        .where("team_id = ?")
        .bind(team_id)
        .resultList();
    if(!team_rows.empty())
    {
        const auto& r = *team_rows.begin();
        return {.org_id                      = r->org_id,
                .team_id                     = r->team_id,
                .allow_member_move_columns   = r->allow_member_move_columns != 0,
                .allow_self_assign_unassigned = r->allow_self_assign_unassigned != 0,
                .allow_self_assign_assigned  = r->allow_self_assign_assigned != 0,
                .allow_abandon               = r->allow_abandon != 0};
    }

    // Find the team's org_id for the org-wide default lookup.
    long long org_id = 0;
    const auto teams =
      m_dbo.find<team_record>().where("id = ?").bind(team_id).resultList();
    if(!teams.empty())
    {
        org_id = (*teams.begin())->org_id;
    }

    // Try org-wide default (team_id = 0).
    const auto org_rows =
      m_dbo.find<team_settings_record>()
        .where("org_id = ? AND team_id = 0")
        .bind(org_id)
        .resultList();
    if(!org_rows.empty())
    {
        const auto& r = *org_rows.begin();
        return {.org_id                      = r->org_id,
                .team_id                     = 0,
                .allow_member_move_columns   = r->allow_member_move_columns != 0,
                .allow_self_assign_unassigned = r->allow_self_assign_unassigned != 0,
                .allow_self_assign_assigned  = r->allow_self_assign_assigned != 0,
                .allow_abandon               = r->allow_abandon != 0};
    }

    // Hard-coded defaults (all true).
    return {.org_id = org_id, .team_id = 0};
}
```

- [ ] **Step 5: Implement set_team_settings()**

```cpp
void kanban_db::set_team_settings(const team_settings_entry& s,
                                   const std::string&         actor)
{
    Wt::Dbo::Transaction t{m_dbo};

    const std::string now =
      Wt::WDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toUTF8();

    // Find existing row.
    const auto rows =
      m_dbo.find<team_settings_record>()
        .where("org_id = ? AND team_id = ?")
        .bind(s.org_id)
        .bind(s.team_id)
        .resultList();

    auto record_change = [&](const std::string& field,
                             const std::string& old_v,
                             const std::string& new_v)
    {
        if(old_v == new_v)
        {
            return;
        }
        auto ev           = m_dbo.add(std::make_unique<team_settings_event_record>());
        ev.modify()->org_id      = s.org_id;
        ev.modify()->team_id     = s.team_id;
        ev.modify()->actor       = actor;
        ev.modify()->occurred_at = now;
        ev.modify()->field_name  = field;
        ev.modify()->old_value   = old_v;
        ev.modify()->new_value   = new_v;
    };

    auto to_s = [](bool b) { return b ? "1" : "0"; };

    if(rows.empty())
    {
        // New row — record changes from defaults (all true).
        record_change("allow_member_move_columns",    "1", to_s(s.allow_member_move_columns));
        record_change("allow_self_assign_unassigned", "1", to_s(s.allow_self_assign_unassigned));
        record_change("allow_self_assign_assigned",   "1", to_s(s.allow_self_assign_assigned));
        record_change("allow_abandon",                "1", to_s(s.allow_abandon));

        auto r = m_dbo.add(std::make_unique<team_settings_record>());
        r.modify()->org_id                      = s.org_id;
        r.modify()->team_id                     = s.team_id;
        r.modify()->allow_member_move_columns   = s.allow_member_move_columns ? 1 : 0;
        r.modify()->allow_self_assign_unassigned = s.allow_self_assign_unassigned ? 1 : 0;
        r.modify()->allow_self_assign_assigned  = s.allow_self_assign_assigned ? 1 : 0;
        r.modify()->allow_abandon               = s.allow_abandon ? 1 : 0;
    }
    else
    {
        Wt::Dbo::ptr<team_settings_record> r = *rows.begin();
        record_change("allow_member_move_columns",    to_s(r->allow_member_move_columns != 0),   to_s(s.allow_member_move_columns));
        record_change("allow_self_assign_unassigned", to_s(r->allow_self_assign_unassigned != 0), to_s(s.allow_self_assign_unassigned));
        record_change("allow_self_assign_assigned",   to_s(r->allow_self_assign_assigned != 0),  to_s(s.allow_self_assign_assigned));
        record_change("allow_abandon",                to_s(r->allow_abandon != 0),               to_s(s.allow_abandon));

        r.modify()->allow_member_move_columns   = s.allow_member_move_columns ? 1 : 0;
        r.modify()->allow_self_assign_unassigned = s.allow_self_assign_unassigned ? 1 : 0;
        r.modify()->allow_self_assign_assigned  = s.allow_self_assign_assigned ? 1 : 0;
        r.modify()->allow_abandon               = s.allow_abandon ? 1 : 0;
    }
}
```

- [ ] **Step 6: Implement settings_events_for_team()**

```cpp
std::vector<team_settings_event_entry>
kanban_db::settings_events_for_team(long long team_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           rows =
      m_dbo.find<team_settings_event_record>()
        .where("team_id = ?")
        .bind(team_id)
        .orderBy("id DESC")
        .resultList();
    std::vector<team_settings_event_entry> out;
    for(const auto& r: rows)
    {
        out.push_back({.id          = r.id(),
                       .org_id      = r->org_id,
                       .team_id     = r->team_id,
                       .actor       = r->actor,
                       .occurred_at = r->occurred_at,
                       .field_name  = r->field_name,
                       .old_value   = r->old_value,
                       .new_value   = r->new_value});
    }
    return out;
}
```

- [ ] **Step 7: Build and run tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_kanban_db
```

Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/kanban/kanban.hpp src/kanban/kanban_db.hpp src/kanban/kanban_db.cpp tests/test_kanban_db.cpp
git commit -m "$(cat <<'EOF'
feat(kanban): add configurable team settings with org-wide defaults and per-team overrides

Settings are audited; all default to true. Lookups fall back to org default then
hard-coded defaults when no row exists.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: User org prefs scaffold

**Files:**
- Test: `tests/test_org_db.cpp`
- Modify: `src/org/org.hpp`
- Modify: `src/org/org_db.hpp`
- Modify: `src/org/org_db.cpp`

- [ ] **Step 1: Write failing tests**

Append to `tests/test_org_db.cpp`:

```cpp
// ---- user_org_pref ----

TEST_CASE("org_db - get_user_org_pref returns defaults when no row")
{
    org_db db{":memory:"};
    const auto p = db.get_user_org_pref("alice", 1);
    CHECK(p.notify_task_available);
    CHECK(p.notify_task_unassigned);
    CHECK(p.notify_coassignee_changed);
    CHECK(p.notify_task_abandoned);
}

TEST_CASE("org_db - set_user_org_pref upserts and round-trips")
{
    org_db           db{":memory:"};
    user_org_pref_entry p;
    p.username                 = "alice";
    p.org_id                   = 1;
    p.notify_task_available    = false;
    p.notify_task_unassigned   = true;
    p.notify_coassignee_changed = true;
    p.notify_task_abandoned    = false;
    db.set_user_org_pref(p);

    const auto back = db.get_user_org_pref("alice", 1);
    CHECK(!back.notify_task_available);
    CHECK(back.notify_task_unassigned);
    CHECK(!back.notify_task_abandoned);
}

TEST_CASE("org_db - set_user_org_pref updates existing row")
{
    org_db           db{":memory:"};
    user_org_pref_entry p;
    p.username = "alice"; p.org_id = 1;
    p.notify_task_available = false;
    p.notify_task_unassigned = true;
    p.notify_coassignee_changed = true;
    p.notify_task_abandoned = true;
    db.set_user_org_pref(p);

    p.notify_task_available = true; // change it
    db.set_user_org_pref(p);

    CHECK(db.get_user_org_pref("alice", 1).notify_task_available);
}
```

- [ ] **Step 2: Add user_org_pref_record and user_org_pref_entry to org.hpp**

```cpp
struct user_org_pref_record
{
    std::string username;
    long long   org_id{0};
    int notify_task_available{1};
    int notify_task_unassigned{1};
    int notify_coassignee_changed{1};
    int notify_task_abandoned{1}; // lead-specific — ignored for non-leads at notification time

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, username,                  "username");
        Wt::Dbo::field(a, org_id,                    "org_id");
        Wt::Dbo::field(a, notify_task_available,     "notify_task_available");
        Wt::Dbo::field(a, notify_task_unassigned,    "notify_task_unassigned");
        Wt::Dbo::field(a, notify_coassignee_changed, "notify_coassignee_changed");
        Wt::Dbo::field(a, notify_task_abandoned,     "notify_task_abandoned");
    }
};

struct user_org_pref_entry
{
    std::string username;
    long long   org_id{0};
    bool notify_task_available{true};
    bool notify_task_unassigned{true};
    bool notify_coassignee_changed{true};
    bool notify_task_abandoned{true};
};
```

- [ ] **Step 3: Add function signatures to org_db.hpp**

```cpp
user_org_pref_entry get_user_org_pref(const std::string& username, long long org_id);
void                set_user_org_pref(const user_org_pref_entry& pref);
```

- [ ] **Step 4: Implement in org_db.cpp**

In the constructor: `m_dbo.mapClass<user_org_pref_record>("user_org_pref");`

Migration:
```cpp
migrate(
  "CREATE TABLE IF NOT EXISTS user_org_pref ("
  "id integer primary key autoincrement,"
  " version integer not null default 0,"
  " username text not null default '',"
  " org_id integer not null default 0,"
  " notify_task_available integer not null default 1,"
  " notify_task_unassigned integer not null default 1,"
  " notify_coassignee_changed integer not null default 1,"
  " notify_task_abandoned integer not null default 1)");
```

Implementation:

```cpp
user_org_pref_entry org_db::get_user_org_pref(const std::string& username, long long org_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           rows =
      m_dbo.find<user_org_pref_record>()
        .where("username = ? AND org_id = ?")
        .bind(username)
        .bind(org_id)
        .resultList();
    if(rows.empty())
    {
        return {.username = username, .org_id = org_id};
    }
    const auto& r = *rows.begin();
    return {.username                 = r->username,
            .org_id                   = r->org_id,
            .notify_task_available    = r->notify_task_available != 0,
            .notify_task_unassigned   = r->notify_task_unassigned != 0,
            .notify_coassignee_changed = r->notify_coassignee_changed != 0,
            .notify_task_abandoned    = r->notify_task_abandoned != 0};
}

void org_db::set_user_org_pref(const user_org_pref_entry& pref)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           rows =
      m_dbo.find<user_org_pref_record>()
        .where("username = ? AND org_id = ?")
        .bind(pref.username)
        .bind(pref.org_id)
        .resultList();
    if(rows.empty())
    {
        auto r                          = m_dbo.add(std::make_unique<user_org_pref_record>());
        r.modify()->username            = pref.username;
        r.modify()->org_id              = pref.org_id;
        r.modify()->notify_task_available    = pref.notify_task_available ? 1 : 0;
        r.modify()->notify_task_unassigned   = pref.notify_task_unassigned ? 1 : 0;
        r.modify()->notify_coassignee_changed = pref.notify_coassignee_changed ? 1 : 0;
        r.modify()->notify_task_abandoned    = pref.notify_task_abandoned ? 1 : 0;
    }
    else
    {
        Wt::Dbo::ptr<user_org_pref_record> r = *rows.begin();
        r.modify()->notify_task_available    = pref.notify_task_available ? 1 : 0;
        r.modify()->notify_task_unassigned   = pref.notify_task_unassigned ? 1 : 0;
        r.modify()->notify_coassignee_changed = pref.notify_coassignee_changed ? 1 : 0;
        r.modify()->notify_task_abandoned    = pref.notify_task_abandoned ? 1 : 0;
    }
}
```

- [ ] **Step 5: Build and run tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_org_db
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/org/org.hpp src/org/org_db.hpp src/org/org_db.cpp tests/test_org_db.cpp
git commit -m "$(cat <<'EOF'
feat(org): scaffold per-org user notification preference table

Schema created; get/set functions with all-true defaults. Wiring to
actual notification dispatch is Phase 2.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: edit_task_details capability

**Files:**
- Modify: `src/kanban/team_cap.hpp`
- Modify: `tests/test_team_cap.cpp`

- [ ] **Step 1: Write failing test**

In `tests/test_team_cap.cpp`, add:

```cpp
TEST_CASE("team_cap - edit_task_details: not in member caps, is in lead caps")
{
    CHECK(!team_cap::team_member_caps.has_any(team_cap::edit_task_details));
    CHECK( team_cap::team_lead_caps.has_any(team_cap::edit_task_details));
    CHECK( team_cap::org_lead_caps.has_any(team_cap::edit_task_details));
    CHECK( team_cap::admin_caps.has_any(team_cap::edit_task_details));
    CHECK(!team_cap::org_viewer_caps.has_any(team_cap::edit_task_details));
}
```

- [ ] **Step 2: Run to confirm failure**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_team_cap
```

Expected: compilation error — `edit_task_details` not declared.

- [ ] **Step 3: Add cap to team_cap.hpp**

```cpp
inline constexpr flags edit_task_details = flags::from_value(1u << 9);

// Updated:
inline constexpr flags team_lead_caps = team_member_caps | view_archived | reassign_task
                                      | create_task | archive_task | manage_team
                                      | edit_task_details;
inline constexpr flags org_lead_caps  = team_lead_caps;
```

(`admin_caps = ~flags{}` already includes everything.)

- [ ] **Step 4: Run tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure -R test_team_cap
```

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add src/kanban/team_cap.hpp tests/test_team_cap.cpp
git commit -m "$(cat <<'EOF'
feat(caps): add edit_task_details capability for leads-only title/description/date editing

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: task_editor_form_widget — lead-only fields + multi-assignee UI

This task refactors the most complex widget. The three changes are:
1. Title/description/dates require `edit_task_details` (not `edit_task_fields`).
2. Status dropdown filters out "Done" for non-leads; a `m_status_vals_used` vector replaces the static k_status_vals index mapping in `save()`.
3. Single assignee combo replaced with chip-list multi-assignee widget.

**Files:**
- Modify: `src/kanban/task_editor_form_widget.hpp`
- Modify: `src/kanban/task_editor_form_widget.cpp`

- [ ] **Step 1: Update constructor signature in task_editor_form_widget.hpp**

Add `team_settings_entry settings` after `team_cap::flags caps`:

```cpp
#include "kanban/kanban.hpp" // already included, ensures team_settings_entry visible

task_editor_form_widget(kanban_db&              db,
                        org_db&                 odb,
                        long long               task_id,
                        long long               team_id,
                        const session_data&     session,
                        team_cap::flags         caps,
                        team_settings_entry     settings,
                        std::function<void()>   on_saved,
                        std::function<void()>   on_cancel);
```

Add new members:

```cpp
team_settings_entry   m_settings;
std::vector<std::string> m_status_vals_used; // status values parallel to combo items
```

Remove old assignee members:
```cpp
// Remove:
Wt::WComboBox*        m_assignee_edit{nullptr};
Wt::WText*            m_assignee_display{nullptr};
Wt::WContainerWidget* m_assignee_field{nullptr};
std::vector<std::string> m_assignee_values;

// Add:
Wt::WContainerWidget* m_assignee_list{nullptr};     // chip list of current assignees
Wt::WComboBox*        m_add_member_combo{nullptr};  // lead-only "add member" dropdown
```

- [ ] **Step 2: Update constructor body — capability variables**

In the constructor, replace:

```cpp
const bool can_edit = caps.has_any(team_cap::edit_task_fields) &&
                      (is_new || !m_original.is_archived);
const bool can_assign = caps.has_any(team_cap::reassign_task) &&
                        (is_new || !m_original.is_archived);
const bool can_use_assignee = ...;
```

with:

```cpp
const bool is_lead        = caps.has_any(team_cap::edit_task_details);
const bool not_archived   = is_new || !m_original.is_archived;
const bool can_edit       = is_lead && not_archived;            // title/desc/dates
const bool can_edit_status = not_archived &&
    (is_lead ||
     (caps.has_any(team_cap::edit_task_fields) &&
      m_settings.allow_member_move_columns));
const bool can_reassign   = caps.has_any(team_cap::reassign_task) && not_archived;
```

- [ ] **Step 3: Status combo — filter "done" for non-leads, build m_status_vals_used**

Replace the current static `k_status_vals` loop with:

```cpp
for(size_t i = 0; i < k_status_vals.size(); ++i)
{
    if(k_status_vals[i] == "done" && !is_lead)
    {
        continue; // non-leads cannot select Done
    }
    m_status_vals_used.push_back(k_status_vals[i]);
    m_status_edit->addItem(k_status_labels[i]);
}
// Set current index using m_status_vals_used:
const auto it = std::find(m_status_vals_used.begin(), m_status_vals_used.end(), status_init);
if(it != m_status_vals_used.end())
{
    m_status_edit->setCurrentIndex(
      static_cast<int>(std::distance(m_status_vals_used.begin(), it)));
}
```

Update the status `changed()` and `blurred()` handlers to use `m_status_vals_used` instead of `k_status_vals`.

Update the status widget visibility to use `can_edit_status` instead of `can_edit`:

```cpp
m_status_display->setStyleClass(can_edit_status && !is_new ? "kb-popup-display" : "");
// ...
if(!is_new)
{
    if(can_edit_status) { m_status_edit->hide(); }
    else { m_status_edit->setDisabled(true); m_status_display->hide(); }
}
if(can_edit_status && !is_new) { /* connect signals */ }
```

- [ ] **Step 4: Replace assignee combo with multi-assignee chip list**

Remove the old `m_assignee_field` block entirely. Replace with:

```cpp
// ── Assignees ─────────────────────────────────────────────────────────────────
m_assignee_list = row->addNew<Wt::WContainerWidget>();
m_assignee_list->setStyleClass("kb-editor-field-wrap kb-popup-field");
m_assignee_list->addNew<Wt::WText>("Assignees", Wt::TextFormat::Plain)
  ->setStyleClass("kb-field-label");

auto* chips_container = m_assignee_list->addNew<Wt::WContainerWidget>();
chips_container->setStyleClass("kb-assignee-chips");

// Render initial chips for existing assignees.
const auto rebuild_chips = [this, chips_container, &session, can_reassign, not_archived]()
{
    chips_container->clear();
    const auto current = m_db.assignees_for_task(m_task_id);
    for(const auto& user: current)
    {
        auto* chip = chips_container->addNew<Wt::WContainerWidget>();
        chip->setStyleClass("kb-assignee-chip");
        chip->addNew<Wt::WText>(user, Wt::TextFormat::Plain);

        const bool is_self              = (user == m_username);
        const bool sole_assignee        = (current.size() == 1);
        const bool can_remove_as_lead   = can_reassign;
        const bool can_remove_as_member =
          !can_reassign && is_self && not_archived &&
          (sole_assignee ? m_settings.allow_abandon
                         : m_settings.allow_self_assign_assigned);

        if(can_remove_as_lead || can_remove_as_member)
        {
            auto* rm = chip->addNew<Wt::WPushButton>("×");
            rm->setStyleClass("kb-assignee-rm");
            rm->clicked().connect(
              [this, user, sole_assignee, can_reassign, chips_container, &session]()
              {
                  if(can_reassign && sole_assignee && !m_settings.allow_abandon)
                  {
                      // Lead override — confirm abandon.
                      auto* dlg = addNew<Wt::WDialog>("Confirm abandon");
                      dlg->contents()->addNew<Wt::WText>(
                        "This task has no other assignees. Removing " + user +
                        " will leave it unassigned. Proceed?",
                        Wt::TextFormat::Plain);
                      auto* yes = dlg->footer()->addNew<Wt::WPushButton>("Yes, abandon");
                      auto* no  = dlg->footer()->addNew<Wt::WPushButton>("Cancel");
                      yes->clicked().connect([this, user, dlg, chips_container]() {
                          dlg->reject();
                          m_db.remove_assignee(m_task_id, user, m_username);
                          live_hub::instance().broadcast("task:" + std::to_string(m_task_id));
                          // rebuild chips by triggering a live-hub update
                      });
                      no->clicked().connect([dlg]() { dlg->reject(); });
                      dlg->show();
                  }
                  else
                  {
                      m_db.remove_assignee(m_task_id, user, m_username);
                      live_hub::instance().broadcast("task:" + std::to_string(m_task_id));
                  }
              });
        }
    }
};

if(!is_new)
{
    rebuild_chips();
}

// "Assign yourself" button for non-lead members.
const bool already_assigned = !is_new &&
  std::find(m_original.assignees.begin(), m_original.assignees.end(), m_username)
  != m_original.assignees.end();
const bool can_self_add =
  !can_reassign && not_archived && !is_new && !already_assigned &&
  (m_original.assignees.empty() ? m_settings.allow_self_assign_unassigned
                                 : m_settings.allow_self_assign_assigned);

if(can_self_add)
{
    auto* self_btn = m_assignee_list->addNew<Wt::WPushButton>("Assign to me");
    self_btn->setStyleClass("editor-btn");
    self_btn->clicked().connect([this, &session]() {
        if(m_db.add_assignee(m_task_id, m_username, m_username))
        {
            live_hub::instance().broadcast("task:" + std::to_string(m_task_id));
        }
    });
}

// Lead-only: "Add member" dropdown.
if(can_reassign && !is_new)
{
    auto* add_row = m_assignee_list->addNew<Wt::WContainerWidget>();
    add_row->setStyleClass("kb-assignee-add-row");
    const auto members = m_db.members_for_team(m_team_id);
    m_add_member_combo = add_row->addNew<Wt::WComboBox>();
    m_add_member_combo->setStyleClass("editor-field");
    m_add_member_combo->addItem("(select member)");
    for(const auto& mem: members)
    {
        m_add_member_combo->addItem(mem);
    }
    auto* add_btn = add_row->addNew<Wt::WPushButton>("Add");
    add_btn->setStyleClass("editor-btn");
    add_btn->clicked().connect([this]() {
        const int idx = m_add_member_combo->currentIndex();
        if(idx <= 0) { return; }
        const auto members_list = m_db.members_for_team(m_team_id);
        const int mem_idx = idx - 1; // offset for "(select member)"
        if(mem_idx < 0 || mem_idx >= static_cast<int>(members_list.size())) { return; }
        const std::string new_user = members_list[mem_idx];
        if(m_db.add_assignee(m_task_id, new_user, m_username))
        {
            // Fire task_assigned notification if lead assigned someone else.
            if(new_user != m_username)
            {
                const auto team = m_db.find_team(m_team_id);
                const auto task = m_db.find_task(m_task_id);
                m_odb.push_notification(
                  new_user, "task_assigned",
                  make_task_assigned_payload(m_task_id,
                                             task ? task->title : "",
                                             m_team_id,
                                             team ? team->name : ""));
                live_hub::instance().broadcast("user:" + new_user);
            }
            live_hub::instance().broadcast("task:" + std::to_string(m_task_id));
            m_add_member_combo->setCurrentIndex(0);
        }
    });
}
```

- [ ] **Step 5: Update save() — remove all assigned_to logic**

In `save()`:
- Change the guard at line 823 from `caps.has_any(team_cap::edit_task_fields)` to `caps.has_any(team_cap::edit_task_details) || can_edit_status_allowed_for_save` — simplest approach: add a helper bool captured in the constructor, or just check:
  ```cpp
  const bool creating = (m_original.id == 0);
  if(!creating && !m_caps.has_any(team_cap::edit_task_fields) && !m_caps.has_any(team_cap::edit_task_details))
  {
      return; // completely read-only
  }
  ```
- Remove lines 847–862 (all `ai`, `new_assignee`, `old_assignee`, reassign guard).
- Remove line 869: `t.assigned_to = new_assignee;`
- Remove lines 898–904 (the old `task_assigned` notification block — it moved to the "Add" button handler).
- Use `m_status_vals_used` instead of `k_status_vals` when reading the selected status:
  ```cpp
  const int         si = m_status_edit->currentIndex();
  const std::string status =
    (si >= 0 && si < static_cast<int>(m_status_vals_used.size()))
      ? m_status_vals_used[si]
      : (creating ? "todo" : m_original.status);
  ```
- Add a non-lead guard for "done" status in save():
  ```cpp
  if(!m_caps.has_any(team_cap::edit_task_details))
  {
      if(status == "done" || m_original.status == "done")
      {
          return; // belt-and-suspenders: UI already filters this
      }
  }
  ```

- [ ] **Step 6: Update history label map for "assignees" field**

In `rebuild_history()`, the field-name → label map at line 939 currently has `"assigned_to"`. Add `"assignees"`:

```cpp
{"assignees", "Assignees"},
```

And remove `{"assigned_to", "Assigned to"}`.

- [ ] **Step 7: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -30
```

Fix any remaining `assigned_to` references in this file. There should be none left after the above steps.

- [ ] **Step 8: Build all and run C++ tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure
```

Expected: all C++ tests pass. UI changes are verified manually / via E2E tests.

- [ ] **Step 9: Commit**

```bash
git add src/kanban/task_editor_form_widget.hpp src/kanban/task_editor_form_widget.cpp
git commit -m "$(cat <<'EOF'
feat(ui): multi-assignee chip widget; restrict title/desc/dates to leads

Assignee combo replaced with chip list + add/remove buttons governed by
team settings. Title, description, and dates require edit_task_details cap.
Status dropdown hides Done option for non-leads.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: kanban_board_widget — multi-assignee serialization + move flags

**Files:**
- Modify: `src/kanban/kanban_board_widget.hpp`
- Modify: `src/kanban/kanban_board_widget.cpp`

- [ ] **Step 1: Update constructor and refresh signatures in kanban_board_widget.hpp**

Replace `bool can_edit` with two flags:

```cpp
kanban_board_widget(std::vector<kanban_task_entry>                          tasks,
                    bool                                                    can_move_columns,
                    bool                                                    can_move_done,
                    const std::map<long long, std::string>&                 type_colors,
                    std::function<void(long long, const std::string&, int)> on_move,
                    std::function<void(long long)>                          on_edit);

void refresh(std::vector<kanban_task_entry>          tasks,
             bool                                    can_move_columns,
             bool                                    can_move_done,
             const std::map<long long, std::string>& type_colors);
```

- [ ] **Step 2: Update serialize_tasks() to output assignees array**

In `kanban_board_widget.cpp`, in `serialize_tasks()`, find the field that outputs `assigned_to`. Replace with:

```cpp
// Before (single assignee):
// out += "\"assigned_to\":\"" + escape(t.assigned_to) + "\"";

// After (array):
out += "\"assignees\":[";
for(size_t i = 0; i < t.assignees.size(); ++i)
{
    if(i > 0) { out += ','; }
    out += '"';
    out += t.assignees[i]; // usernames are safe identifiers, no escaping needed
    out += '"';
}
out += ']';
```

- [ ] **Step 3: Update init_js() to pass both move flags into JS**

In `init_js()`, the JS is rendered with a `canEdit` variable. Replace with two booleans:

```cpp
void kanban_board_widget::init_js(const std::string& json,
                                   bool               can_move_columns,
                                   bool               can_move_done)
{
    // Pass both flags to the JS init.
    // The JS board reads canMoveColumns and canMoveDone.
    const std::string js = R"(
        (function(){
            var data   = )" + json + R"(;
            var opts   = {
                canMoveColumns: )" + (can_move_columns ? "true" : "false") + R"(,
                canMoveDone:    )" + (can_move_done ? "true" : "false") + R"(
            };
            KanbanBoard.init(')" + m_mount_id + R"(', ')" + m_cb_id + R"(', data, opts);
        })();
    )";
    Wt::WApplication::instance()->doJavaScript(js);
}
```

Update the JavaScript board (`resources/kanban-board.js` or similar) to:
- Use `opts.canMoveColumns` to enable/disable drag between non-done columns.
- Use `opts.canMoveDone` to enable/disable drag to/from the "done" column.
- Render `task.assignees` (an array) as a comma-joined display on each card, e.g. `task.assignees.join(', ')` or as separate avatar chips.

> **Note:** The exact JS file path and function name (`KanbanBoard.init`) should be verified by checking the existing board JS. Apply the pattern used there.

- [ ] **Step 4: Update constructor and refresh implementations**

Pass `can_move_columns` and `can_move_done` through to `init_js()` and store if needed for `refresh()`.

- [ ] **Step 5: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -30
```

Fix callers that pass `bool can_edit` — they'll be updated in Task 11.

- [ ] **Step 6: Commit (after callers are updated)**

After Task 11 makes it compile cleanly:

```bash
git add src/kanban/kanban_board_widget.hpp src/kanban/kanban_board_widget.cpp
git commit -m "$(cat <<'EOF'
feat(board): multi-assignee JSON serialization; split can_edit into move-column + move-done flags

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: Wire settings through pages and altinf_app

**Files:**
- Modify: `src/pages/kanban_board_page.hpp` + `.cpp`
- Modify: `src/pages/kanban_task_editor_page.hpp` + `.cpp`
- Modify: `src/kanban/task_popup_widget.hpp` + `.cpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Add team_settings_entry to kanban_board_page**

In `kanban_board_page.hpp`, add `#include "kanban/kanban.hpp"` if not already present, and add to the constructor:

```cpp
kanban_board_page(kanban_db&          db,
                  org_db&             odb,
                  const session_data& session,
                  long long           team_id,
                  team_cap::flags     caps,
                  team_settings_entry settings,
                  bool                show_gantt);
```

Add member: `team_settings_entry m_settings;`

- [ ] **Step 2: Update kanban_board_page.cpp**

Store `settings` in `m_settings`. Compute board-widget flags:

```cpp
const bool is_lead          = m_caps.has_any(team_cap::edit_task_details);
const bool can_move_columns = is_lead ||
    (m_caps.has_any(team_cap::edit_task_fields) && m_settings.allow_member_move_columns);
const bool can_move_done    = is_lead;
```

Update the `kanban_board_widget` construction in `refresh()` to use `can_move_columns` and `can_move_done` instead of the old `bool can_edit`.

In the `on_move` callback, add server-side validation:

```cpp
auto on_move = [this, can_move_columns, can_move_done]
               (long long task_id, const std::string& new_status, int sort_order)
{
    const auto task = m_db.find_task(task_id);
    if(!task) { return; }

    const bool from_done = (task->status == "done");
    const bool to_done   = (new_status == "done");

    if((from_done || to_done) && !can_move_done)
    {
        return; // reject silently — JS already prevents this, but guard server-side
    }
    if(!from_done && !to_done && !can_move_columns)
    {
        return;
    }

    m_db.update_task_status(task_id, new_status, sort_order, m_username);
    live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
};
```

- [ ] **Step 3: Add team_settings_entry to kanban_task_editor_page**

In `kanban_task_editor_page.hpp`:

```cpp
kanban_task_editor_page(kanban_db&               db,
                        org_db&                  odb,
                        long long                team_id,
                        const session_data&      session,
                        team_cap::flags          caps,
                        team_settings_entry      settings,
                        const kanban_task_entry* existing,
                        std::function<void()>    on_save);
```

In the `.cpp`, pass `settings` to `task_editor_form_widget` constructor.

- [ ] **Step 4: Add team_settings_entry to task_popup_widget**

In `task_popup_widget.hpp`:

```cpp
task_popup_widget(kanban_db&          db,
                  org_db&             odb,
                  long long           task_id,
                  const session_data& session,
                  team_cap::flags     caps,
                  team_settings_entry settings,
                  long long           team_id);
```

In the `.cpp`, pass `settings` to `task_editor_form_widget` constructor.

- [ ] **Step 5: Update altinf_app.cpp — fetch settings and thread through**

After the `resolve_team_caps()` call, add:

```cpp
const auto settings = m_kanban_db->settings_for_team(team_id);
```

Update all board/editor/popup construction sites to pass `settings`:

```cpp
// Board page (two places: board and gantt):
*m_kanban_db, *m_org_db, m_session, team_id, caps, settings, false

// New task editor:
*m_kanban_db, *m_org_db, team_id, m_session, caps, settings, nullptr, [...]

// Edit task editor:
*m_kanban_db, *m_org_db, team_id, m_session, caps, settings, &(*m_edit_task), [...]
```

The `task_popup_widget` is constructed inside `kanban_board_page.cpp`; verify its site there too.

- [ ] **Step 6: Build and run all C++ tests**

```bash
cmake --build build --parallel $(nproc) && cd build && ctest --output-on-failure
```

Expected: all tests pass, no compilation errors.

- [ ] **Step 7: Commit**

```bash
git add src/pages/kanban_board_page.hpp src/pages/kanban_board_page.cpp \
        src/pages/kanban_task_editor_page.hpp src/pages/kanban_task_editor_page.cpp \
        src/kanban/task_popup_widget.hpp src/kanban/task_popup_widget.cpp \
        src/altinf_app.cpp
git commit -m "$(cat <<'EOF'
feat: wire team_settings through board page, task editor, and popup

altinf_app fetches settings per-team and threads them into all widgets.
Board validates moves server-side against settings and lead status.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review Checklist

### Spec coverage

| Requirement | Task |
|-------------|------|
| Multi-assignee junction table | Tasks 1–3 |
| Done-column clears assignees, no unassign notification (audit only) | Task 4 |
| add_assignee rejects done/archived tasks | Task 3 |
| Team settings: allow_member_move_columns | Tasks 5–6 + 11 |
| Team settings: allow_self_assign_unassigned | Tasks 5–6 + 9 |
| Team settings: allow_self_assign_assigned | Tasks 5–6 + 9 |
| Team settings: allow_abandon | Tasks 5–6 + 9 |
| Team settings: org-wide default + per-team override | Tasks 5–6 |
| Team settings: changes audited | Tasks 5–6 |
| Lead bypass of team settings | Task 9 (is_lead check bypasses m_settings) |
| Lead abandon-confirmation alert when allow_abandon=false | Task 9 (WDialog in chip remove handler) |
| Lead-only: title, description, dates | Task 8 (edit_task_details cap) + Task 9 |
| Non-lead status dropdown excludes Done | Task 9 |
| Server-side move validation (Done restricted to leads) | Task 11 |
| Per-org user notification prefs schema (scaffold) | Task 7 |
| task_assigned notification fires when lead assigns someone else | Task 9 (add button handler) |
| Read-only board access for org members (org_viewer_caps already returns view_board) | Existing `resolve_team_caps()` — verified, no code change needed |
| assignees serialized to board JS | Task 10 |

### Open items (not in this plan — addressed in later phases)

- Notification dispatch for task_available, task_unassigned, task_abandoned (Phase 2)
- Watching system (Phase 3)
- Team settings management UI (the `set_team_settings()` DB function exists; the UI page is deferred)
- User notification preferences UI (schema exists; the settings page is Phase 2)
