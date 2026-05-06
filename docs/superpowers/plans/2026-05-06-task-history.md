# Task History & Soft Delete Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Track every meaningful change to a task (who, what field, before/after, when), archive rather than delete tasks/teams/orgs, and expose history to any user who can view the task.

**Architecture:** Two new tables (`task_event`, `task_field_change`) record field-level diffs on every write. Existing tables gain `is_archived` columns managed via the existing ALTER TABLE migration pattern. The task editor gains a History tab (WTabWidget); a new archive page lists archived tasks per team; the team management page gains a collapsible archived-teams section.

**Tech Stack:** C++/Wt Dbo (SQLite backend), Catch2 (unit tests), Playwright (E2E). Build: `cmake --build build --parallel $(nproc)`. Run Catch2 tests: `cd build && ctest --output-on-failure`. Run E2E: `cd e2e && npx playwright test`.

---

## File Map

| Action | File | Responsibility |
|---|---|---|
| Modify | `src/kanban/kanban.hpp` | New Dbo records + entry structs; `is_archived` on existing structs |
| Modify | `src/kanban/kanban_db.hpp` | Updated signatures; new public/private methods |
| Modify | `src/kanban/kanban_db.cpp` | All DB logic |
| Modify | `src/org/org.hpp` | `is_archived` on `org_record` / `org_entry` |
| Modify | `src/org/org_db.hpp` | `archive_org` declaration |
| Modify | `src/org/org_db.cpp` | Migration + `archive_org`; filter `all_orgs`/`orgs_for_user` |
| Modify | `src/pages/kanban_board_page.hpp` | Add `m_username` member |
| Modify | `src/pages/kanban_board_page.cpp` | Thread actor; add "Archived" link |
| Modify | `src/pages/kanban_team_page.cpp` | `archive_team`; archived teams section |
| Modify | `src/pages/kanban_task_editor_page.hpp` | Tab widget + history panel members |
| Modify | `src/pages/kanban_task_editor_page.cpp` | Tabs; read-only mode; archive/history |
| Modify | `src/altinf_app.cpp` | Route for `/board/{id}/archive` |
| Modify | `tests/test_kanban_db.cpp` | Update call sites; new history/archive tests |
| Modify | `tests/test_org_db.cpp` | `archive_org` test |
| Create | `src/pages/kanban_archive_page.hpp` | Archive page declaration |
| Create | `src/pages/kanban_archive_page.cpp` | Archive page implementation |

---

## Task 1: New structs in kanban.hpp and org.hpp

**Files:**
- Modify: `src/kanban/kanban.hpp`
- Modify: `src/org/org.hpp`

- [ ] **Step 1: Add `is_archived` to kanban.hpp records and entries**

In `src/kanban/kanban.hpp`, add `is_archived` to `kanban_task_record`, `team_record`, `kanban_task_entry`, and `team_entry`:

```cpp
struct team_record
{
    std::string name;
    long long   org_id{0};
    int         is_archived{0};  // ← add

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, name,        "name");
        Wt::Dbo::field(a, org_id,      "org_id");
        Wt::Dbo::field(a, is_archived, "is_archived");  // ← add
    }
};
```

```cpp
struct kanban_task_record
{
    long long   team_id{0};
    std::string status;
    std::string title;
    std::string description;
    std::string assigned_to;
    Wt::WDate   start_date;
    Wt::WDate   end_date;
    long long   type_id{0};
    int         sort_order{0};
    int         is_archived{0};  // ← add

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, team_id,     "team_id");
        Wt::Dbo::field(a, status,      "status");
        Wt::Dbo::field(a, title,       "title");
        Wt::Dbo::field(a, description, "description");
        Wt::Dbo::field(a, assigned_to, "assigned_to");
        Wt::Dbo::field(a, start_date,  "start_date");
        Wt::Dbo::field(a, end_date,    "end_date");
        Wt::Dbo::field(a, type_id,     "type_id");
        Wt::Dbo::field(a, sort_order,  "sort_order");
        Wt::Dbo::field(a, is_archived, "is_archived");  // ← add
    }
};
```

```cpp
struct team_entry
{
    long long   id{0};
    std::string name;
    long long   org_id{0};
    bool        is_archived{false};  // ← add
};

struct kanban_task_entry
{
    long long   id{0};
    long long   team_id{0};
    std::string status;
    std::string title;
    std::string description;
    std::string assigned_to;
    Wt::WDate   start_date;
    Wt::WDate   end_date;
    long long   type_id{0};
    int         sort_order{0};
    bool        is_archived{false};  // ← add
};
```

- [ ] **Step 2: Add new Dbo records and entry structs to kanban.hpp**

Append to the Dbo records section (before the entry structs section):

```cpp
struct task_event_record
{
    long long   task_id{0};
    std::string actor;
    std::string occurred_at;
    std::string event_type;

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, task_id,     "task_id");
        Wt::Dbo::field(a, actor,       "actor");
        Wt::Dbo::field(a, occurred_at, "occurred_at");
        Wt::Dbo::field(a, event_type,  "event_type");
    }
};

struct task_field_change_record
{
    long long   event_id{0};
    std::string field_name;
    std::string old_value;
    std::string new_value;

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, event_id,   "event_id");
        Wt::Dbo::field(a, field_name, "field_name");
        Wt::Dbo::field(a, old_value,  "old_value");
        Wt::Dbo::field(a, new_value,  "new_value");
    }
};
```

Append to the entry structs section:

```cpp
struct task_field_change_entry
{
    std::string field_name;
    std::string old_value;
    std::string new_value;
};

struct task_event_entry
{
    long long   id{0};
    long long   task_id{0};
    std::string actor;
    std::string occurred_at;
    std::string event_type;
    std::vector<task_field_change_entry> changes;
};
```

- [ ] **Step 3: Add `is_archived` to org.hpp**

In `src/org/org.hpp`, add to `org_record`:

```cpp
struct org_record
{
    std::string name;
    int         is_archived{0};  // ← add

    template<class Action>
    void persist(Action& a)
    {
        Wt::Dbo::field(a, name,        "name");
        Wt::Dbo::field(a, is_archived, "is_archived");  // ← add
    }
};
```

Add to `org_entry`:

```cpp
struct org_entry
{
    long long   id{0};
    std::string name;
    bool        is_archived{false};  // ← add
};
```

- [ ] **Step 4: Build to confirm no compile errors**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -20
```

Expected: build succeeds (the new fields are ignored by the DB layer until mapped).

- [ ] **Step 5: Commit**

```bash
git add src/kanban/kanban.hpp src/org/org.hpp
git commit -m "feat: add is_archived and history structs to kanban/org types"
```

---

## Task 2: kanban_db — map new tables and add migrations

**Files:**
- Modify: `src/kanban/kanban_db.hpp`
- Modify: `src/kanban/kanban_db.cpp`

- [ ] **Step 1: Update kanban_db.hpp — add new method declarations**

Replace the Tasks section and add new public methods. The complete updated public interface (Tasks block + new additions):

```cpp
// Tasks
long long                        add_task(const kanban_task_entry& e,
                                          const std::string& actor);
void                             update_task(const kanban_task_entry& e,
                                             const std::string& actor);
void                             update_task_status(long long          id,
                                                    const std::string& status,
                                                    int                sort_order,
                                                    const std::string& actor);
bool                             self_assign(long long task_id, const std::string& username);
void                             archive_task(long long id, const std::string& actor);
void                             unarchive_task(long long id, const std::string& actor);
std::optional<kanban_task_entry> find_task(long long id);
std::vector<kanban_task_entry>   tasks_for_team(long long team_id);
std::vector<kanban_task_entry>   archived_tasks_for_team(long long team_id);
std::vector<task_event_entry>    history_for_task(long long task_id);
```

Replace Teams section to update `delete_team` → `archive_team`:

```cpp
// Teams
long long                 create_team(const std::string& name, long long org_id);
void                      rename_team(long long id, const std::string& name);
void                      set_team_org(long long team_id, long long org_id);
std::vector<team_entry>   all_teams();
std::vector<team_entry>   teams_for_org(long long org_id);
std::optional<team_entry> find_team(long long id);
void                      archive_team(long long id, const std::string& actor);
void                      unarchive_team(long long id);
```

Add to the private section:

```cpp
void record_event(long long task_id,
                  const std::string& actor,
                  const std::string& event_type,
                  const std::vector<task_field_change_entry>& changes);
std::string type_name_for_id(long long type_id);

static team_entry        to_entry(const Wt::Dbo::ptr<team_record>& p);
static kanban_task_entry to_entry(const Wt::Dbo::ptr<kanban_task_record>& p);
static task_type_entry   to_entry(const Wt::Dbo::ptr<task_type_record>& p);
```

- [ ] **Step 2: Map new classes and add migrations in kanban_db.cpp constructor**

In the `kanban_db::kanban_db` constructor, add mappings and migrations:

```cpp
kanban_db::kanban_db(const std::string& db_path)
{
    m_dbo.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
    m_dbo.mapClass<team_record>("team");
    m_dbo.mapClass<team_member_record>("team_member");
    m_dbo.mapClass<kanban_task_record>("kanban_task");
    m_dbo.mapClass<task_type_record>("task_type");
    m_dbo.mapClass<task_event_record>("task_event");             // ← add
    m_dbo.mapClass<task_field_change_record>("task_field_change"); // ← add

    try { m_dbo.createTables(); } catch(const Wt::Dbo::Exception&) {}

    // Idempotent column migrations.
    auto migrate = [this](const std::string& sql) {
        try { Wt::Dbo::Transaction t{m_dbo}; m_dbo.execute(sql); } catch(...) {}
    };

    migrate("ALTER TABLE team ADD COLUMN org_id INTEGER NOT NULL DEFAULT 0");
    migrate("ALTER TABLE team_member ADD COLUMN is_lead INTEGER NOT NULL DEFAULT 0");
    migrate("ALTER TABLE kanban_task ADD COLUMN type_id INTEGER NOT NULL DEFAULT 0");
    migrate("ALTER TABLE kanban_task ADD COLUMN is_archived INTEGER NOT NULL DEFAULT 0"); // ← add
    migrate("ALTER TABLE team ADD COLUMN is_archived INTEGER NOT NULL DEFAULT 0");        // ← add
}
```

Note: the original constructor used individual try/catch blocks. Replace them all with the `migrate` lambda shown above — same behaviour, less repetition.

- [ ] **Step 3: Update `to_entry` helpers to populate `is_archived`**

```cpp
team_entry kanban_db::to_entry(const Wt::Dbo::ptr<team_record>& p)
{
    return {.id = p.id(), .name = p->name, .org_id = p->org_id,
            .is_archived = p->is_archived != 0};  // ← add
}

kanban_task_entry kanban_db::to_entry(const Wt::Dbo::ptr<kanban_task_record>& p)
{
    kanban_task_entry e;
    e.id          = p.id();
    e.team_id     = p->team_id;
    e.status      = p->status;
    e.title       = p->title;
    e.description = p->description;
    e.assigned_to = p->assigned_to;
    e.start_date  = p->start_date;
    e.end_date    = p->end_date;
    e.type_id     = p->type_id;
    e.sort_order  = p->sort_order;
    e.is_archived = p->is_archived != 0;  // ← add
    return e;
}
```

- [ ] **Step 4: Build to confirm no compile errors**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -20
```

Expected: build succeeds. Tests will fail until call sites are updated (do not run tests yet).

- [ ] **Step 5: Commit**

```bash
git add src/kanban/kanban_db.hpp src/kanban/kanban_db.cpp
git commit -m "feat: map task_event/task_field_change tables; add is_archived migrations"
```

---

## Task 3: kanban_db — record_event and history_for_task (TDD)

**Files:**
- Modify: `src/kanban/kanban_db.cpp`
- Modify: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Write failing tests**

Add to `tests/test_kanban_db.cpp` (at the end, before the task types section):

```cpp
// ---- history ----

TEST_CASE("kanban_db - history_for_task empty before any events")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    // Add a task bypassing the history-recording add_task (use raw id from add_task below
    // once that is implemented). For now, just confirm an unknown task returns empty.
    CHECK(db.history_for_task(9999).empty());
}
```

- [ ] **Step 2: Run test to confirm it compiles and passes** (it is trivially true until Step 3)

```bash
cd build && ctest -R test_kanban_db --output-on-failure
```

- [ ] **Step 3: Implement the `utc_now_iso8601` free function and `record_event` + `history_for_task`**

Add at the top of `src/kanban/kanban_db.cpp` (after the existing includes):

```cpp
#include <chrono>
#include <ctime>
```

Add a static file-local helper below the existing `escape_json`-style helpers (or right after the includes):

```cpp
static std::string utc_now_iso8601()
{
    const auto t = std::chrono::system_clock::to_time_t(
                       std::chrono::system_clock::now());
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}
```

Implement `record_event` (called within an open transaction):

```cpp
void kanban_db::record_event(long long          task_id,
                              const std::string& actor,
                              const std::string& event_type,
                              const std::vector<task_field_change_entry>& changes)
{
    auto ev              = m_dbo.add(std::make_unique<task_event_record>());
    ev.modify()->task_id     = task_id;
    ev.modify()->actor       = actor;
    ev.modify()->occurred_at = utc_now_iso8601();
    ev.modify()->event_type  = event_type;
    m_dbo.flush();
    const long long event_id = ev.id();

    for(const auto& ch: changes)
    {
        auto fc              = m_dbo.add(std::make_unique<task_field_change_record>());
        fc.modify()->event_id   = event_id;
        fc.modify()->field_name = ch.field_name;
        fc.modify()->old_value  = ch.old_value;
        fc.modify()->new_value  = ch.new_value;
    }
}
```

Implement `history_for_task`:

```cpp
std::vector<task_event_entry> kanban_db::history_for_task(long long task_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto events = m_dbo.find<task_event_record>()
                          .where("task_id = ?")
                          .bind(task_id)
                          .orderBy("id DESC")
                          .resultList();

    std::vector<task_event_entry> out;
    for(const auto& ev: events)
    {
        task_event_entry entry;
        entry.id          = ev.id();
        entry.task_id     = ev->task_id;
        entry.actor       = ev->actor;
        entry.occurred_at = ev->occurred_at;
        entry.event_type  = ev->event_type;

        const auto fields = m_dbo.find<task_field_change_record>()
                              .where("event_id = ?")
                              .bind(entry.id)
                              .orderBy("id")
                              .resultList();
        for(const auto& fc: fields)
        {
            entry.changes.push_back(
              {.field_name = fc->field_name,
               .old_value  = fc->old_value,
               .new_value  = fc->new_value});
        }
        out.push_back(std::move(entry));
    }
    return out;
}
```

Also implement `type_name_for_id` (called within an open transaction):

```cpp
std::string kanban_db::type_name_for_id(long long type_id)
{
    if(type_id == 0)
        return "";
    try
    {
        auto p = m_dbo.load<task_type_record>(type_id);
        return p->name;
    }
    catch(const Wt::Dbo::ObjectNotFoundException&)
    {
        return "";
    }
}
```

- [ ] **Step 4: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -20
```

Expected: build succeeds (ignore existing test failures from changed signatures for now).

- [ ] **Step 5: Commit**

```bash
git add src/kanban/kanban_db.cpp tests/test_kanban_db.cpp
git commit -m "feat: add record_event, history_for_task, type_name_for_id to kanban_db"
```

---

## Task 4: kanban_db — update add_task/update_task/update_task_status + fix all test call sites

This is the signature-change task. All existing call sites in the test file break at once and are fixed here.

**Files:**
- Modify: `src/kanban/kanban_db.cpp`
- Modify: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Implement new `add_task` with actor**

Replace the existing `add_task` implementation:

```cpp
long long kanban_db::add_task(const kanban_task_entry& e, const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};
    auto                 p  = m_dbo.add(std::make_unique<kanban_task_record>());
    p.modify()->team_id     = e.team_id;
    p.modify()->status      = e.status.empty() ? "todo" : e.status;
    p.modify()->title       = e.title;
    p.modify()->description = e.description;
    p.modify()->assigned_to = e.assigned_to;
    p.modify()->start_date  = e.start_date;
    p.modify()->end_date    = e.end_date;
    p.modify()->type_id     = e.type_id;
    p.modify()->sort_order  = e.sort_order;
    p.modify()->is_archived = 0;
    m_dbo.flush();
    const long long new_id = p.id();
    record_event(new_id, actor, "created", {});
    return new_id;
}
```

- [ ] **Step 2: Implement new `update_task` with actor and diff**

Replace the existing `update_task` implementation:

```cpp
void kanban_db::update_task(const kanban_task_entry& e, const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(e.id).resultList();
    if(results.empty())
        return;

    Wt::Dbo::ptr<kanban_task_record> p = *results.begin();

    // Compute diff before writing.
    std::vector<task_field_change_entry> changes;
    auto check = [&](const std::string& field,
                     const std::string& old_val,
                     const std::string& new_val) {
        if(old_val != new_val)
            changes.push_back({field, old_val, new_val});
    };

    const auto date_str = [](const Wt::WDate& d) {
        return d.isValid() ? d.toString("yyyy-MM-dd").toUTF8() : std::string{};
    };

    check("status",      p->status,      e.status);
    check("title",       p->title,       e.title);
    check("description", p->description, e.description);
    check("assigned_to", p->assigned_to, e.assigned_to);
    check("start_date",  date_str(p->start_date), date_str(e.start_date));
    check("end_date",    date_str(p->end_date),   date_str(e.end_date));

    const std::string old_type = type_name_for_id(p->type_id);
    const std::string new_type = type_name_for_id(e.type_id);
    check("type", old_type, new_type);

    // Write.
    p.modify()->status      = e.status;
    p.modify()->title       = e.title;
    p.modify()->description = e.description;
    p.modify()->assigned_to = e.assigned_to;
    p.modify()->start_date  = e.start_date;
    p.modify()->end_date    = e.end_date;
    p.modify()->type_id     = e.type_id;
    p.modify()->sort_order  = e.sort_order;

    if(!changes.empty())
        record_event(e.id, actor, "updated", changes);
}
```

- [ ] **Step 3: Implement new `update_task_status` with actor**

Replace the existing `update_task_status` implementation:

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
        return;
    Wt::Dbo::ptr<kanban_task_record> p = *results.begin();

    if(p->status != status)
        record_event(id, actor, "updated", {{"status", p->status, status}});

    p.modify()->status     = status;
    p.modify()->sort_order = sort_order;
}
```

- [ ] **Step 4: Update `self_assign` to record history**

Replace the existing `self_assign` implementation:

```cpp
bool kanban_db::self_assign(long long task_id, const std::string& username)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(task_id).resultList();
    if(results.empty())
        return false;
    Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
    if(!p->assigned_to.empty())
        return false;
    if(!is_member(p->team_id, username))
        return false;
    record_event(task_id, username, "updated",
                 {{"assigned_to", p->assigned_to, username}});
    p.modify()->assigned_to = username;
    return true;
}
```

- [ ] **Step 5: Update `tasks_for_team` to filter archived tasks**

```cpp
std::vector<kanban_task_entry> kanban_db::tasks_for_team(long long team_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results = m_dbo.find<kanban_task_record>()
                           .where("team_id = ? AND is_archived = 0")  // ← filter
                           .bind(team_id)
                           .orderBy("sort_order, id")
                           .resultList();
    std::vector<kanban_task_entry> out;
    for(const auto& p: results)
        out.push_back(to_entry(p));
    return out;
}
```

- [ ] **Step 6: Remove `delete_task` from `kanban_db.hpp` and `kanban_db.cpp`**

Delete the `void delete_task(long long id);` declaration from `kanban_db.hpp`.
Delete the `void kanban_db::delete_task(long long id) { ... }` implementation from `kanban_db.cpp`.
(The only caller was `kanban_task_editor_page.cpp:283`, which is replaced in Task 10.)

- [ ] **Step 7: Update ALL existing test call sites in test_kanban_db.cpp**

Every `add_task`, `update_task`, `update_task_status`, and `delete_task` call in the test file needs updating. Apply these changes:

```cpp
// Line 75 (inside delete_team test — updated in Task 6):
//   db.add_task(make_task(tid, "Chore"));
//   db.delete_team(tid);
// Leave for Task 6.

// "add task and retrieve for team" test:
db.add_task(make_task(tid, "Alpha", "todo", 1), "test");
db.add_task(make_task(tid, "Beta",  "todo", 0), "test");

// "find_task" test:
const long long id = db.add_task(make_task(tid, "My Task"), "test");

// "update task" test:
const long long id = db.add_task(make_task(tid, "Original"), "test");
// ...
db.update_task(task, "test");

// "update_task_status" test:
const long long id = db.add_task(make_task(tid, "T", "todo"), "test");
db.update_task_status(id, "done", 5, "test");

// "delete task" test — rename to archive:
TEST_CASE("kanban_db - archive_task hides task from active list")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id  = db.add_task(make_task(tid, "To Archive"), "alice");
    db.add_task(make_task(tid, "To Keep"), "alice");
    db.archive_task(id, "alice");
    const auto tasks = db.tasks_for_team(tid);
    REQUIRE(tasks.size() == 1);
    CHECK(tasks[0].title == "To Keep");
    // archived task is still findable
    const auto archived = db.find_task(id);
    REQUIRE(archived.has_value());
    CHECK(archived->is_archived);
}

// "self_assign succeeds" test:
const long long id = db.add_task(make_task(tid, "Work"), "test");

// "self_assign fails when already assigned" test:
const long long id = db.add_task(t, "test");

// "self_assign fails for non-member" test:
const long long id = db.add_task(make_task(tid, "Work"), "test");

// "delete_task_type zeroes out affected tasks" test:
const long long tid = db.add_task(e, "test");

// "task type_id persists through add and find" test:
const long long id = db.add_task(e, "test");

// "update_task preserves type_id change" test:
const long long id = db.add_task(e, "test");
// ...
db.update_task(task, "test");
```

- [ ] **Step 7: Add history tests for add_task and update_task**

Add to the `// ---- history ----` section in the test file:

```cpp
TEST_CASE("kanban_db - add_task records created event")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id  = db.add_task(make_task(tid, "Hello"), "alice");
    const auto hist = db.history_for_task(id);
    REQUIRE(hist.size() == 1);
    CHECK(hist[0].event_type == "created");
    CHECK(hist[0].actor == "alice");
    CHECK(hist[0].changes.empty());
}

TEST_CASE("kanban_db - update_task records field diffs")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id  = db.add_task(make_task(tid, "Original"), "alice");
    auto task    = *db.find_task(id);
    task.title   = "Updated";
    task.status  = "done";
    db.update_task(task, "bob");
    const auto hist = db.history_for_task(id);
    REQUIRE(hist.size() == 2); // created + updated
    const auto& upd = hist[0]; // newest first
    CHECK(upd.event_type == "updated");
    CHECK(upd.actor == "bob");
    REQUIRE(upd.changes.size() == 2);
    const auto title_ch = std::find_if(upd.changes.begin(), upd.changes.end(),
      [](const task_field_change_entry& c){ return c.field_name == "title"; });
    REQUIRE(title_ch != upd.changes.end());
    CHECK(title_ch->old_value == "Original");
    CHECK(title_ch->new_value == "Updated");
}

TEST_CASE("kanban_db - update_task with no changes records no event")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id  = db.add_task(make_task(tid, "Same"), "alice");
    auto task = *db.find_task(id);
    db.update_task(task, "alice"); // no fields changed
    const auto hist = db.history_for_task(id);
    REQUIRE(hist.size() == 1); // only the created event
}

TEST_CASE("kanban_db - update_task_status records status change")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id  = db.add_task(make_task(tid, "T", "todo"), "alice");
    db.update_task_status(id, "done", 0, "bob");
    const auto hist = db.history_for_task(id);
    REQUIRE(hist.size() == 2);
    CHECK(hist[0].event_type == "updated");
    CHECK(hist[0].actor == "bob");
    REQUIRE(hist[0].changes.size() == 1);
    CHECK(hist[0].changes[0].field_name == "status");
    CHECK(hist[0].changes[0].old_value  == "todo");
    CHECK(hist[0].changes[0].new_value  == "done");
}
```

- [ ] **Step 8: Build and run tests**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
cd build && ctest -R test_kanban_db --output-on-failure
```

Expected: all `test_kanban_db` tests pass. (The `delete_team`→`archive_team` rename is in Task 6 — if the test for it fails until then, that is expected; update it in Task 6.)

- [ ] **Step 9: Commit**

```bash
git add src/kanban/kanban_db.cpp tests/test_kanban_db.cpp
git commit -m "feat: add_task/update_task/update_task_status record history; fix test call sites"
```

---

## Task 5: kanban_db — archive_task and unarchive_task

**Files:**
- Modify: `src/kanban/kanban_db.cpp`
- Modify: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Implement `archive_task` and `unarchive_task`**

Add to `src/kanban/kanban_db.cpp`:

```cpp
void kanban_db::archive_task(long long id, const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
    if(results.empty())
        return;
    Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
    if(p->is_archived)
        return;
    p.modify()->is_archived = 1;
    record_event(id, actor, "archived", {});
}

void kanban_db::unarchive_task(long long id, const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
    if(results.empty())
        return;
    Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
    if(!p->is_archived)
        return;
    p.modify()->is_archived = 0;
    record_event(id, actor, "unarchived", {});
}
```

- [ ] **Step 2: Implement `archived_tasks_for_team`**

```cpp
std::vector<kanban_task_entry> kanban_db::archived_tasks_for_team(long long team_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results = m_dbo.find<kanban_task_record>()
                           .where("team_id = ? AND is_archived = 1")
                           .bind(team_id)
                           .orderBy("id DESC")
                           .resultList();
    std::vector<kanban_task_entry> out;
    for(const auto& p: results)
        out.push_back(to_entry(p));
    return out;
}
```

- [ ] **Step 3: Add tests**

```cpp
TEST_CASE("kanban_db - archive_task records archived event and hides from active list")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id  = db.add_task(make_task(tid, "Work"), "alice");
    db.archive_task(id, "alice");
    CHECK(db.tasks_for_team(tid).empty());
    const auto archived = db.archived_tasks_for_team(tid);
    REQUIRE(archived.size() == 1);
    CHECK(archived[0].title == "Work");
    const auto hist = db.history_for_task(id);
    CHECK(hist[0].event_type == "archived");
    CHECK(hist[0].actor == "alice");
}

TEST_CASE("kanban_db - unarchive_task restores task to active list")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    const long long id  = db.add_task(make_task(tid, "Work"), "alice");
    db.archive_task(id, "alice");
    db.unarchive_task(id, "bob");
    REQUIRE(db.tasks_for_team(tid).size() == 1);
    CHECK(db.archived_tasks_for_team(tid).empty());
    const auto hist = db.history_for_task(id);
    CHECK(hist[0].event_type == "unarchived");
    CHECK(hist[0].actor == "bob");
}
```

- [ ] **Step 4: Build and run tests**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
cd build && ctest -R test_kanban_db --output-on-failure
```

Expected: all passing.

- [ ] **Step 5: Commit**

```bash
git add src/kanban/kanban_db.cpp tests/test_kanban_db.cpp
git commit -m "feat: archive_task, unarchive_task, archived_tasks_for_team"
```

---

## Task 6: kanban_db — archive_team, unarchive_team, teams_for_org filter

**Files:**
- Modify: `src/kanban/kanban_db.cpp`
- Modify: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Implement `archive_team`**

Replace the existing `delete_team` implementation with `archive_team`:

```cpp
void kanban_db::archive_team(long long id, const std::string& actor)
{
    Wt::Dbo::Transaction t{m_dbo};

    // Archive team.
    const auto team_rows =
      m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
    if(team_rows.empty())
        return;
    (*team_rows.begin()).modify()->is_archived = 1;

    // Archive all tasks in the team (with history events).
    const auto task_rows = m_dbo.find<kanban_task_record>()
                             .where("team_id = ? AND is_archived = 0")
                             .bind(id)
                             .resultList();
    for(auto p: task_rows)
    {
        p.modify()->is_archived = 1;
        record_event(p.id(), actor, "archived", {});
    }
}
```

- [ ] **Step 2: Implement `unarchive_team`**

```cpp
void kanban_db::unarchive_team(long long id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           rows =
      m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
    if(!rows.empty())
        (*rows.begin()).modify()->is_archived = 0;
    // Tasks in the team are NOT automatically unarchived — they stay archived.
}
```

- [ ] **Step 3: Update `teams_for_org` and `all_teams` to filter archived**

```cpp
std::vector<team_entry> kanban_db::teams_for_org(long long org_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results = m_dbo.find<team_record>()
                           .where("org_id = ? AND is_archived = 0")  // ← filter
                           .bind(org_id)
                           .orderBy("id")
                           .resultList();
    std::vector<team_entry> out;
    for(const auto& p: results)
        out.push_back(to_entry(p));
    return out;
}

std::vector<team_entry> kanban_db::all_teams()
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results = m_dbo.find<team_record>()
                           .where("is_archived = 0")  // ← filter
                           .orderBy("id")
                           .resultList();
    std::vector<team_entry> out;
    for(const auto& p: results)
        out.push_back(to_entry(p));
    return out;
}
```

- [ ] **Step 4: Add a new method `archived_teams_for_org`**

Add to `kanban_db.hpp` public interface:

```cpp
std::vector<team_entry> archived_teams_for_org(long long org_id);
```

Implement in `kanban_db.cpp`:

```cpp
std::vector<team_entry> kanban_db::archived_teams_for_org(long long org_id)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results = m_dbo.find<team_record>()
                           .where("org_id = ? AND is_archived = 1")
                           .bind(org_id)
                           .orderBy("id")
                           .resultList();
    std::vector<team_entry> out;
    for(const auto& p: results)
        out.push_back(to_entry(p));
    return out;
}
```

- [ ] **Step 5: Update the existing delete_team test; add archive_team tests**

Replace the `"kanban_db - delete_team removes members and tasks"` test:

```cpp
TEST_CASE("kanban_db - archive_team hides team and archives all its tasks")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_member(tid, "alice");
    db.add_task(make_task(tid, "Chore"), "alice");
    db.archive_team(tid, "alice");

    // Team hidden from active list.
    CHECK(db.teams_for_org(1).empty());
    // Team still findable.
    const auto t = db.find_team(tid);
    REQUIRE(t.has_value());
    CHECK(t->is_archived);
    // Members preserved.
    CHECK(db.members_for_team(tid).size() == 1);
    // Tasks archived (hidden from active list, visible in archived list).
    CHECK(db.tasks_for_team(tid).empty());
    CHECK(db.archived_tasks_for_team(tid).size() == 1);
}

TEST_CASE("kanban_db - unarchive_team restores team, tasks stay archived")
{
    kanban_db       db{":memory:"};
    const long long tid = db.create_team("T", 1);
    db.add_task(make_task(tid, "Chore"), "alice");
    db.archive_team(tid, "alice");
    db.unarchive_team(tid);
    CHECK(db.teams_for_org(1).size() == 1);
    // Tasks still archived (individual unarchive required).
    CHECK(db.tasks_for_team(tid).empty());
    CHECK(db.archived_tasks_for_team(tid).size() == 1);
}
```

- [ ] **Step 6: Build and run tests**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
cd build && ctest -R test_kanban_db --output-on-failure
```

Expected: all passing.

- [ ] **Step 7: Commit**

```bash
git add src/kanban/kanban_db.hpp src/kanban/kanban_db.cpp tests/test_kanban_db.cpp
git commit -m "feat: archive_team, unarchive_team, archived_teams_for_org; filter teams_for_org"
```

---

## Task 7: org_db — is_archived migration and archive_org

**Files:**
- Modify: `src/org/org_db.hpp`
- Modify: `src/org/org_db.cpp`
- Modify: `tests/test_org_db.cpp`

- [ ] **Step 1: Update `org_db.hpp`**

Add declaration:

```cpp
void archive_org(long long id, const std::string& actor);
```

- [ ] **Step 2: Add migration and map `is_archived` in `org_db.cpp` constructor**

In `org_db::org_db`, the constructor already has `try { m_dbo.createTables(); } catch(...){}`. Add the migration after it (follow the same pattern as `kanban_db`):

```cpp
try
{
    Wt::Dbo::Transaction t{m_dbo};
    m_dbo.execute(
      "ALTER TABLE org ADD COLUMN is_archived INTEGER NOT NULL DEFAULT 0");
}
catch(...)
{}
```

- [ ] **Step 3: Update `to_entry` for `org_record`**

```cpp
org_entry org_db::to_entry(const Wt::Dbo::ptr<org_record>& p)
{
    return {.id = p.id(), .name = p->name, .is_archived = p->is_archived != 0};
}
```

- [ ] **Step 4: Filter `all_orgs` and `orgs_for_user`**

Find and update `all_orgs` to add `WHERE is_archived = 0`. Find and update `orgs_for_user` similarly. (Read the current implementations in `org_db.cpp` and add the `.where("is_archived = 0")` clause — the exact query structure matches the kanban pattern.)

- [ ] **Step 5: Implement `archive_org`**

```cpp
void org_db::archive_org(long long id, const std::string& /*actor*/)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results =
      m_dbo.find<org_record>().where("id = ?").bind(id).resultList();
    if(!results.empty())
        (*results.begin()).modify()->is_archived = 1;
    // Cascade to teams/tasks is the caller's responsibility.
}
```

- [ ] **Step 6: Add test to test_org_db.cpp**

Find where tests end in `tests/test_org_db.cpp` and append:

```cpp
TEST_CASE("org_db - archive_org hides org from active lists")
{
    org_db          db{":memory:"};
    const long long id = db.create_org("Acme", "alice");
    db.archive_org(id, "alice");
    CHECK(db.all_orgs().empty());
    // find_org still returns it.
    const auto found = db.find_org(id);
    REQUIRE(found.has_value());
    CHECK(found->is_archived);
}
```

- [ ] **Step 7: Build and run tests**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
cd build && ctest -R "test_kanban_db|test_org_db" --output-on-failure
```

Expected: all passing.

- [ ] **Step 8: Commit**

```bash
git add src/org/org_db.hpp src/org/org_db.cpp src/org/org.hpp tests/test_org_db.cpp
git commit -m "feat: add is_archived to org; archive_org; filter all_orgs and orgs_for_user"
```

---

## Task 8: kanban_board_page — thread actor and add Archived link

**Files:**
- Modify: `src/pages/kanban_board_page.hpp`
- Modify: `src/pages/kanban_board_page.cpp`

- [ ] **Step 1: Add `m_username` to `kanban_board_page.hpp`**

```cpp
private:
    kanban_db&                       m_db;
    std::string                      m_username;   // ← add
    long long                        m_team_id{0};
    // ... rest unchanged
```

- [ ] **Step 2: Store username and thread actor in `kanban_board_page.cpp`**

In the constructor, store the username (add after the `m_db{db}` member initialization or early in the constructor body):

```cpp
m_username = session.username;
```

Update the `on_move` lambda (currently at `m_db.update_task_status(tid, status, sort)`) to pass actor:

```cpp
[this](long long tid, const std::string& status, int sort) {
    m_db.update_task_status(tid, status, sort, m_username);  // ← actor added
    live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
    m_board_widget->refresh(m_db.tasks_for_team(m_team_id), m_is_lead, m_type_colors);
},
```

- [ ] **Step 3: Add "Archived" link in the board header (leads only)**

In the header section of `kanban_board_page.cpp`, after the existing "Manage Team" anchor (inside `if(m_is_lead)`):

```cpp
hdr->addNew<Wt::WAnchor>(
     Wt::WLink{Wt::LinkType::InternalPath,
               team_url + "/archive"},
     "Archived")
  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
```

- [ ] **Step 4: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/pages/kanban_board_page.hpp src/pages/kanban_board_page.cpp
git commit -m "feat: thread actor through update_task_status; add Archived link to board header"
```

---

## Task 9: kanban_team_page — archive_team and archived teams section

**Files:**
- Modify: `src/pages/kanban_team_page.hpp`
- Modify: `src/pages/kanban_team_page.cpp`

- [ ] **Step 1: Add `m_archived_teams_section` member to `kanban_team_page.hpp`**

```cpp
Wt::WContainerWidget* m_archived_teams_section{nullptr};  // ← add
```

Also add a private helper declaration:

```cpp
void refresh_archived_teams();
```

- [ ] **Step 2: Replace `delete_team` with `archive_team` in `build_team_block`**

In `src/pages/kanban_team_page.cpp`, find the `del_team` button (around line 322). Replace its `clicked()` handler:

```cpp
auto* del_team = hdr->addNew<Wt::WPushButton>("Archive team");  // label change
del_team->setStyleClass("link-action-btn link-delete-btn");
del_team->clicked().connect(
  [this, tid = team.id] {
      m_kdb.archive_team(tid, m_session.username);  // ← was delete_team(tid)
      live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
      live_hub::instance().broadcast("team:" + std::to_string(tid));
      refresh_teams();
      refresh_archived_teams();
  });
```

- [ ] **Step 3: Add collapsible "Archived Teams" section**

At the end of the `kanban_team_page` constructor (or at the end of `refresh_teams`), after the active teams section, add:

```cpp
// Archived teams (org leads only).
m_archived_teams_section = addNew<Wt::WContainerWidget>();
m_archived_teams_section->setStyleClass("kb-archived-section");
refresh_archived_teams();
```

- [ ] **Step 4: Implement `refresh_archived_teams`**

```cpp
void kanban_team_page::refresh_archived_teams()
{
    m_archived_teams_section->clear();
    const auto archived = m_kdb.archived_teams_for_org(m_org_id);
    if(archived.empty())
        return;

    auto* hdr = m_archived_teams_section->addNew<Wt::WContainerWidget>();
    hdr->setStyleClass("kb-section-hdr");
    hdr->addNew<Wt::WText>("Archived Teams", Wt::TextFormat::Plain);

    for(const auto& team: archived)
    {
        auto* row = m_archived_teams_section->addNew<Wt::WContainerWidget>();
        row->setStyleClass("kb-archived-team-row");
        row->addNew<Wt::WText>(team.name, Wt::TextFormat::Plain);

        auto* btn = row->addNew<Wt::WPushButton>("Unarchive");
        btn->setStyleClass("editor-btn");
        btn->clicked().connect(
          [this, tid = team.id] {
              m_kdb.unarchive_team(tid);
              live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
              refresh_teams();
              refresh_archived_teams();
          });
    }
}
```

- [ ] **Step 5: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add src/pages/kanban_team_page.hpp src/pages/kanban_team_page.cpp
git commit -m "feat: archive_team replaces delete_team; add archived teams section to team page"
```

---

## Task 10: kanban_task_editor_page — tabs, read-only mode, archive/unarchive, history tab

This task restructures the existing editor significantly. Read `src/pages/kanban_task_editor_page.cpp` fully before editing.

**Files:**
- Modify: `src/pages/kanban_task_editor_page.hpp`
- Modify: `src/pages/kanban_task_editor_page.cpp`

- [ ] **Step 1: Update `kanban_task_editor_page.hpp` and add WTabWidget include to `.cpp`**

Add to `src/pages/kanban_task_editor_page.hpp`:

```cpp
#include <Wt/WTabWidget.h>
```

Also add to `src/pages/kanban_task_editor_page.cpp` (top of file, with other includes):

```cpp
#include <Wt/WTabWidget.h>
```

Replace `m_del_btn` with:

```cpp
Wt::WPushButton*      m_archive_btn{nullptr};   // was m_del_btn
Wt::WContainerWidget* m_history_panel{nullptr};
```

Add private helper:

```cpp
void populate_history();
```

Remove `m_del_btn` member.

- [ ] **Step 2: Restructure constructor — wrap form in WTabWidget**

In `src/pages/kanban_task_editor_page.cpp`, after adding the `<h1>` heading and before creating the form container, insert:

```cpp
auto* tabs_widget = addNew<Wt::WTabWidget>();
tabs_widget->setStyleClass("kb-editor-tabs");

// Tab 0: Details
auto form_uptr = std::make_unique<Wt::WContainerWidget>();
auto* form     = form_uptr.get();
tabs_widget->addTab(std::move(form_uptr), "Details");
form->setStyleClass("kb-editor-form");
```

Remove `auto* form = addNew<Wt::WContainerWidget>(); form->setStyleClass("kb-editor-form");` (the old line).

All subsequent `form->addNew<...>` calls remain unchanged — they now add to the tab's form container.

- [ ] **Step 3: Handle read-only mode for archived tasks**

After the form widgets are built (before the buttons), add:

```cpp
const bool is_archived = existing && existing->is_archived;
```

In the buttons section, replace the Delete button block with:

```cpp
if(existing && is_lead)
{
    if(is_archived)
    {
        m_archive_btn = btn_row->addNew<Wt::WPushButton>("Unarchive");
        m_archive_btn->setStyleClass("editor-btn");
        m_archive_btn->clicked().connect(
          [this] {
              m_db.unarchive_task(m_existing->id, m_username);
              live_hub::instance().broadcast(
                "team:" + std::to_string(m_team_id));
              m_on_save();
          });
    }
    else
    {
        m_archive_btn = btn_row->addNew<Wt::WPushButton>("Archive");
        m_archive_btn->setStyleClass("editor-btn editor-btn-danger");
        m_archive_btn->clicked().connect(
          [this] {
              const long long tid = m_existing->id;
              if(m_task_id != 0)
              {
                  live_hub::instance().unsubscribe(
                    "task:" + std::to_string(m_task_id), m_session_id);
                  m_task_id = 0;
              }
              m_db.archive_task(tid, m_username);
              live_hub::instance().broadcast(
                "team:" + std::to_string(m_team_id));
              live_hub::instance().broadcast("task:" + std::to_string(tid));
              m_on_save();
          });
    }
}
```

For archived tasks, disable all form fields and hide Save:

```cpp
if(is_archived)
{
    m_title->setReadOnly(true);
    m_description->setReadOnly(true);
    m_status->setDisabled(true);
    m_assigned_to->setDisabled(true);
    m_start_date->setDisabled(true);
    m_end_date->setDisabled(true);
    for(auto* c: m_type_chips)
        c->setDisabled(true);
    if(m_save_btn)
        m_save_btn->hide();
}
```

- [ ] **Step 4: Pass actor to add_task and update_task in `save()`**

In `kanban_task_editor_page::save()`:

```cpp
m_db.update_task(t, m_username);   // was: m_db.update_task(t)
// ...
t.id = m_db.add_task(t, m_username);  // was: t.id = m_db.add_task(t)
```

- [ ] **Step 5: Add History tab (existing tasks only)**

After building Tab 0 (Details), add:

```cpp
if(existing)
{
    auto hist_uptr = std::make_unique<Wt::WContainerWidget>();
    m_history_panel = hist_uptr.get();
    m_history_panel->setStyleClass("kb-history-panel");
    const int hist_tab_idx =
      tabs_widget->addTab(std::move(hist_uptr), "History");

    tabs_widget->currentChanged().connect(
      [this, hist_tab_idx](int idx) {
          if(idx == hist_tab_idx)
              populate_history();
      });
}
```

- [ ] **Step 6: Implement `populate_history`**

Add a static file-local helper at the top of `kanban_task_editor_page.cpp`:

```cpp
static std::string format_occurred_at(const std::string& iso)
{
    static const char* months[] = {
      "Jan","Feb","Mar","Apr","May","Jun",
      "Jul","Aug","Sep","Oct","Nov","Dec"};
    if(iso.size() < 16)
        return iso;
    try
    {
        const int year  = std::stoi(iso.substr(0, 4));
        const int month = std::stoi(iso.substr(5, 2));
        const int day   = std::stoi(iso.substr(8, 2));
        const std::string time = iso.substr(11, 5);
        if(month < 1 || month > 12)
            return iso;
        return std::string(months[month - 1]) + " " + std::to_string(day) +
               ", " + std::to_string(year) + " at " + time;
    }
    catch(...)
    {
        return iso;
    }
}

static std::string status_label(const std::string& s)
{
    if(s == "todo")        return "To Do";
    if(s == "in_progress") return "In Progress";
    if(s == "review")      return "Review";
    if(s == "done")        return "Done";
    return s;
}

static std::string field_label(const std::string& f)
{
    if(f == "status")      return "Status";
    if(f == "title")       return "Title";
    if(f == "description") return "Description";
    if(f == "assigned_to") return "Assigned to";
    if(f == "start_date")  return "Start date";
    if(f == "end_date")    return "End date";
    if(f == "type")        return "Type";
    return f;
}

static std::string display_value(const std::string& field, const std::string& val)
{
    if(val.empty())
        return "(unset)";
    if(field == "status")
        return status_label(val);
    return val;
}
```

Add to `kanban_task_editor_page.cpp`:

```cpp
void kanban_task_editor_page::populate_history()
{
    if(!m_history_panel)
        return;
    m_history_panel->clear();

    const auto events = m_db.history_for_task(m_existing->id);
    if(events.empty())
    {
        m_history_panel->addNew<Wt::WText>("No history yet.", Wt::TextFormat::Plain);
        return;
    }

    for(const auto& ev: events)
    {
        auto* block = m_history_panel->addNew<Wt::WContainerWidget>();
        block->setStyleClass("kb-history-event");

        const std::string header_text =
          format_occurred_at(ev.occurred_at) + "  \xe2\x80\x94  " + ev.actor;
        block->addNew<Wt::WText>(header_text, Wt::TextFormat::Plain)
          ->setStyleClass("kb-history-hdr");

        if(ev.event_type == "created")
        {
            block->addNew<Wt::WText>("[Task created]", Wt::TextFormat::Plain)
              ->setStyleClass("kb-history-meta");
        }
        else if(ev.event_type == "archived")
        {
            block->addNew<Wt::WText>("[Task archived]", Wt::TextFormat::Plain)
              ->setStyleClass("kb-history-meta");
        }
        else if(ev.event_type == "unarchived")
        {
            block->addNew<Wt::WText>("[Task unarchived]", Wt::TextFormat::Plain)
              ->setStyleClass("kb-history-meta");
        }
        else
        {
            for(const auto& ch: ev.changes)
            {
                const std::string line =
                  field_label(ch.field_name) + ": " +
                  display_value(ch.field_name, ch.old_value) +
                  " \xe2\x86\x92 " +
                  display_value(ch.field_name, ch.new_value);
                block->addNew<Wt::WText>(line, Wt::TextFormat::Plain)
                  ->setStyleClass("kb-history-field");
            }
        }
    }
}
```

- [ ] **Step 7: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 8: Commit**

```bash
git add src/pages/kanban_task_editor_page.hpp src/pages/kanban_task_editor_page.cpp
git commit -m "feat: task editor gains History tab, Archive/Unarchive button, read-only archived mode"
```

---

## Task 11: kanban_archive_page and altinf_app route

**Files:**
- Create: `src/pages/kanban_archive_page.hpp`
- Create: `src/pages/kanban_archive_page.cpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Create `kanban_archive_page.hpp`**

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"

#include <Wt/WContainerWidget.h>

#include <string>

class kanban_archive_page: public Wt::WContainerWidget
{
public:
    kanban_archive_page(kanban_db&          db,
                        const session_data& session,
                        long long           team_id);
};
```

- [ ] **Step 2: Create `kanban_archive_page.cpp`**

```cpp
#include "kanban_archive_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

kanban_archive_page::kanban_archive_page(kanban_db&          db,
                                         const session_data& /*session*/,
                                         long long           team_id)
{
    setStyleClass("page kb-archive-page");

    const auto team = db.find_team(team_id);
    const std::string team_url = "/board/" + std::to_string(team_id);

    const std::string heading =
      team ? "Archived Tasks \xe2\x80\x94 " + team->name : "Archived Tasks";
    addNew<Wt::WText>("<h1>" + heading + "</h1>", Wt::TextFormat::UnsafeXHTML);

    addNew<Wt::WAnchor>(
          Wt::WLink{Wt::LinkType::InternalPath, team_url}, "\xe2\x86\x90 Back to board")
      ->setStyleClass("editor-btn editor-btn-cancel");

    const auto tasks = db.archived_tasks_for_team(team_id);
    if(tasks.empty())
    {
        addNew<Wt::WText>("No archived tasks.", Wt::TextFormat::Plain)
          ->setStyleClass("kb-empty-msg");
        return;
    }

    auto* list = addNew<Wt::WContainerWidget>();
    list->setStyleClass("kb-archive-list");

    static const auto status_label = [](const std::string& s) -> std::string {
        if(s == "todo")        return "To Do";
        if(s == "in_progress") return "In Progress";
        if(s == "review")      return "Review";
        if(s == "done")        return "Done";
        return s;
    };

    for(const auto& task: tasks)
    {
        auto* row = list->addNew<Wt::WContainerWidget>();
        row->setStyleClass("kb-archive-row");

        const std::string task_url =
          "/board/" + std::to_string(team_id) +
          "/task/" + std::to_string(task.id) + "/edit";

        auto* link = row->addNew<Wt::WAnchor>(
          Wt::WLink{Wt::LinkType::InternalPath, task_url},
          task.title);
        link->setStyleClass("kb-archive-title");

        row->addNew<Wt::WText>(
              " (" + status_label(task.status) + ")",
              Wt::TextFormat::Plain)
          ->setStyleClass("kb-archive-status");
    }
}
```

- [ ] **Step 3: Add CMakeLists entry for kanban_archive_page**

In the main `CMakeLists.txt`, find the `altinf` executable's source list and add:

```cmake
src/pages/kanban_archive_page.cpp
```

- [ ] **Step 4: Add route in `altinf_app.cpp`**

In the `/board/` routing block, add a new `else if` branch before the final `else { show_not_found(); }`:

```cpp
else if(suffix == "/archive")
{
    if(!is_lead)
    {
        show_forbidden();
        return;
    }
    m_content->addNew<kanban_archive_page>(
      *m_kanban_db, m_session, team_id);
}
```

Add the include at the top of `altinf_app.cpp`:

```cpp
#include "pages/kanban_archive_page.hpp"
```

- [ ] **Step 5: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add src/pages/kanban_archive_page.hpp src/pages/kanban_archive_page.cpp \
        src/altinf_app.cpp CMakeLists.txt
git commit -m "feat: add kanban_archive_page and /board/{id}/archive route"
```

---

## Task 12: E2E tests

**Files:**
- Modify: `e2e/` (add new test file or append to existing spec)

- [ ] **Step 1: Identify or create the Playwright spec file**

Check what spec files exist:

```bash
ls e2e/tests/
```

Create `e2e/tests/task-history.spec.ts` (or `.js` if the project uses JS specs — match the existing convention).

- [ ] **Step 2: Write E2E tests**

```typescript
import { test, expect } from '@playwright/test';

// Assumes a logged-in lead session is available via storageState or login helper.
// Adjust baseURL, credentials, and team navigation to match existing E2E conventions.

test.describe('Task history', () => {
  test('history tab shows created event after task creation', async ({ page }) => {
    // Navigate to a team board as a lead, create a new task.
    await page.goto('/board/1/task/new');
    await page.fill('[placeholder="Task title"]', 'History Test Task');
    await page.click('button:has-text("Create Task")');

    // Find the task on the board and open it.
    await page.waitForSelector('.kb-card:has-text("History Test Task")');
    await page.click('.kb-card:has-text("History Test Task")');
    await page.waitForURL('**/edit');

    // Switch to History tab.
    await page.click('text=History');
    await expect(page.locator('.kb-history-event')).toHaveCount(1);
    await expect(page.locator('.kb-history-meta')).toContainText('[Task created]');
  });

  test('history tab records field changes on save', async ({ page }) => {
    await page.goto('/board/1/task/new');
    await page.fill('[placeholder="Task title"]', 'Diff Test Task');
    await page.click('button:has-text("Create Task")');
    await page.waitForSelector('.kb-card:has-text("Diff Test Task")');
    await page.click('.kb-card:has-text("Diff Test Task")');
    await page.waitForURL('**/edit');

    // Edit title and save.
    await page.fill('[placeholder="Task title"]', 'Diff Test Task Updated');
    await page.click('button:has-text("Save Changes")');
    await page.waitForURL('/board/1');

    // Reopen and check history.
    await page.click('.kb-card:has-text("Diff Test Task Updated")');
    await page.waitForURL('**/edit');
    await page.click('text=History');
    await expect(page.locator('.kb-history-event')).toHaveCount(2);
    await expect(page.locator('.kb-history-field').first())
      .toContainText('Diff Test Task');
  });

  test('Archive button hides task from board', async ({ page }) => {
    await page.goto('/board/1/task/new');
    await page.fill('[placeholder="Task title"]', 'To Archive');
    await page.click('button:has-text("Create Task")');
    await page.waitForSelector('.kb-card:has-text("To Archive")');
    await page.click('.kb-card:has-text("To Archive")');
    await page.waitForURL('**/edit');

    await page.click('button:has-text("Archive")');
    await page.waitForURL('/board/1');
    await expect(page.locator('.kb-card:has-text("To Archive")')).toHaveCount(0);
  });

  test('archived task appears on archive page in read-only mode', async ({ page }) => {
    // Navigate to the archive page.
    await page.goto('/board/1/archive');
    await expect(page.locator('.kb-archive-title:has-text("To Archive")')).toBeVisible();

    // Open the archived task.
    await page.click('.kb-archive-title:has-text("To Archive")');
    await page.waitForURL('**/edit');

    // Save button should be hidden; Unarchive button visible.
    await expect(page.locator('button:has-text("Save Changes")')).toHaveCount(0);
    await expect(page.locator('button:has-text("Unarchive")')).toBeVisible();

    // Fields should be read-only.
    await expect(page.locator('[placeholder="Task title"]')).toBeDisabled();
  });

  test('Unarchive restores task to board', async ({ page }) => {
    await page.goto('/board/1/archive');
    await page.click('.kb-archive-title:has-text("To Archive")');
    await page.waitForURL('**/edit');
    await page.click('button:has-text("Unarchive")');
    await page.waitForURL('/board/1');
    await expect(page.locator('.kb-card:has-text("To Archive")')).toBeVisible();
  });

  test('archived teams section appears in team management for org lead', async ({ page }) => {
    // Archive a team via the team management page.
    await page.goto('/board/1/manage');
    await page.click('button:has-text("Archive team")');
    await expect(page.locator('text=Archived Teams')).toBeVisible();
    await expect(page.locator('.kb-archived-team-row')).toHaveCount(1);
  });
});
```

- [ ] **Step 3: Run all three test suites**

```bash
cd build && ctest --output-on-failure
cd /home/altainia/code/altinf/e2e && npx playwright test tests/task-history.spec.ts
```

Fix any failures before proceeding.

- [ ] **Step 4: Commit**

```bash
git add e2e/tests/task-history.spec.ts
git commit -m "test: add E2E tests for task history, archive/unarchive, archive page"
```
