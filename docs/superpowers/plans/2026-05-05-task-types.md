# Task Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Introduce org-scoped task types — each type has a name and color; a task's board/Gantt color comes entirely from its type (gray if no type); org leads manage types on a dedicated page.

**Architecture:** `task_type` table in `kanban_db`; `kanban_task` replaces `color` with `type_id`; board/Gantt widgets receive a `map<long long,string>` to resolve colors at render time; task editor shows a labeled chip selector; a new `org_type_manager_page` handles CRUD.

**Tech Stack:** C++17, Wt 4.x (Dbo ORM, WContainerWidget, WLineEdit, WComboBox, WColorPicker), SQLite, SCSS, Catch2, Playwright

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/kanban/kanban.hpp` | Modify | Add `task_type_record`/`task_type_entry`; replace `color` with `type_id` in task structs |
| `src/kanban/kanban_db.hpp` | Modify | Declare type CRUD methods |
| `src/kanban/kanban_db.cpp` | Modify | Register table, implement type CRUD, update task CRUD |
| `src/kanban/kanban_board_widget.hpp` | Modify | Add `type_colors` map param |
| `src/kanban/kanban_board_widget.cpp` | Modify | Store map, resolve color in serializer |
| `src/kanban/gantt_view_widget.hpp` | Modify | Add `type_colors` map param |
| `src/kanban/gantt_view_widget.cpp` | Modify | Store map, resolve color in serializer |
| `src/pages/kanban_board_page.hpp` | Modify | Add `m_org_id`, `m_type_colors`, org-types subscription |
| `src/pages/kanban_board_page.cpp` | Modify | Build map, pass to widgets, subscribe to type changes |
| `src/pages/kanban_task_editor_page.hpp` | Modify | Replace WColorPicker with chip selector members |
| `src/pages/kanban_task_editor_page.cpp` | Modify | Build chip row, write `type_id` on save |
| `src/pages/org_board_page.cpp` | Modify | Pass `type_colors` to board widgets |
| `src/pages/org_landing_page.cpp` | Modify | Add "Manage Types" link for org leads |
| `src/pages/org_type_manager_page.hpp` | Create | New page declaration |
| `src/pages/org_type_manager_page.cpp` | Create | Type CRUD UI, palette default, live broadcast |
| `src/altinf_app.cpp` | Modify | Add `/org/{id}/types` route; pass types to task editor sites |
| `resources/scss/_kanban.scss` | Modify | Chip selector styles |
| `tests/test_kanban_db.cpp` | Modify | Remove color refs, add type CRUD tests |
| `e2e/specs/live-task-types.spec.ts` | Create | E2E coverage for type manager and task editor |

> `CMakeLists.txt` does **not** need editing — it uses `file(GLOB_RECURSE SOURCES src/*.cpp)`.

---

## Task 1: Update data model and fix all forced compilation errors

**Files:**
- Modify: `src/kanban/kanban.hpp`
- Modify: `src/kanban/kanban_db.cpp` (lines ~50–64, ~325–335, ~345–360)
- Modify: `src/kanban/kanban_board_widget.cpp` (line ~107, temporary placeholder)
- Modify: `src/kanban/gantt_view_widget.cpp` (line ~78, temporary placeholder)
- Modify: `tests/test_kanban_db.cpp` (lines 8–22, 187–197)

- [ ] **Step 1: Add type structs and change task structs in `kanban.hpp`**

  In `src/kanban/kanban.hpp`, add after `team_member_record` (before `kanban_task_record`):

  ```cpp
  struct task_type_record
  {
      long long   org_id{0};
      std::string name;
      std::string color;

      template<class Action>
      void persist(Action& a)
      {
          Wt::Dbo::field(a, org_id, "org_id");
          Wt::Dbo::field(a, name,   "name");
          Wt::Dbo::field(a, color,  "color");
      }
  };
  ```

  In `kanban_task_record`, replace:
  ```cpp
  std::string color;
  ```
  with:
  ```cpp
  long long type_id{0};
  ```
  And in its `persist`:
  ```cpp
  Wt::Dbo::field(a, color, "color");
  ```
  becomes:
  ```cpp
  Wt::Dbo::field(a, type_id, "type_id");
  ```

  In `kanban_task_entry`, replace:
  ```cpp
  std::string color;
  ```
  with:
  ```cpp
  long long type_id{0};
  ```

  Add `task_type_entry` after `team_member_entry`:
  ```cpp
  struct task_type_entry
  {
      long long   id{0};
      long long   org_id{0};
      std::string name;
      std::string color;
  };
  ```

- [ ] **Step 2: Fix `kanban_db.cpp` — `to_entry`, `add_task`, `update_task`**

  In `kanban_db::to_entry(const Wt::Dbo::ptr<kanban_task_record>& p)`, replace:
  ```cpp
  e.color       = p->color;
  ```
  with:
  ```cpp
  e.type_id     = p->type_id;
  ```

  Find `add_task` (around line 325) and replace:
  ```cpp
  p.modify()->color       = e.color;
  ```
  with:
  ```cpp
  p.modify()->type_id     = e.type_id;
  ```

  Find `update_task` (around line 353) and replace the same line the same way.

- [ ] **Step 3: Temporarily hardcode color in `kanban_board_widget.cpp`**

  Find line ~107 in `src/kanban/kanban_board_widget.cpp`:
  ```cpp
  << "\"color\":\"" << escape_json(t.color) << "\","
  ```
  Replace with:
  ```cpp
  << "\"color\":\"#cccccc\","
  ```
  (This is a placeholder — Task 3 replaces it with real type resolution.)

- [ ] **Step 4: Temporarily hardcode color in `gantt_view_widget.cpp`**

  Find line ~78 in `src/kanban/gantt_view_widget.cpp`:
  ```cpp
  << "\"color\":\"" << escape_json_gv(t.color) << "\","
  ```
  Replace with:
  ```cpp
  << "\"color\":\"#cccccc\","
  ```

- [ ] **Step 5: Fix `tests/test_kanban_db.cpp`**

  In `make_task` (lines 8–22), remove:
  ```cpp
  e.color      = "#7aa2d4";
  ```

  Replace the `"kanban_db - update task"` test case body with:
  ```cpp
  TEST_CASE("kanban_db - update task")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long id   = db.add_task(make_task(tid, "Original"));
      auto            task = *db.find_task(id);
      task.title           = "Updated";
      db.update_task(task);
      const auto updated = db.find_task(id);
      CHECK(updated->title == "Updated");
  }
  ```

- [ ] **Step 6: Build**

  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel $(nproc) 2>&1 | tail -20
  ```
  Expected: build succeeds with no errors.

- [ ] **Step 7: Run Catch2 tests**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -30
  ```
  Expected: all tests pass.

- [ ] **Step 8: Commit**

  ```bash
  git add src/kanban/kanban.hpp src/kanban/kanban_db.cpp \
          src/kanban/kanban_board_widget.cpp src/kanban/gantt_view_widget.cpp \
          tests/test_kanban_db.cpp
  git commit -m "refactor: replace task color field with type_id"
  ```

---

## Task 2: Add task type CRUD to `kanban_db` (TDD)

**Files:**
- Modify: `src/kanban/kanban_db.hpp`
- Modify: `src/kanban/kanban_db.cpp`
- Modify: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Write failing Catch2 tests**

  Append to `tests/test_kanban_db.cpp`:

  ```cpp
  // ---- task types ----

  TEST_CASE("kanban_db - create and retrieve task type")
  {
      kanban_db       db{":memory:"};
      const long long id = db.create_task_type(1, "Bug Fix", "#e07b54");
      CHECK(id > 0);
      const auto t = db.find_task_type(id);
      REQUIRE(t.has_value());
      CHECK(t->name == "Bug Fix");
      CHECK(t->color == "#e07b54");
      CHECK(t->org_id == 1);
  }

  TEST_CASE("kanban_db - types_for_org returns only org types")
  {
      kanban_db db{":memory:"};
      db.create_task_type(1, "Bug Fix", "#e07b54");
      db.create_task_type(1, "Feature", "#5a9e6f");
      db.create_task_type(2, "Other",   "#7aa2d4");
      const auto org1 = db.types_for_org(1);
      REQUIRE(org1.size() == 2);
      const auto org2 = db.types_for_org(2);
      REQUIRE(org2.size() == 1);
  }

  TEST_CASE("kanban_db - types_for_org empty when no types")
  {
      kanban_db db{":memory:"};
      CHECK(db.types_for_org(99).empty());
  }

  TEST_CASE("kanban_db - update_task_type changes name and color")
  {
      kanban_db       db{":memory:"};
      const long long id = db.create_task_type(1, "Old", "#aaaaaa");
      db.update_task_type(id, "New", "#bbbbbb");
      const auto t = db.find_task_type(id);
      REQUIRE(t.has_value());
      CHECK(t->name  == "New");
      CHECK(t->color == "#bbbbbb");
  }

  TEST_CASE("kanban_db - delete_task_type zeroes out affected tasks")
  {
      kanban_db       db{":memory:"};
      const long long team_id = db.create_team("T", 1);
      const long long type_id = db.create_task_type(1, "Bug Fix", "#e07b54");

      kanban_task_entry e;
      e.team_id = team_id;
      e.title   = "A task";
      e.type_id = type_id;
      const long long tid = db.add_task(e);

      db.delete_task_type(type_id);

      CHECK(!db.find_task_type(type_id).has_value());
      CHECK(db.find_task(tid)->type_id == 0);
  }

  TEST_CASE("kanban_db - find_task_type missing returns nullopt")
  {
      kanban_db db{":memory:"};
      CHECK(!db.find_task_type(9999).has_value());
  }

  TEST_CASE("kanban_db - task type_id persists through add and find")
  {
      kanban_db       db{":memory:"};
      const long long team_id = db.create_team("T", 1);
      const long long type_id = db.create_task_type(1, "Feature", "#5a9e6f");

      kanban_task_entry e;
      e.team_id = team_id;
      e.title   = "My task";
      e.type_id = type_id;
      const long long id = db.add_task(e);

      const auto found = db.find_task(id);
      REQUIRE(found.has_value());
      CHECK(found->type_id == type_id);
  }

  TEST_CASE("kanban_db - update_task preserves type_id change")
  {
      kanban_db       db{":memory:"};
      const long long team_id = db.create_team("T", 1);
      const long long type_id = db.create_task_type(1, "Feature", "#5a9e6f");

      kanban_task_entry e;
      e.team_id = team_id;
      e.title   = "My task";
      e.type_id = 0;
      const long long id = db.add_task(e);

      auto task    = *db.find_task(id);
      task.type_id = type_id;
      db.update_task(task);

      CHECK(db.find_task(id)->type_id == type_id);
  }
  ```

- [ ] **Step 2: Build — expect compile failure**

  ```bash
  cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -10
  ```
  Expected: errors like `'create_task_type' is not a member of 'kanban_db'`.

- [ ] **Step 3: Add declarations to `kanban_db.hpp`**

  Inside the `class kanban_db` declaration, add a new section after the Tasks section:

  ```cpp
  // Task types
  long long                        create_task_type(long long org_id,
                                                    const std::string& name,
                                                    const std::string& color);
  void                             update_task_type(long long          id,
                                                    const std::string& name,
                                                    const std::string& color);
  void                             delete_task_type(long long id);
  std::vector<task_type_entry>     types_for_org(long long org_id);
  std::optional<task_type_entry>   find_task_type(long long id);
  ```

  Also add `static task_type_entry to_entry(const Wt::Dbo::ptr<task_type_record>& p);` to the private section.

- [ ] **Step 4: Register the table and add ALTER TABLE in `kanban_db.cpp` constructor**

  In the constructor, add `mapClass` call alongside the existing ones:
  ```cpp
  m_dbo.mapClass<task_type_record>("task_type");
  ```
  (Add it after `m_dbo.mapClass<kanban_task_record>("kanban_task");`.)

  Add a migration block after the existing ALTER TABLE blocks:
  ```cpp
  try
  {
      Wt::Dbo::Transaction t{m_dbo};
      m_dbo.execute("ALTER TABLE kanban_task ADD COLUMN type_id INTEGER NOT NULL DEFAULT 0");
  }
  catch(...)
  {}
  ```

- [ ] **Step 5: Implement type CRUD in `kanban_db.cpp`**

  After the existing `to_entry` for teams/members, add:

  ```cpp
  task_type_entry kanban_db::to_entry(const Wt::Dbo::ptr<task_type_record>& p)
  {
      return {.id = p.id(), .org_id = p->org_id, .name = p->name, .color = p->color};
  }
  ```

  Add the implementations (place them before the permission helpers):

  ```cpp
  // ---- Task types ----

  long long kanban_db::create_task_type(long long          org_id,
                                        const std::string& name,
                                        const std::string& color)
  {
      Wt::Dbo::Transaction t{m_dbo};
      auto p           = m_dbo.add(std::make_unique<task_type_record>());
      p.modify()->org_id = org_id;
      p.modify()->name   = name;
      p.modify()->color  = color;
      m_dbo.flush();
      return p.id();
  }

  void kanban_db::update_task_type(long long          id,
                                   const std::string& name,
                                   const std::string& color)
  {
      Wt::Dbo::Transaction t{m_dbo};
      auto p = m_dbo.load<task_type_record>(id);
      p.modify()->name  = name;
      p.modify()->color = color;
  }

  void kanban_db::delete_task_type(long long id)
  {
      Wt::Dbo::Transaction t{m_dbo};
      m_dbo.execute("UPDATE kanban_task SET type_id=0 WHERE type_id=?").bind(id);
      auto p = m_dbo.load<task_type_record>(id);
      p.remove();
  }

  std::vector<task_type_entry> kanban_db::types_for_org(long long org_id)
  {
      Wt::Dbo::Transaction t{m_dbo};
      const auto results = m_dbo.find<task_type_record>()
                               .where("org_id = ?")
                               .bind(org_id)
                               .orderBy("id")
                               .resultList();
      std::vector<task_type_entry> out;
      for(const auto& p: results)
          out.push_back(to_entry(p));
      return out;
  }

  std::optional<task_type_entry> kanban_db::find_task_type(long long id)
  {
      Wt::Dbo::Transaction t{m_dbo};
      try
      {
          auto p = m_dbo.load<task_type_record>(id);
          return to_entry(p);
      }
      catch(const Wt::Dbo::ObjectNotFoundException&)
      {
          return std::nullopt;
      }
  }
  ```

- [ ] **Step 6: Build**

  ```bash
  cmake --build build --parallel $(nproc) 2>&1 | tail -20
  ```
  Expected: succeeds.

- [ ] **Step 7: Run Catch2 tests**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -30
  ```
  Expected: all tests pass including the new type CRUD tests.

- [ ] **Step 8: Commit**

  ```bash
  git add src/kanban/kanban.hpp src/kanban/kanban_db.hpp src/kanban/kanban_db.cpp \
          tests/test_kanban_db.cpp
  git commit -m "feat: add task type CRUD to kanban_db"
  ```

---

## Task 3: Color resolution in board and Gantt widgets

**Files:**
- Modify: `src/kanban/kanban_board_widget.hpp`
- Modify: `src/kanban/kanban_board_widget.cpp`
- Modify: `src/kanban/gantt_view_widget.hpp`
- Modify: `src/kanban/gantt_view_widget.cpp`
- Modify: `src/pages/kanban_board_page.hpp`
- Modify: `src/pages/kanban_board_page.cpp`
- Modify: `src/pages/org_board_page.cpp`

- [ ] **Step 1: Update `kanban_board_widget.hpp`**

  Add `#include <map>` (if not present).

  Change the constructor signature:
  ```cpp
  kanban_board_widget(std::vector<kanban_task_entry>                          tasks,
                      bool                                                    can_edit,
                      std::map<long long, std::string>                        type_colors,
                      std::function<void(long long, const std::string&, int)> on_move,
                      std::function<void(long long)>                          on_edit);
  ```

  Change `refresh`:
  ```cpp
  void refresh(std::vector<kanban_task_entry> tasks, bool can_edit,
               std::map<long long, std::string> type_colors);
  ```

  Add private member:
  ```cpp
  std::map<long long, std::string> m_type_colors;
  ```

  Change `serialize_tasks` to non-static (remove `static`):
  ```cpp
  std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
  ```

- [ ] **Step 2: Update `kanban_board_widget.cpp`**

  Change the constructor to accept and store `type_colors`:
  ```cpp
  kanban_board_widget::kanban_board_widget(
    std::vector<kanban_task_entry>                          tasks,
    bool                                                    can_edit,
    std::map<long long, std::string>                        type_colors,
    std::function<void(long long, const std::string&, int)> on_move,
    std::function<void(long long)>                          on_edit):
    m_type_colors{std::move(type_colors)}
  ```
  (Add the member initializer; keep the rest of the constructor body unchanged.)

  Change `refresh` to:
  ```cpp
  void kanban_board_widget::refresh(std::vector<kanban_task_entry> tasks,
                                    bool                           can_edit,
                                    std::map<long long, std::string> type_colors)
  {
      m_type_colors = std::move(type_colors);
      init_js(serialize_tasks(tasks), can_edit);
  }
  ```

  In `serialize_tasks`, replace the temporary `"#cccccc"` placeholder with:
  ```cpp
  const auto        it    = m_type_colors.find(t.type_id);
  const std::string color = (it != m_type_colors.end()) ? it->second : "#cccccc";
  << "\"color\":\"" << escape_json(color) << "\","
  ```

- [ ] **Step 3: Update `gantt_view_widget.hpp`**

  Add `#include <map>` (if not present).

  Change the constructor:
  ```cpp
  explicit gantt_view_widget(std::vector<kanban_task_entry>   tasks,
                              std::map<long long, std::string> type_colors);
  ```

  Change `refresh`:
  ```cpp
  void refresh(std::vector<kanban_task_entry>   tasks,
               std::map<long long, std::string> type_colors);
  ```

  Add private member and change `serialize_tasks` to non-static:
  ```cpp
  std::map<long long, std::string> m_type_colors;
  std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
  ```

- [ ] **Step 4: Update `gantt_view_widget.cpp`**

  Change the constructor to accept and store `type_colors`:
  ```cpp
  gantt_view_widget::gantt_view_widget(
    std::vector<kanban_task_entry>   tasks,
    std::map<long long, std::string> type_colors):
    m_type_colors{std::move(type_colors)}
  ```

  Change `refresh` to:
  ```cpp
  void gantt_view_widget::refresh(std::vector<kanban_task_entry>   tasks,
                                  std::map<long long, std::string> type_colors)
  {
      m_type_colors = std::move(type_colors);
      doJavaScript("initGantt('" + id() + "'," + serialize_tasks(tasks) + ");");
  }
  ```

  In `serialize_tasks`, replace the temporary `"#cccccc"` placeholder with:
  ```cpp
  const auto        it    = m_type_colors.find(t.type_id);
  const std::string color = (it != m_type_colors.end()) ? it->second : "#cccccc";
  << "\"color\":\"" << escape_json_gv(color) << "\","
  ```

- [ ] **Step 5: Update `kanban_board_page.hpp`**

  Add `#include <map>` (if not present).

  Add private members:
  ```cpp
  long long                        m_org_id{0};
  std::map<long long, std::string> m_type_colors;
  ```

  Update the unsubscribe session id member — add a second session tag for types channel (they can reuse the same `m_session_id`).

- [ ] **Step 6: Update `kanban_board_page.cpp`**

  Add a private helper before the constructor (or inline in the constructor):

  In the constructor, after `find_team(team_id)` succeeds, store org_id and build type_colors:
  ```cpp
  m_org_id    = team->org_id;
  const auto type_vec = db.types_for_org(m_org_id);
  for(const auto& ty : type_vec)
      m_type_colors[ty.id] = ty.color;
  ```

  Update both widget constructions to pass `m_type_colors`:
  ```cpp
  // Board:
  m_board_widget = addNew<kanban_board_widget>(
    tasks,
    m_is_lead,
    m_type_colors,
    [this](...) { ... },
    [this](...) { ... });

  // Gantt:
  m_gantt_widget = addNew<gantt_view_widget>(tasks, m_type_colors);
  ```

  Add subscription to org types channel alongside the existing team subscription:
  ```cpp
  live_hub::instance().subscribe(
    "org:" + std::to_string(m_org_id) + ":types",
    m_session_id,
    [this, alive = m_alive] {
        if(*alive)
        {
            refresh();
            Wt::WApplication::instance()->triggerUpdate();
        }
    });
  ```

  In the destructor, also unsubscribe from the types channel:
  ```cpp
  if(m_org_id != 0)
  {
      live_hub::instance().unsubscribe(
        "org:" + std::to_string(m_org_id) + ":types", m_session_id);
  }
  ```

  Update `refresh()` to rebuild type colors and pass them:
  ```cpp
  void kanban_board_page::refresh()
  {
      m_type_colors.clear();
      for(const auto& ty : m_db.types_for_org(m_org_id))
          m_type_colors[ty.id] = ty.color;

      const auto tasks = m_db.tasks_for_team(m_team_id);
      if(m_show_gantt && m_gantt_widget)
          m_gantt_widget->refresh(tasks, m_type_colors);
      else if(m_board_widget)
          m_board_widget->refresh(tasks, m_is_lead, m_type_colors);
  }
  ```

- [ ] **Step 7: Update `org_board_page.cpp`**

  In the constructor, after `find_org` succeeds and before the teams loop, build type colors:
  ```cpp
  std::map<long long, std::string> type_colors;
  for(const auto& ty : kdb.types_for_org(org_id))
      type_colors[ty.id] = ty.color;
  ```

  Pass `type_colors` to each `kanban_board_widget`:
  ```cpp
  auto* board = section->addNew<kanban_board_widget>(
    tasks,
    true,
    type_colors,
    [&kdb, tid](long long task_id, const std::string& status, int sort) {
        kdb.update_task_status(task_id, status, sort);
    },
    [tid](long long task_id) {
        Wt::WApplication::instance()->setInternalPath(
          "/board/" + std::to_string(tid) + "/task/" +
            std::to_string(task_id) + "/edit", true);
    });
  ```

- [ ] **Step 8: Build**

  ```bash
  cmake --build build --parallel $(nproc) 2>&1 | tail -20
  ```
  Expected: succeeds.

- [ ] **Step 9: Run Catch2 and JS tests**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -20
  cd ../tests/js && node test_gantt.js 2>&1 | tail -10
  ```
  Expected: all pass.

- [ ] **Step 10: Commit**

  ```bash
  git add src/kanban/kanban_board_widget.hpp src/kanban/kanban_board_widget.cpp \
          src/kanban/gantt_view_widget.hpp src/kanban/gantt_view_widget.cpp \
          src/pages/kanban_board_page.hpp src/pages/kanban_board_page.cpp \
          src/pages/org_board_page.cpp
  git commit -m "feat: resolve task colors from type map in board and Gantt widgets"
  ```

---

## Task 4: Task editor chip selector

**Files:**
- Modify: `src/pages/kanban_task_editor_page.hpp`
- Modify: `src/pages/kanban_task_editor_page.cpp`
- Modify: `resources/scss/_kanban.scss`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Update `kanban_task_editor_page.hpp`**

  Add `#include <map>` (if not present).

  Change the constructor to add a `types` parameter:
  ```cpp
  kanban_task_editor_page(kanban_db&                          db,
                          org_db&                             odb,
                          long long                           team_id,
                          const session_data&                 session,
                          bool                                is_lead,
                          const kanban_task_entry*            existing,
                          const std::vector<std::string>&     members,
                          const std::vector<task_type_entry>& types,
                          std::function<void()>               on_save);
  ```

  Remove:
  ```cpp
  Wt::WColorPicker* m_color{nullptr};
  ```

  Add:
  ```cpp
  long long                           m_type_id{0};
  std::vector<Wt::WContainerWidget*>  m_type_chips;
  ```

- [ ] **Step 2: Rewrite the color section in `kanban_task_editor_page.cpp`**

  Remove `#include <Wt/WColor.h>` from the includes.

  Add `const std::vector<task_type_entry>& types` to the constructor parameter list.

  Find the `// ── Dates & color` section (~line 154). The color subsection starts at ~line 200 and looks like:
  ```cpp
  auto* color_wrap = sched_row->addNew<Wt::WContainerWidget>();
  color_wrap->setStyleClass("kb-editor-field-wrap");
  color_wrap->addNew<Wt::WText>("Color", Wt::TextFormat::Plain)
    ->setStyleClass("kb-field-label");
  m_color = color_wrap->addNew<Wt::WColorPicker>();
  ...
  m_color->setColor(Wt::WColor(cr, cg, cb));
  ```

  Delete that block entirely and replace the section label `"<h2>Schedule</h2>"` section heading with:
  > Keep the dates section unchanged. After the `sched_row` closes, add a new **Type** section immediately below it:

  ```cpp
  // ── Type ─────────────────────────────────────────────────────────────────
  form->addNew<Wt::WText>("<h2>Type</h2>", Wt::TextFormat::UnsafeXHTML);

  auto* type_row = form->addNew<Wt::WContainerWidget>();
  type_row->setStyleClass("kb-type-chips");

  // Determine which type_id to pre-select.
  m_type_id = 0;
  if(existing)
  {
      const bool valid_type = std::any_of(
        types.begin(), types.end(),
        [&](const task_type_entry& ty) { return ty.id == existing->type_id; });
      if(valid_type)
          m_type_id = existing->type_id;
  }

  // Helper lambda — builds one chip and appends it to m_type_chips.
  auto add_chip = [&](long long chip_type_id,
                      const std::string& label,
                      const std::string& hex)
  {
      auto* chip = type_row->addNew<Wt::WContainerWidget>();
      chip->setStyleClass(chip_type_id == m_type_id
                          ? "kb-type-chip selected"
                          : "kb-type-chip");

      auto* dot = chip->addNew<Wt::WContainerWidget>();
      dot->setStyleClass("kb-type-chip__dot");
      if(hex.size() == 7 && hex[0] == '#')
      {
          int r = std::stoi(hex.substr(1, 2), nullptr, 16);
          int g = std::stoi(hex.substr(3, 2), nullptr, 16);
          int b = std::stoi(hex.substr(5, 2), nullptr, 16);
          dot->decorationStyle().setBackgroundColor(Wt::WColor{r, g, b});
      }
      chip->addNew<Wt::WText>(label, Wt::TextFormat::Plain);
      m_type_chips.push_back(chip);

      chip->clicked().connect([this, chip, chip_type_id] {
          m_type_id = chip_type_id;
          for(auto* c : m_type_chips)
              c->removeStyleClass("selected");
          chip->addStyleClass("selected");
      });
  };

  add_chip(0, "(None)", "#cccccc");
  for(const auto& ty : types)
      add_chip(ty.id, ty.name, ty.color);
  ```

- [ ] **Step 3: Update `save()` in `kanban_task_editor_page.cpp`**

  Find the color block in `save()`:
  ```cpp
  const auto wc = m_color->color();
  char       cbuf[8];
  std::snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x", wc.red(), wc.green(), wc.blue());
  ```
  and the line:
  ```cpp
  t.color = cbuf;
  ```
  Delete both blocks. In place of `t.color = cbuf;`, write:
  ```cpp
  t.type_id = m_type_id;
  ```

- [ ] **Step 4: Add chip SCSS to `resources/scss/_kanban.scss`**

  Append at the end of the file:

  ```scss
  // ── Type chip selector ────────────────────────────────────────────────────
  .kb-type-chips {
    display:   flex;
    flex-wrap: wrap;
    gap:       6px;
  }

  .kb-type-chip {
    display:       flex;
    align-items:   center;
    gap:           6px;
    padding:       5px 10px;
    border-radius: 14px;
    border:        1px solid var(--color-border);
    cursor:        pointer;
    background:    var(--color-surface);
    user-select:   none;
    transition:    border-color 0.12s;

    &.selected {
      border-color: var(--color-accent);
      border-width: 2px;
      font-weight:  600;
    }

    &:hover:not(.selected) {
      border-color: var(--color-muted);
    }
  }

  .kb-type-chip__dot {
    width:         10px;
    height:        10px;
    border-radius: 50%;
    background:    #cccccc;
    flex-shrink:   0;
  }
  ```

- [ ] **Step 5: Update both task editor construction sites in `altinf_app.cpp`**

  For the **new task** site (~line 337):
  ```cpp
  const auto members = m_kanban_db->members_for_team(team_id);
  const auto types   = m_kanban_db->types_for_org(team->org_id);
  m_content->addNew<kanban_task_editor_page>(
    *m_kanban_db, *m_org_db, team_id, m_session, is_lead,
    nullptr, members, types,
    [this, team_id] {
        setInternalPath("/board/" + std::to_string(team_id), true);
    });
  ```

  For the **edit task** site (~line 364):
  ```cpp
  const auto members = m_kanban_db->members_for_team(team_id);
  const auto types   = m_kanban_db->types_for_org(team->org_id);
  m_content->addNew<kanban_task_editor_page>(
    *m_kanban_db, *m_org_db, team_id, m_session, is_lead,
    &(*m_edit_task), members, types,
    [this, team_id] {
        setInternalPath("/board/" + std::to_string(team_id), true);
    });
  ```

  Note: `team` is already available from `find_team(team_id)` at ~line 294 in both code paths.

- [ ] **Step 6: Build**

  ```bash
  cmake --build build --parallel $(nproc) 2>&1 | tail -20
  ```
  Expected: succeeds.

- [ ] **Step 7: Run all C++ and JS tests**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -20
  cd ../tests/js && node test_gantt.js 2>&1 | tail -10
  ```
  Expected: all pass.

- [ ] **Step 8: Commit**

  ```bash
  git add src/pages/kanban_task_editor_page.hpp \
          src/pages/kanban_task_editor_page.cpp \
          resources/scss/_kanban.scss \
          src/altinf_app.cpp
  git commit -m "feat: replace color picker with type chip selector in task editor"
  ```

---

## Task 5: Type manager page

**Files:**
- Create: `src/pages/org_type_manager_page.hpp`
- Create: `src/pages/org_type_manager_page.cpp`
- Modify: `resources/scss/_kanban.scss`

- [ ] **Step 1: Create `org_type_manager_page.hpp`**

  ```cpp
  #pragma once

  #include "auth/session_data.hpp"
  #include "kanban/kanban_db.hpp"
  #include "org/org_db.hpp"

  #include <Wt/WColorPicker.h>
  #include <Wt/WContainerWidget.h>
  #include <Wt/WLineEdit.h>
  #include <Wt/WText.h>

  #include <memory>

  class org_type_manager_page: public Wt::WContainerWidget
  {
  public:
      org_type_manager_page(kanban_db&          db,
                             org_db&             odb,
                             long long           org_id,
                             const session_data& session);

  private:
      kanban_db&  m_db;
      long long   m_org_id{0};

      Wt::WContainerWidget* m_list{nullptr};
      Wt::WLineEdit*        m_name_input{nullptr};
      Wt::WColorPicker*     m_color_picker{nullptr};
      Wt::WText*            m_status_msg{nullptr};

      long long             m_editing_id{0};
      Wt::WLineEdit*        m_edit_name{nullptr};
      Wt::WColorPicker*     m_edit_color{nullptr};

      static const std::vector<std::string> k_palette;

      void refresh_list();
      void add_type();
      void start_edit(long long type_id);
      void save_edit(long long type_id);
      void delete_type(long long type_id);
  };
  ```

- [ ] **Step 2: Create `org_type_manager_page.cpp`**

  ```cpp
  #include "org_type_manager_page.hpp"

  #include <Wt/WAnchor.h>
  #include <Wt/WApplication.h>
  #include <Wt/WColor.h>
  #include <Wt/WLink.h>
  #include <Wt/WPushButton.h>
  #include <Wt/WText.h>

  #include <cstdio>

  #include "widgets/live_hub.hpp"

  const std::vector<std::string> org_type_manager_page::k_palette = {
      "#e07b54", "#5a9e6f", "#7aa2d4", "#c47fc4", "#d4a04e",
      "#4eb8d4", "#d45a5a", "#7dc47d", "#8888cc", "#c4b84e"};

  static Wt::WColor hex_to_wcolor(const std::string& hex)
  {
      if(hex.size() != 7 || hex[0] != '#')
          return Wt::WColor{0x7a, 0xa2, 0xd4};
      int r = std::stoi(hex.substr(1, 2), nullptr, 16);
      int g = std::stoi(hex.substr(3, 2), nullptr, 16);
      int b = std::stoi(hex.substr(5, 2), nullptr, 16);
      return Wt::WColor{r, g, b};
  }

  static std::string wcolor_to_hex(const Wt::WColor& c)
  {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.red(), c.green(), c.blue());
      return buf;
  }

  org_type_manager_page::org_type_manager_page(kanban_db&          db,
                                                org_db&             odb,
                                                long long           org_id,
                                                const session_data& session):
    m_db{db},
    m_org_id{org_id}
  {
      (void)session;
      setStyleClass("page org-type-manager-page");

      const auto org = odb.find_org(org_id);
      const std::string org_name = org ? org->name : "Organization";

      addNew<Wt::WText>("<h1>Task Types \xe2\x80\x94 " + org_name + "</h1>",
                        Wt::TextFormat::UnsafeXHTML);

      auto* back_url = "/org/" + std::to_string(org_id);
      addNew<Wt::WAnchor>(
        Wt::WLink{Wt::LinkType::InternalPath, back_url},
        "\xe2\x86\x90 Back to organization")
        ->setStyleClass("kb-back-link");

      // ── Existing types list ───────────────────────────────────────────────
      m_list = addNew<Wt::WContainerWidget>();
      m_list->setStyleClass("org-type-list");
      refresh_list();

      // ── Add new type form ─────────────────────────────────────────────────
      addNew<Wt::WText>("<h2>Add new type</h2>", Wt::TextFormat::UnsafeXHTML);

      auto* add_row = addNew<Wt::WContainerWidget>();
      add_row->setStyleClass("kb-editor-row");

      m_name_input = add_row->addNew<Wt::WLineEdit>();
      m_name_input->setPlaceholderText("Type name");
      m_name_input->setStyleClass("editor-field");

      const auto existing_types = m_db.types_for_org(m_org_id);
      const std::string default_color =
          k_palette[existing_types.size() % k_palette.size()];

      m_color_picker = add_row->addNew<Wt::WColorPicker>();
      m_color_picker->setStyleClass("kb-color-picker");
      m_color_picker->setColor(hex_to_wcolor(default_color));

      auto* add_btn = add_row->addNew<Wt::WPushButton>("Add Type");
      add_btn->setStyleClass("editor-btn");
      add_btn->clicked().connect([this] { add_type(); });

      m_status_msg = addNew<Wt::WText>("", Wt::TextFormat::Plain);
      m_status_msg->setStyleClass("editor-status");
  }

  void org_type_manager_page::refresh_list()
  {
      m_list->clear();
      m_editing_id = 0;
      m_edit_name  = nullptr;
      m_edit_color = nullptr;

      const auto types = m_db.types_for_org(m_org_id);
      if(types.empty())
      {
          m_list->addNew<Wt::WText>("No types yet.", Wt::TextFormat::Plain)
            ->setStyleClass("org-empty-note");
          return;
      }

      for(const auto& ty : types)
      {
          auto* row = m_list->addNew<Wt::WContainerWidget>();
          row->setStyleClass("org-type-row");

          // Color dot
          auto* dot = row->addNew<Wt::WContainerWidget>();
          dot->setStyleClass("org-type-dot");
          dot->decorationStyle().setBackgroundColor(hex_to_wcolor(ty.color));

          row->addNew<Wt::WText>(ty.name, Wt::TextFormat::Plain)
            ->setStyleClass("org-type-name");

          auto* edit_btn = row->addNew<Wt::WPushButton>("Edit");
          edit_btn->setStyleClass("editor-btn editor-btn-cancel");
          edit_btn->clicked().connect([this, id = ty.id] { start_edit(id); });

          auto* del_btn = row->addNew<Wt::WPushButton>("Delete");
          del_btn->setStyleClass("editor-btn editor-btn-danger");
          del_btn->clicked().connect([this, id = ty.id] { delete_type(id); });
      }
  }

  void org_type_manager_page::start_edit(long long type_id)
  {
      const auto opt = m_db.find_task_type(type_id);
      if(!opt)
          return;

      m_editing_id = type_id;
      m_list->clear();

      const auto types = m_db.types_for_org(m_org_id);
      for(const auto& ty : types)
      {
          auto* row = m_list->addNew<Wt::WContainerWidget>();
          row->setStyleClass("org-type-row");

          if(ty.id == type_id)
          {
              // Inline edit form
              m_edit_name = row->addNew<Wt::WLineEdit>();
              m_edit_name->setText(ty.name);
              m_edit_name->setStyleClass("editor-field");

              m_edit_color = row->addNew<Wt::WColorPicker>();
              m_edit_color->setStyleClass("kb-color-picker");
              m_edit_color->setColor(hex_to_wcolor(ty.color));

              auto* save_btn = row->addNew<Wt::WPushButton>("Save");
              save_btn->setStyleClass("editor-btn");
              save_btn->clicked().connect([this, type_id] { save_edit(type_id); });

              auto* cancel_btn = row->addNew<Wt::WPushButton>("Cancel");
              cancel_btn->setStyleClass("editor-btn editor-btn-cancel");
              cancel_btn->clicked().connect([this] { refresh_list(); });
          }
          else
          {
              auto* dot = row->addNew<Wt::WContainerWidget>();
              dot->setStyleClass("org-type-dot");
              dot->decorationStyle().setBackgroundColor(hex_to_wcolor(ty.color));
              row->addNew<Wt::WText>(ty.name, Wt::TextFormat::Plain)
                ->setStyleClass("org-type-name");
          }
      }
  }

  void org_type_manager_page::save_edit(long long type_id)
  {
      if(!m_edit_name || !m_edit_color)
          return;
      const std::string name = m_edit_name->text().toUTF8();
      if(name.empty())
      {
          m_status_msg->setText("Name is required.");
          return;
      }
      m_db.update_task_type(type_id, name, wcolor_to_hex(m_edit_color->color()));
      live_hub::instance().broadcast(
          "org:" + std::to_string(m_org_id) + ":types");
      m_status_msg->setText("");
      refresh_list();
  }

  void org_type_manager_page::add_type()
  {
      const std::string name = m_name_input->text().toUTF8();
      if(name.empty())
      {
          m_status_msg->setText("Name is required.");
          return;
      }
      m_db.create_task_type(
          m_org_id, name, wcolor_to_hex(m_color_picker->color()));
      live_hub::instance().broadcast(
          "org:" + std::to_string(m_org_id) + ":types");

      m_name_input->setText({});
      m_status_msg->setText("");

      // Advance default color to next palette entry.
      const auto types = m_db.types_for_org(m_org_id);
      m_color_picker->setColor(
          hex_to_wcolor(k_palette[types.size() % k_palette.size()]));

      refresh_list();
  }

  void org_type_manager_page::delete_type(long long type_id)
  {
      m_db.delete_task_type(type_id);
      live_hub::instance().broadcast(
          "org:" + std::to_string(m_org_id) + ":types");
      refresh_list();
  }
  ```

- [ ] **Step 3: Add type manager SCSS to `resources/scss/_kanban.scss`**

  Append at the end of the file:

  ```scss
  // ── Type manager page ─────────────────────────────────────────────────────
  .org-type-list {
    display:        flex;
    flex-direction: column;
    gap:            8px;
    margin-bottom:  24px;
  }

  .org-type-row {
    display:     flex;
    align-items: center;
    gap:         10px;
    padding:     8px 12px;
    background:  var(--color-surface);
    border:      1px solid var(--color-border);
    border-radius: var(--radius);
  }

  .org-type-dot {
    width:         16px;
    height:        16px;
    border-radius: 50%;
    flex-shrink:   0;
  }

  .org-type-name {
    flex: 1;
  }
  ```

- [ ] **Step 4: Build**

  ```bash
  cmake --build build --parallel $(nproc) 2>&1 | tail -20
  ```
  Expected: succeeds.

- [ ] **Step 5: Commit**

  ```bash
  git add src/pages/org_type_manager_page.hpp \
          src/pages/org_type_manager_page.cpp \
          resources/scss/_kanban.scss
  git commit -m "feat: add org_type_manager_page for CRUD management of task types"
  ```

---

## Task 6: Routing and navigation

**Files:**
- Modify: `src/altinf_app.cpp`
- Modify: `src/pages/org_landing_page.cpp`

- [ ] **Step 1: Add `#include` and route in `altinf_app.cpp`**

  At the top with other page includes, add:
  ```cpp
  #include "pages/org_type_manager_page.hpp"
  ```

  In the `/org/` routing block (~line 428), add a new `else if` branch before the existing `else` catch-all:
  ```cpp
  else if(suffix == "/types")
  {
      if(!is_org_lead)
      {
          show_forbidden();
          return;
      }
      m_content->addNew<org_type_manager_page>(
        *m_kanban_db, *m_org_db, org_id, m_session);
  }
  ```

- [ ] **Step 2: Add "Manage Types" link to `org_landing_page.cpp`**

  In `org_landing_page.cpp`, in the `if(m_is_org_lead)` block (~line 59), add a third action button after the existing two:
  ```cpp
  actions->addNew<Wt::WAnchor>(
           Wt::WLink{Wt::LinkType::InternalPath,
                     "/org/" + std::to_string(m_org_id) + "/types"},
           "Manage types")
    ->setStyleClass("editor-btn editor-btn-cancel");
  ```

- [ ] **Step 3: Build**

  ```bash
  cmake --build build --parallel $(nproc) 2>&1 | tail -20
  ```
  Expected: succeeds.

- [ ] **Step 4: Run all C++ and JS tests**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -20
  cd ../tests/js && node test_gantt.js 2>&1 | tail -10
  ```
  Expected: all pass.

- [ ] **Step 5: Commit**

  ```bash
  git add src/altinf_app.cpp src/pages/org_landing_page.cpp
  git commit -m "feat: add /org/{id}/types route and Manage Types nav link"
  ```

---

## Task 7: E2E tests

**Files:**
- Create: `e2e/specs/live-task-types.spec.ts`

- [ ] **Step 1: Write the E2E spec**

  Create `e2e/specs/live-task-types.spec.ts`:

  ```typescript
  import { test, expect, type Page } from '@playwright/test';

  test.describe.configure({ mode: 'serial' });

  async function loginAs(page: Page, username: string, password: string) {
    await page.goto('/?_=/login');
    await page.locator('input[placeholder="Username"]').fill(username);
    await page.locator('input[placeholder="Password"]').fill(password);
    await page.locator('.login-btn').click();
    await expect(page.locator('.nav-logout')).toBeVisible();
  }

  async function goToOrgLanding(page: Page, orgName: string) {
    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await expect(page.locator('.org-admin-page')).toBeVisible();
    await page.locator('.org-list-link', { hasText: new RegExp(`^${orgName}$`) }).click();
    await expect(page.locator('.org-landing-page')).toBeVisible();
  }

  test.beforeAll(async ({ browser }) => {
    const page = await browser.newPage();
    await loginAs(page, 'admin', 'testpass');

    // Create org and team used by all type tests.
    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await page.locator('input[placeholder="Organization name"]').fill('TypeTestOrg');
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: /^TypeTestOrg$/ })).toBeVisible();

    await page.locator('.org-list-link', { hasText: /^TypeTestOrg$/ }).click();
    await page.getByRole('link', { name: 'Manage organization' }).click();
    await page.locator('input[placeholder="Team name"]').fill('TypeTeam');
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block')).toBeVisible();

    await page.close();
  });

  test('type manager: org lead can navigate to type manager page', async ({ browser }) => {
    const page = await (await browser.newContext()).newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToOrgLanding(page, 'TypeTestOrg');

    await page.getByRole('link', { name: 'Manage types' }).click();
    await expect(page.locator('.org-type-manager-page')).toBeVisible();
    await expect(page.locator('h1')).toContainText('Task Types');
  });

  test('type manager: create a type and see it in the list', async ({ browser }) => {
    const page = await (await browser.newContext()).newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToOrgLanding(page, 'TypeTestOrg');
    await page.getByRole('link', { name: 'Manage types' }).click();
    await expect(page.locator('.org-type-manager-page')).toBeVisible();

    await page.locator('input[placeholder="Type name"]').fill('Bug Fix');
    await page.getByRole('button', { name: 'Add Type' }).click();

    await expect(page.locator('.org-type-name', { hasText: 'Bug Fix' })).toBeVisible();
  });

  test('type manager: edit a type name', async ({ browser }) => {
    const page = await (await browser.newContext()).newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToOrgLanding(page, 'TypeTestOrg');
    await page.getByRole('link', { name: 'Manage types' }).click();
    await expect(page.locator('.org-type-name', { hasText: 'Bug Fix' })).toBeVisible();

    await page.locator('.org-type-row', { hasText: 'Bug Fix' })
      .getByRole('button', { name: 'Edit' }).click();

    const nameInput = page.locator('.org-type-row input[type="text"]').first();
    await nameInput.fill('Defect');
    await page.locator('.org-type-row').getByRole('button', { name: 'Save' }).click();

    await expect(page.locator('.org-type-name', { hasText: 'Defect' })).toBeVisible();
    await expect(page.locator('.org-type-name', { hasText: 'Bug Fix' })).not.toBeVisible();
  });

  test('task editor: shows chip selector instead of color picker', async ({ browser }) => {
    const page = await (await browser.newContext()).newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToOrgLanding(page, 'TypeTestOrg');
    await page.locator('.org-team-link').first().click();
    await expect(page.locator('.kb-board')).toBeVisible();

    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();

    // Chip selector should be visible; no color picker input.
    await expect(page.locator('.kb-type-chips')).toBeVisible();
    await expect(page.locator('.kb-color-picker')).not.toBeVisible();
  });

  test('task editor: assign a type and board card shows that type color', async ({ browser }) => {
    const page = await (await browser.newContext()).newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToOrgLanding(page, 'TypeTestOrg');
    await page.locator('.org-team-link').first().click();
    await expect(page.locator('.kb-board')).toBeVisible();

    await page.locator('.kb-new-btn').click();
    await page.locator('input[placeholder="Task title"]').fill('TypedTask');

    // Click the "Defect" type chip.
    await page.locator('.kb-type-chip', { hasText: 'Defect' }).click();
    await expect(page.locator('.kb-type-chip.selected')).toContainText('Defect');

    // Save.
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible();

    // The card's left border should not be the default gray (#cccccc).
    const card = page.locator('.kb-card', { hasText: 'TypedTask' });
    await expect(card).toBeVisible();
    const borderColor = await card.evaluate((el) =>
      window.getComputedStyle(el).borderLeftColor);
    expect(borderColor).not.toBe('rgb(204, 204, 204)');
  });

  test('type manager: delete type — task card goes gray', async ({ browser }) => {
    const page = await (await browser.newContext()).newPage();
    await loginAs(page, 'admin', 'testpass');

    // Delete the "Defect" type.
    await goToOrgLanding(page, 'TypeTestOrg');
    await page.getByRole('link', { name: 'Manage types' }).click();
    await page.locator('.org-type-row', { hasText: 'Defect' })
      .getByRole('button', { name: 'Delete' }).click();
    await expect(page.locator('.org-type-name', { hasText: 'Defect' })).not.toBeVisible();

    // Navigate to the board; card should now show the gray (#cccccc) border.
    await goToOrgLanding(page, 'TypeTestOrg');
    await page.locator('.org-team-link').first().click();
    await expect(page.locator('.kb-board')).toBeVisible();

    const card = page.locator('.kb-card', { hasText: 'TypedTask' });
    await expect(card).toBeVisible();
    const borderColor = await card.evaluate((el) =>
      window.getComputedStyle(el).borderLeftColor);
    expect(borderColor).toBe('rgb(204, 204, 204)');
  });
  ```

- [ ] **Step 2: Run full test suite**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -20
  cd ../tests/js && node test_gantt.js 2>&1 | tail -10
  cd ../e2e && npx playwright test 2>&1 | tail -30
  ```
  Expected: all three suites pass.

- [ ] **Step 3: Commit**

  ```bash
  git add e2e/specs/live-task-types.spec.ts
  git commit -m "test: add E2E tests for task type manager and editor chip selector"
  ```
