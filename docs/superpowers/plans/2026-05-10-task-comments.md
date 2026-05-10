# Task Comments Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow any team member who can view a board to post, edit, and soft-delete Markdown comments on tasks, with a full audit log and real-time updates to all viewers of the same task.

**Architecture:** Two new Wt::Dbo tables (`task_comment`, `task_comment_event`) mirror the existing `task_event`/`task_field_change` pattern. The task editor's Details tab grows a Comments section below the type chips. Comment changes broadcast on a dedicated `"task:{id}:comments"` live_hub channel (separate from `"task:{id}"`) so they refresh the comment list without triggering the task-field stale-save guard.

**Tech Stack:** C++23, Wt::Dbo (SQLite3), cmark (already linked), Catch2 v3, Playwright E2E.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `src/kanban/kanban.hpp` | Modify | Add `task_comment_record`, `task_comment_event_record`, `task_comment_entry` |
| `src/kanban/kanban_db.hpp` | Modify | Declare 4 public comment methods + `record_comment_event` private helper |
| `src/kanban/kanban_db.cpp` | Modify | Map new tables, add CREATE TABLE migrations, implement all methods |
| `src/pages/kanban_task_editor_page.hpp` | Modify | Add `m_comment_list`, `m_comment_compose`, `rebuild_comments()` |
| `src/pages/kanban_task_editor_page.cpp` | Modify | Extract `format_ts` helper, build comment section, subscribe to comments channel |
| `resources/scss/_kanban.scss` | Modify | Comment section styles |
| `tests/test_kanban_db.cpp` | Modify | 8 new Catch2 test cases for comment DB methods |
| `e2e/specs/comments.spec.ts` | Create | Playwright E2E tests (post, edit, delete, lead dialogs, archived, live updates) |

---

## Task 1: Add data structs to `kanban.hpp`

**Files:**
- Modify: `src/kanban/kanban.hpp`

- [ ] **Step 1: Add the three new structs after the existing `task_event_entry` struct (line 175)**

  Append this block at the end of `src/kanban/kanban.hpp`, before the final `};` closing:

  ```cpp
  struct task_comment_record
  {
      long long   task_id{0};
      std::string author;
      std::string body;
      std::string created_at;
      int         is_deleted{0};

      template<class Action>
      void persist(Action& a)
      {
          Wt::Dbo::field(a, task_id,    "task_id");
          Wt::Dbo::field(a, author,     "author");
          Wt::Dbo::field(a, body,       "body");
          Wt::Dbo::field(a, created_at, "created_at");
          Wt::Dbo::field(a, is_deleted, "is_deleted");
      }
  };

  struct task_comment_event_record
  {
      long long   comment_id{0};
      std::string actor;
      std::string occurred_at;
      std::string event_type;
      std::string body_snapshot;

      template<class Action>
      void persist(Action& a)
      {
          Wt::Dbo::field(a, comment_id,    "comment_id");
          Wt::Dbo::field(a, actor,         "actor");
          Wt::Dbo::field(a, occurred_at,   "occurred_at");
          Wt::Dbo::field(a, event_type,    "event_type");
          Wt::Dbo::field(a, body_snapshot, "body_snapshot");
      }
  };

  struct task_comment_entry
  {
      long long   id{0};
      long long   task_id{0};
      std::string author;
      std::string body;        // empty string when is_deleted
      std::string created_at;
      bool        is_deleted{false};
      std::string last_edited_by;
      std::string last_edited_at;
      std::string deleted_by;
      std::string deleted_at;
  };
  ```

- [ ] **Step 2: Verify the file compiles in isolation**

  Run: `cmake --build build --parallel $(nproc) --target altinf 2>&1 | head -30`

  Expected: Either clean build or errors only from kanban_db.cpp (not kanban.hpp).

- [ ] **Step 3: Commit**

  ```bash
  git add src/kanban/kanban.hpp
  git commit -m "feat(comments): add task_comment and task_comment_event structs"
  ```

---

## Task 2: Add method declarations to `kanban_db.hpp`

**Files:**
- Modify: `src/kanban/kanban_db.hpp`

- [ ] **Step 1: Add the public comment methods after the `history_for_task` declaration (line 59)**

  In the `// Tasks` section of `kanban_db.hpp`, after the `history_for_task` declaration, add:

  ```cpp
  // Comments
  long long                            add_comment(long long task_id,
                                                   const std::string& author,
                                                   const std::string& body);
  void                                 edit_comment(long long comment_id,
                                                    const std::string& editor,
                                                    const std::string& new_body);
  void                                 delete_comment(long long comment_id,
                                                      const std::string& actor);
  std::vector<task_comment_entry>      comments_for_task(long long task_id);
  ```

- [ ] **Step 2: Add the private helper declaration after the existing `record_event` declaration (line 86)**

  In the `private:` section of `kanban_db.hpp`, after `record_event`, add:

  ```cpp
  void record_comment_event(long long          comment_id,
                             const std::string& actor,
                             const std::string& event_type,
                             const std::string& body_snapshot);
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add src/kanban/kanban_db.hpp
  git commit -m "feat(comments): declare comment DB methods"
  ```

---

## Task 3: Write failing Catch2 tests

**Files:**
- Modify: `tests/test_kanban_db.cpp`

- [ ] **Step 1: Append the following test cases at the end of `tests/test_kanban_db.cpp`**

  ```cpp
  // ---- comments ----

  TEST_CASE("kanban_db - add_comment returns id and is retrievable")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      const long long cid  = db.add_comment(task, "alice", "Hello **world**");
      CHECK(cid > 0);
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 1);
      CHECK(comments[0].id == cid);
      CHECK(comments[0].author == "alice");
      CHECK(comments[0].body == "Hello **world**");
      CHECK(!comments[0].is_deleted);
      CHECK(comments[0].last_edited_by.empty());
      CHECK(comments[0].deleted_by.empty());
      CHECK(!comments[0].created_at.empty());
  }

  TEST_CASE("kanban_db - comments_for_task returns in chronological order")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      db.add_comment(task, "alice", "First");
      db.add_comment(task, "bob",   "Second");
      db.add_comment(task, "carol", "Third");
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 3);
      CHECK(comments[0].body == "First");
      CHECK(comments[1].body == "Second");
      CHECK(comments[2].body == "Third");
  }

  TEST_CASE("kanban_db - edit_comment updates body and records edited event")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      const long long cid  = db.add_comment(task, "alice", "Original");
      db.edit_comment(cid, "alice", "Revised");
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 1);
      CHECK(comments[0].body == "Revised");
      CHECK(comments[0].last_edited_by == "alice");
      CHECK(!comments[0].last_edited_at.empty());
  }

  TEST_CASE("kanban_db - edit_comment reflects most recent editor when edited twice")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      const long long cid  = db.add_comment(task, "alice", "v1");
      db.edit_comment(cid, "alice", "v2");
      db.edit_comment(cid, "bob",   "v3");
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 1);
      CHECK(comments[0].body == "v3");
      CHECK(comments[0].last_edited_by == "bob");
  }

  TEST_CASE("kanban_db - edit_comment is a no-op on deleted comment")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      const long long cid  = db.add_comment(task, "alice", "Original");
      db.delete_comment(cid, "alice");
      db.edit_comment(cid, "alice", "Should not apply");
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 1);
      CHECK(comments[0].body.empty());           // still deleted, body cleared
      CHECK(comments[0].last_edited_by.empty()); // no edit event recorded
  }

  TEST_CASE("kanban_db - delete_comment sets is_deleted and clears body in entry")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      const long long cid  = db.add_comment(task, "alice", "Goodbye");
      db.delete_comment(cid, "bob");
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 1);
      CHECK(comments[0].is_deleted);
      CHECK(comments[0].body.empty());
      CHECK(comments[0].deleted_by == "bob");
      CHECK(!comments[0].deleted_at.empty());
  }

  TEST_CASE("kanban_db - delete_comment is a no-op if already deleted")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      const long long cid  = db.add_comment(task, "alice", "Hello");
      db.delete_comment(cid, "alice");
      db.delete_comment(cid, "bob"); // second delete is a no-op
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 1);
      CHECK(comments[0].deleted_by == "alice"); // original deletor preserved
  }

  TEST_CASE("kanban_db - comments_for_task includes deleted comments as placeholders")
  {
      kanban_db       db{":memory:"};
      const long long tid  = db.create_team("T", 1);
      const long long task = db.add_task(make_task(tid, "Work"), "creator");
      db.add_comment(task, "alice", "Keep me");
      const long long cid2 = db.add_comment(task, "bob", "Delete me");
      db.delete_comment(cid2, "bob");
      const auto comments = db.comments_for_task(task);
      REQUIRE(comments.size() == 2);
      CHECK(!comments[0].is_deleted);
      CHECK(comments[0].body == "Keep me");
      CHECK(comments[1].is_deleted);
      CHECK(comments[1].body.empty());
  }
  ```

- [ ] **Step 2: Attempt to build the test binary — expect a linker failure**

  Run: `cmake --build build --parallel $(nproc) --target test_kanban_db 2>&1 | tail -20`

  Expected: Linker errors like `undefined reference to 'kanban_db::add_comment'` — this confirms the tests are wired and waiting for an implementation.

- [ ] **Step 3: Commit**

  ```bash
  git add tests/test_kanban_db.cpp
  git commit -m "test(comments): add failing Catch2 tests for comment DB methods"
  ```

---

## Task 4: Implement the DB layer

**Files:**
- Modify: `src/kanban/kanban_db.cpp`

- [ ] **Step 1: Map the new record classes and add CREATE TABLE migrations in the constructor**

  After the existing `m_dbo.mapClass<task_field_change_record>(...)` call (line 17), add:

  ```cpp
  m_dbo.mapClass<task_comment_record>("task_comment");
  m_dbo.mapClass<task_comment_event_record>("task_comment_event");
  ```

  After all existing `migrate(...)` calls (after line 41), add:

  ```cpp
  migrate(
    "CREATE TABLE IF NOT EXISTS task_comment ("
    "id integer primary key autoincrement,"
    " task_id integer not null default 0,"
    " author text not null default '',"
    " body text not null default '',"
    " created_at text not null default '',"
    " is_deleted integer not null default 0)");

  migrate(
    "CREATE TABLE IF NOT EXISTS task_comment_event ("
    "id integer primary key autoincrement,"
    " comment_id integer not null default 0,"
    " actor text not null default '',"
    " occurred_at text not null default '',"
    " event_type text not null default '',"
    " body_snapshot text not null default '')");
  ```

- [ ] **Step 2: Add the private `record_comment_event` helper at the bottom of `kanban_db.cpp`**

  Append after the existing `record_event` implementation:

  ```cpp
  void kanban_db::record_comment_event(long long          comment_id,
                                        const std::string& actor,
                                        const std::string& event_type,
                                        const std::string& body_snapshot)
  {
      const std::string ts =
        Wt::WDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toUTF8();

      auto ev                    = m_dbo.add(std::make_unique<task_comment_event_record>());
      ev.modify()->comment_id    = comment_id;
      ev.modify()->actor         = actor;
      ev.modify()->occurred_at   = ts;
      ev.modify()->event_type    = event_type;
      ev.modify()->body_snapshot = body_snapshot;
  }
  ```

- [ ] **Step 3: Implement `add_comment`**

  Add in the `// ---- Tasks ----` section after `history_for_task`:

  ```cpp
  // ---- Comments ----

  long long kanban_db::add_comment(long long          task_id,
                                    const std::string& author,
                                    const std::string& body)
  {
      Wt::Dbo::Transaction t{m_dbo};
      const std::string    ts =
        Wt::WDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toUTF8();

      auto p               = m_dbo.add(std::make_unique<task_comment_record>());
      p.modify()->task_id    = task_id;
      p.modify()->author     = author;
      p.modify()->body       = body;
      p.modify()->created_at = ts;
      p.modify()->is_deleted = 0;
      m_dbo.flush();
      const long long new_id = p.id();

      record_comment_event(new_id, author, "created", body);
      return new_id;
  }
  ```

- [ ] **Step 4: Implement `edit_comment`**

  ```cpp
  void kanban_db::edit_comment(long long          comment_id,
                                const std::string& editor,
                                const std::string& new_body)
  {
      Wt::Dbo::Transaction t{m_dbo};
      const auto           results = m_dbo.find<task_comment_record>()
                             .where("id = ?")
                             .bind(comment_id)
                             .resultList();
      if(results.empty())
      {
          return;
      }
      Wt::Dbo::ptr<task_comment_record> p = *results.begin();
      if(p->is_deleted)
      {
          return; // No-op on deleted comment.
      }
      p.modify()->body = new_body;
      record_comment_event(comment_id, editor, "edited", new_body);
  }
  ```

- [ ] **Step 5: Implement `delete_comment`**

  ```cpp
  void kanban_db::delete_comment(long long comment_id, const std::string& actor)
  {
      Wt::Dbo::Transaction t{m_dbo};
      const auto           results = m_dbo.find<task_comment_record>()
                             .where("id = ?")
                             .bind(comment_id)
                             .resultList();
      if(results.empty())
      {
          return;
      }
      Wt::Dbo::ptr<task_comment_record> p = *results.begin();
      if(p->is_deleted)
      {
          return; // Already deleted — no-op.
      }
      const std::string body_snapshot = p->body;
      p.modify()->is_deleted          = 1;
      record_comment_event(comment_id, actor, "deleted", body_snapshot);
  }
  ```

- [ ] **Step 6: Implement `comments_for_task`**

  ```cpp
  std::vector<task_comment_entry> kanban_db::comments_for_task(long long task_id)
  {
      Wt::Dbo::Transaction t{m_dbo};
      const auto           rows = m_dbo.find<task_comment_record>()
                           .where("task_id = ?")
                           .bind(task_id)
                           .orderBy("id")
                           .resultList();

      std::vector<task_comment_entry> out;
      for(const auto& c: rows)
      {
          task_comment_entry e;
          e.id         = c.id();
          e.task_id    = c->task_id;
          e.author     = c->author;
          e.created_at = c->created_at;
          e.is_deleted = c->is_deleted != 0;
          e.body       = e.is_deleted ? "" : c->body;

          const auto events = m_dbo.find<task_comment_event_record>()
                                .where("comment_id = ?")
                                .bind(c.id())
                                .orderBy("id")
                                .resultList();
          for(const auto& ev: events)
          {
              if(ev->event_type == "edited")
              {
                  e.last_edited_by = ev->actor;
                  e.last_edited_at = ev->occurred_at;
              }
              else if(ev->event_type == "deleted")
              {
                  e.deleted_by = ev->actor;
                  e.deleted_at = ev->occurred_at;
              }
          }
          out.push_back(std::move(e));
      }
      return out;
  }
  ```

- [ ] **Step 7: Build the test binary and run the tests**

  Run: `cmake --build build --parallel $(nproc) --target test_kanban_db && ./build/test_kanban_db`

  Expected: All 8 new `kanban_db - *comment*` tests pass alongside all pre-existing tests.

- [ ] **Step 8: Commit**

  ```bash
  git add src/kanban/kanban_db.cpp
  git commit -m "feat(comments): implement comment DB methods with audit log"
  ```

---

## Task 5: Add SCSS for the comment section

**Files:**
- Modify: `resources/scss/_kanban.scss`

- [ ] **Step 1: Append the following block at the end of `resources/scss/_kanban.scss`**

  ```scss
  // ── Task comments ─────────────────────────────────────────────────────────────

  .kb-comment-section {
    display:        flex;
    flex-direction: column;
    gap:            1rem;
    margin-top:     1rem;

    h2 {
      font-size:      0.8rem;
      color:          var(--color-muted);
      text-transform: uppercase;
      letter-spacing: 0.06em;
      border-bottom:  1px solid var(--color-border);
      padding-bottom: 0.3rem;
      margin-top:     0.5rem;
    }
  }

  .kb-comment-list {
    display:        flex;
    flex-direction: column;
    gap:            0.75rem;
  }

  .kb-comment-item {
    border:        1px solid var(--color-border);
    border-radius: var(--radius);
    padding:       0.75rem 1rem;
    background:    var(--color-surface);
  }

  .kb-comment-header {
    display:        flex;
    gap:            0.5rem;
    font-size:      0.78rem;
    color:          var(--color-muted);
    margin-bottom:  0.4rem;

    .kb-comment-author { font-weight: 600; color: var(--color-text); }
  }

  .kb-comment-body {
    font-size: 0.9rem;
    line-height: 1.55;

    p  { margin: 0 0 0.4rem; }
    p:last-child { margin-bottom: 0; }
    code { font-family: var(--font-mono); font-size: 0.85em; }
  }

  .kb-comment-meta {
    font-size:   0.72rem;
    color:       var(--color-muted);
    margin-top:  0.3rem;
    font-style:  italic;
  }

  .kb-comment-actions {
    display:     flex;
    gap:         0.5rem;
    margin-top:  0.5rem;

    button {
      font-size:     0.72rem;
      padding:       0.2rem 0.5rem;
      background:    transparent;
      border:        1px solid var(--color-border);
      border-radius: var(--radius);
      color:         var(--color-muted);
      cursor:        pointer;

      &:hover { color: var(--color-text); border-color: var(--color-accent); }
    }

    .kb-comment-del-btn:hover {
      color:        #e06c6c;
      border-color: #e06c6c;
    }
  }

  .kb-comment-edit-area {
    display:        flex;
    flex-direction: column;
    gap:            0.5rem;
    margin-top:     0.4rem;

    textarea {
      width:         100%;
      min-height:    80px;
      resize:        vertical;
      font-family:   var(--font-mono);
      font-size:     0.85rem;
      box-sizing:    border-box;
    }

    .kb-comment-edit-btns {
      display: flex;
      gap:     0.5rem;
    }
  }

  .kb-comment-deleted {
    font-style: italic;
    color:      var(--color-muted);
    font-size:  0.85rem;
    padding:    0.5rem 0;
  }

  .kb-comment-compose {
    display:        flex;
    flex-direction: column;
    gap:            0.5rem;

    textarea {
      width:       100%;
      min-height:  70px;
      resize:      vertical;
      font-size:   0.9rem;
      box-sizing:  border-box;
    }
  }

  .kb-comment-post-btn { align-self: flex-start; }
  ```

- [ ] **Step 2: Build to verify SCSS compiles**

  Run: `cmake --build build --parallel $(nproc) --target scss_compile 2>&1`

  Expected: `Compiling SCSS → CSS` or no output (already up to date after touching the file).

- [ ] **Step 3: Commit**

  ```bash
  git add resources/scss/_kanban.scss
  git commit -m "feat(comments): add SCSS for comment section"
  ```

---

## Task 6: Wire up the comment section UI

**Files:**
- Modify: `src/pages/kanban_task_editor_page.hpp`
- Modify: `src/pages/kanban_task_editor_page.cpp`

- [ ] **Step 1: Update `kanban_task_editor_page.hpp`**

  Add the following to the private section of `kanban_task_editor_page`:

  After `Wt::WContainerWidget* m_history_panel{nullptr};` add:

  ```cpp
  Wt::WContainerWidget* m_comment_list{nullptr};
  Wt::WContainerWidget* m_comment_compose{nullptr};
  ```

  After `void rebuild_history();` add:

  ```cpp
  void rebuild_comments();
  ```

- [ ] **Step 2: Extract `format_ts` to a file-static helper in `kanban_task_editor_page.cpp`**

  Add this free function near the top of `kanban_task_editor_page.cpp` (after the `#include` block, before the `k_status_vals` definition):

  ```cpp
  static std::string format_ts(const std::string& iso)
  {
      if(iso.size() < 16)
      {
          return iso;
      }
      try
      {
          const int          yr  = std::stoi(iso.substr(0, 4));
          const int          mo  = std::stoi(iso.substr(5, 2));
          const int          dy  = std::stoi(iso.substr(8, 2));
          const auto         hr  = iso.substr(11, 2);
          const auto         mn  = iso.substr(14, 2);
          static const char* months[] = {
            "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
          return std::string(months[mo]) + " " + std::to_string(dy) +
                 ", " + std::to_string(yr) + " at " + hr + ":" + mn;
      }
      catch(...)
      {
          return iso;
      }
  }
  ```

  Then replace the local `format_ts` lambda inside `rebuild_history()` (which spans several lines) with a call to the static function. In `rebuild_history()`, delete the lines:

  ```cpp
  auto format_ts = [](const std::string& iso) -> std::string {
      // Parse "2026-05-06T14:30:00Z" -> "May 6, 2026 at 14:30"
      if(iso.size() < 16)
      {
          return iso;
      }
      try
      {
          const int          yr       = std::stoi(iso.substr(0, 4));
          const int          mo       = std::stoi(iso.substr(5, 2));
          const int          dy       = std::stoi(iso.substr(8, 2));
          const auto         hr       = iso.substr(11, 2);
          const auto         mn       = iso.substr(14, 2);
          static const char* months[] = {
            "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
          return std::string(months[mo]) + " " + std::to_string(dy) +
                 ", " + std::to_string(yr) + " at " + hr + ":" + mn;
      }
      catch(...)
      {
          return iso;
      }
  };
  ```

  Those lines are already replaced by the file-static `format_ts` function — no other changes needed in `rebuild_history()`.

- [ ] **Step 3: Add the necessary `#include` directives to `kanban_task_editor_page.cpp`**

  After the existing `#include <Wt/WAnchor.h>` block, add:

  ```cpp
  #include <Wt/WDialog.h>
  #include <cmark.h>
  #include <cstdlib>
  ```

- [ ] **Step 4: Build the comment section in the constructor**

  In `kanban_task_editor_page.cpp`, after the type chip section (after the closing brace of the `add_chip(0, "(None)", "#cccccc")` and the type loop — around where `m_status_msg` is created), and BEFORE the `m_status_msg` line, add the comment section to the Details tab form:

  ```cpp
  // ── Comments ─────────────────────────────────────────────────────────────────
  if(existing)
  {
      form->addNew<Wt::WText>("<h2>Comments</h2>", Wt::TextFormat::UnsafeXHTML);
      auto* comment_section = form->addNew<Wt::WContainerWidget>();
      comment_section->setStyleClass("kb-comment-section");

      m_comment_list = comment_section->addNew<Wt::WContainerWidget>();
      m_comment_list->setStyleClass("kb-comment-list");

      if(!existing->is_archived)
      {
          m_comment_compose = comment_section->addNew<Wt::WContainerWidget>();
          m_comment_compose->setStyleClass("kb-comment-compose");
          auto* compose_ta = m_comment_compose->addNew<Wt::WTextArea>();
          compose_ta->setPlaceholderText("Add a comment\xe2\x80\xa6");
          compose_ta->setStyleClass("editor-field");
          auto* post_btn   = m_comment_compose->addNew<Wt::WPushButton>("Post");
          post_btn->setStyleClass("editor-btn kb-comment-post-btn");
          post_btn->setDisabled(true);
          compose_ta->keyWentUp().connect([compose_ta, post_btn] {
              post_btn->setDisabled(compose_ta->text().empty());
          });
          post_btn->clicked().connect([this, compose_ta, post_btn] {
              const std::string body = compose_ta->text().toUTF8();
              if(body.empty())
              {
                  return;
              }
              m_db.add_comment(m_task_id, m_username, body);
              compose_ta->setText({});
              post_btn->setDisabled(true);
              live_hub::instance().broadcast(
                "task:" + std::to_string(m_task_id) + ":comments");
              rebuild_comments();
          });
      }
  }
  ```

- [ ] **Step 5: Subscribe to the comments live_hub channel in the constructor**

  In the constructor, after the existing `live_hub::instance().subscribe("task:..." ...)` block (around line 349), add:

  ```cpp
  if(existing)
  {
      live_hub::instance().subscribe(
        "task:" + std::to_string(m_task_id) + ":comments",
        m_session_id,
        [this, alive = m_alive] {
            if(*alive)
            {
                rebuild_comments();
                Wt::WApplication::instance()->triggerUpdate();
            }
        });
  }
  ```

  Note: This block must appear AFTER `m_task_id` and `m_session_id` are assigned (they're set in the existing subscription block above).

- [ ] **Step 6: Unsubscribe from the comments channel in the destructor**

  In `kanban_task_editor_page::~kanban_task_editor_page()`, after the existing `live_hub::instance().unsubscribe("task:..." ...)` call, add:

  ```cpp
  if(m_task_id != 0)
  {
      live_hub::instance().unsubscribe(
        "task:" + std::to_string(m_task_id) + ":comments", m_session_id);
  }
  ```

  The destructor will now have two unsubscribe calls (one for each channel). Both check `m_task_id != 0`.

- [ ] **Step 7: Implement `rebuild_comments()` and add an initial call**

  Add this method implementation to `kanban_task_editor_page.cpp`, after `rebuild_history()`:

  ```cpp
  void kanban_task_editor_page::rebuild_comments()
  {
      if(!m_comment_list || m_task_id == 0)
      {
          return;
      }
      m_comment_list->clear();

      const auto comments = m_db.comments_for_task(m_task_id);

      for(const auto& e: comments)
      {
          auto* item = m_comment_list->addNew<Wt::WContainerWidget>();

          if(e.is_deleted)
          {
              item->setStyleClass("kb-comment-deleted");
              const std::string msg =
                "Comment deleted by " + e.deleted_by +
                " \xe2\x80\x94 " + format_ts(e.deleted_at);
              item->addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
              continue;
          }

          item->setStyleClass("kb-comment-item");

          // Header: author + timestamp
          auto* header = item->addNew<Wt::WContainerWidget>();
          header->setStyleClass("kb-comment-header");
          auto* author_span = header->addNew<Wt::WText>(e.author, Wt::TextFormat::Plain);
          author_span->setStyleClass("kb-comment-author");
          header->addNew<Wt::WText>(
            " \xe2\x80\x94 " + format_ts(e.created_at), Wt::TextFormat::Plain);

          // Content div (rendered body + meta + actions) — hidden during inline edit
          auto* content_div = item->addNew<Wt::WContainerWidget>();

          // Rendered Markdown body
          {
              char* const html =
                cmark_markdown_to_html(e.body.c_str(), e.body.size(), CMARK_OPT_DEFAULT);
              content_div->addNew<Wt::WText>(std::string{html}, Wt::TextFormat::UnsafeXHTML)
                ->setStyleClass("kb-comment-body");
              free(html);
          }

          // "Edited by" metadata
          if(!e.last_edited_by.empty())
          {
              const std::string meta =
                "Edited by " + e.last_edited_by +
                " \xe2\x80\x94 " + format_ts(e.last_edited_at);
              content_div->addNew<Wt::WText>(meta, Wt::TextFormat::Plain)
                ->setStyleClass("kb-comment-meta");
          }

          // Edit/Delete actions — visible only to author and leads
          const bool can_act = (m_username == e.author) || m_is_lead;
          const bool is_own  = (m_username == e.author);

          auto* actions_div = content_div->addNew<Wt::WContainerWidget>();
          actions_div->setStyleClass("kb-comment-actions");

          // Edit inline area (hidden by default)
          auto* edit_div = item->addNew<Wt::WContainerWidget>();
          edit_div->setStyleClass("kb-comment-edit-area");
          edit_div->hide();

          if(can_act && !m_existing->is_archived)
          {
              // -- Edit button --
              auto* edit_btn = actions_div->addNew<Wt::WPushButton>("Edit");
              const long long comment_id  = e.id;
              const std::string raw_body  = e.body;
              const std::string e_author  = e.author;

              // Build the inline edit area now (hidden)
              auto* edit_ta = edit_div->addNew<Wt::WTextArea>();
              edit_ta->setStyleClass("editor-field");
              edit_ta->setText(raw_body);
              auto* edit_btns = edit_div->addNew<Wt::WContainerWidget>();
              edit_btns->setStyleClass("kb-comment-edit-btns");
              auto* save_btn   = edit_btns->addNew<Wt::WPushButton>("Save");
              save_btn->setStyleClass("editor-btn");
              auto* cancel_btn = edit_btns->addNew<Wt::WPushButton>("Cancel");
              cancel_btn->setStyleClass("editor-btn editor-btn-cancel");

              save_btn->clicked().connect([this, comment_id, edit_ta] {
                  const std::string new_body = edit_ta->text().toUTF8();
                  if(new_body.empty()) return;
                  m_db.edit_comment(comment_id, m_username, new_body);
                  live_hub::instance().broadcast(
                    "task:" + std::to_string(m_task_id) + ":comments");
                  rebuild_comments();
              });
              cancel_btn->clicked().connect([content_div, edit_div] {
                  edit_div->hide();
                  content_div->show();
              });

              edit_btn->clicked().connect(
                [this, content_div, edit_div, edit_ta, raw_body, e_author, is_own] {
                    edit_ta->setText(raw_body);
                    if(is_own)
                    {
                        content_div->hide();
                        edit_div->show();
                    }
                    else
                    {
                        auto* d = new Wt::WDialog("Edit Another User's Comment");
                        d->contents()->addNew<Wt::WText>(
                          "This comment was written by " + e_author +
                          ". Are you sure you want to edit it?",
                          Wt::TextFormat::Plain);
                        auto* yes = d->footer()->addNew<Wt::WPushButton>("Edit Anyway");
                        yes->setStyleClass("editor-btn");
                        auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
                        no->setStyleClass("editor-btn editor-btn-cancel");
                        yes->clicked().connect([d, content_div, edit_div] {
                            d->accept();
                            content_div->hide();
                            edit_div->show();
                        });
                        no->clicked().connect([d] { d->reject(); });
                        d->finished().connect([d](Wt::DialogCode) { delete d; });
                        d->show();
                    }
                });

              // -- Delete button --
              auto* del_btn = actions_div->addNew<Wt::WPushButton>("Delete");
              del_btn->setStyleClass("kb-comment-del-btn");
              del_btn->clicked().connect(
                [this, comment_id, e_author, is_own] {
                    auto* d      = new Wt::WDialog("Delete Comment");
                    const std::string msg =
                      is_own
                        ? "Are you sure you want to delete this comment?"
                        : "This comment was written by " + e_author +
                          ". Are you sure you want to delete it?";
                    d->contents()->addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
                    auto* yes = d->footer()->addNew<Wt::WPushButton>("Delete");
                    yes->setStyleClass("editor-btn editor-btn-danger");
                    auto* no  = d->footer()->addNew<Wt::WPushButton>("Cancel");
                    no->setStyleClass("editor-btn editor-btn-cancel");
                    yes->clicked().connect([this, d, comment_id] {
                        d->accept();
                        m_db.delete_comment(comment_id, m_username);
                        live_hub::instance().broadcast(
                          "task:" + std::to_string(m_task_id) + ":comments");
                        rebuild_comments();
                    });
                    no->clicked().connect([d] { d->reject(); });
                    d->finished().connect([d](Wt::DialogCode) { delete d; });
                    d->show();
                });
          }
      }
  }
  ```

  Then call `rebuild_comments()` at the end of the constructor (after the live_hub subscription block), inside the `if(existing)` guard:

  ```cpp
  if(existing)
  {
      rebuild_comments();
  }
  ```

- [ ] **Step 8: Build the full application**

  Run: `cmake --build build --parallel $(nproc) 2>&1 | tail -20`

  Expected: Zero errors. The binary is built at `./build/altinf`.

- [ ] **Step 9: Smoke test the UI manually**

  Start the server:
  ```bash
  ./build/altinf \
    --docroot ./build/resources \
    --http-address 0.0.0.0 --http-port 8080 \
    --https-address 0.0.0.0 --https-port 8443 \
    --ssl-certificate ./certs/cert.pem \
    --ssl-private-key ./certs/key.pem \
    --ssl-tmp-dh ./certs/dh.pem
  ```

  Open a task editor, verify:
  - Comments section appears below Type chips
  - Compose area present (textarea + Post button)
  - Post button disabled when textarea empty; enabled after typing
  - Posting a comment shows it in the list with author/timestamp
  - Markdown renders (type `**bold**`, verify `<strong>` in DOM)
  - Edit button opens inline textarea; Save updates the comment; "Edited by" label appears
  - Delete button shows confirmation dialog; confirming shows the placeholder

- [ ] **Step 10: Commit**

  ```bash
  git add src/pages/kanban_task_editor_page.hpp src/pages/kanban_task_editor_page.cpp
  git commit -m "feat(comments): add comment section to task editor with live updates"
  ```

---

## Task 7: Write and run E2E tests

**Files:**
- Create: `e2e/specs/comments.spec.ts`

- [ ] **Step 1: Create `e2e/specs/comments.spec.ts`**

  ```typescript
  import { test, expect, type Page } from '@playwright/test';

  test.describe.configure({ mode: 'serial' });

  const ORG  = 'CommentOrg';
  const TEAM = 'CommentTeam';

  async function loginAs(page: Page, username: string, password: string) {
    await page.goto('/?_=/login');
    await page.locator('input[placeholder="Username"]').fill(username);
    await page.locator('input[placeholder="Password"]').fill(password);
    await page.locator('.login-btn').click();
    await expect(page.locator('.nav-logout')).toBeVisible();
  }

  async function goToBoard(page: Page) {
    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await expect(page.locator('.org-admin-page')).toBeVisible();
    await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
    await expect(page.locator('.org-landing-page')).toBeVisible();
    await page.locator('.org-team-link', { hasText: TEAM }).click();
    await expect(page.locator('.kb-board')).toBeVisible();
  }

  async function createTask(page: Page, title: string) {
    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
    await page.locator('input[placeholder="Task title"]').fill(title);
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible();
  }

  async function openTaskEditor(page: Page, title: string) {
    await page.locator('.kb-card', { hasText: title }).first().locator('.kb-card-edit').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
  }

  async function postComment(page: Page, body: string) {
    await page.locator('.kb-comment-compose textarea').fill(body);
    await page.locator('.kb-comment-post-btn').click();
    // Wait for the comment to appear in the list
    await expect(page.locator('.kb-comment-item').last()).toBeVisible();
  }

  test.beforeAll(async ({ browser }) => {
    const page = await browser.newPage();
    await loginAs(page, 'admin', 'testpass');

    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await expect(page.locator('.org-admin-page')).toBeVisible();

    const alreadyExists = await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).isVisible();
    if (!alreadyExists) {
      await page.locator('input[placeholder="Organization name"]').fill(ORG);
      await page.locator('.org-create-form .editor-btn').click();
      await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
    }

    await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
    await page.getByRole('link', { name: 'Manage organization' }).click();
    await expect(page.locator('.kb-team-page')).toBeVisible();

    const teamExists = await page.locator('.kb-team-block:has(input[value="' + TEAM + '"])').isVisible();
    if (!teamExists) {
      await page.locator('input[placeholder="Team name"]').fill(TEAM);
      await page.getByRole('button', { name: 'Create' }).click();
      await expect(page.locator('.kb-team-block:has(input[value="' + TEAM + '"])')).toBeVisible();
    }

    await page.close();
  });

  // ── Test 1: Post a comment ────────────────────────────────────────────────────

  test('comments: posting a comment renders it in the list', async ({ browser }) => {
    const ctx  = await browser.newContext();
    const page = await ctx.newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToBoard(page);
    await createTask(page, 'CommentTask1');
    await openTaskEditor(page, 'CommentTask1');

    // Post button is disabled while textarea is empty
    await expect(page.locator('.kb-comment-post-btn')).toBeDisabled();

    // Type a comment and post
    await postComment(page, 'Hello **world**');

    // Comment appears in the list
    await expect(page.locator('.kb-comment-item')).toBeVisible();
    // Author label present
    await expect(page.locator('.kb-comment-author', { hasText: 'admin' })).toBeVisible();
    // Markdown rendered: **world** → <strong>
    const bodyHtml = await page.locator('.kb-comment-body').first().innerHTML();
    expect(bodyHtml).toContain('<strong>');

    // Textarea is cleared after posting
    await expect(page.locator('.kb-comment-compose textarea')).toHaveValue('');

    await ctx.close();
  });

  // ── Test 2: Edit own comment ──────────────────────────────────────────────────

  test('comments: editing own comment updates body and shows "Edited by" label', async ({ browser }) => {
    const ctx  = await browser.newContext();
    const page = await ctx.newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToBoard(page);
    await openTaskEditor(page, 'CommentTask1');

    // Click Edit on the first comment (no confirmation dialog for own comment)
    await page.locator('.kb-comment-item').first().locator('button', { hasText: 'Edit' }).click();

    // Inline textarea appears
    await expect(page.locator('.kb-comment-edit-area textarea')).toBeVisible();
    await page.locator('.kb-comment-edit-area textarea').fill('Revised body');
    await page.locator('.kb-comment-edit-area button', { hasText: 'Save' }).click();

    // Updated body visible
    await expect(page.locator('.kb-comment-body').first()).toContainText('Revised body');
    // "Edited by" metadata
    await expect(page.locator('.kb-comment-meta').first()).toContainText('Edited by admin');

    await ctx.close();
  });

  // ── Test 3: Delete own comment ────────────────────────────────────────────────

  test('comments: deleting own comment shows placeholder', async ({ browser }) => {
    const ctx  = await browser.newContext();
    const page = await ctx.newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToBoard(page);
    await createTask(page, 'CommentTask2');
    await openTaskEditor(page, 'CommentTask2');
    await postComment(page, 'Delete me');

    await page.locator('.kb-comment-item').first().locator('button', { hasText: 'Delete' }).click();

    // Confirmation dialog — standard message (own comment, no author callout)
    await expect(page.getByRole('dialog')).toBeVisible();
    await expect(page.getByRole('dialog')).toContainText('Are you sure you want to delete this comment?');
    await expect(page.getByRole('dialog')).not.toContainText('written by');
    await page.getByRole('dialog').locator('button', { hasText: 'Delete' }).click();

    // Placeholder appears; no comment-item remains
    await expect(page.locator('.kb-comment-deleted').first()).toBeVisible();
    await expect(page.locator('.kb-comment-deleted').first()).toContainText('Comment deleted by admin');
    await expect(page.locator('.kb-comment-item')).not.toBeVisible();

    await ctx.close();
  });

  // ── Test 4: Archived task — compose hidden, comments visible ─────────────────

  test('comments: archived task hides compose area; existing comments remain visible', async ({ browser }) => {
    const ctx  = await browser.newContext();
    const page = await ctx.newPage();
    await loginAs(page, 'admin', 'testpass');
    await goToBoard(page);
    await createTask(page, 'CommentTaskArchive');
    await openTaskEditor(page, 'CommentTaskArchive');
    await postComment(page, 'Pre-archive comment');

    // Archive the task
    await page.locator('.editor-btn-danger', { hasText: 'Archive' }).click();
    await expect(page.locator('.kb-board')).toBeVisible();

    // Navigate to archive page and open the archived task
    await page.locator('.kb-manage-link', { hasText: 'Archived' }).click();
    await expect(page.locator('.kb-archive-page')).toBeVisible();
    await page.locator('.kb-archive-title', { hasText: 'CommentTaskArchive' }).first().click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();

    // Compose area hidden
    await expect(page.locator('.kb-comment-compose')).not.toBeVisible();
    // Comment still visible
    await expect(page.locator('.kb-comment-item')).toBeVisible();
    await expect(page.locator('.kb-comment-body').first()).toContainText('Pre-archive comment');

    await ctx.close();
  });

  // ── Test 5: Live update — add ─────────────────────────────────────────────────

  test('comments: comment posted in session B appears in session A without reload', async ({ browser }) => {
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const pageA = await ctxA.newPage();
    const pageB = await ctxB.newPage();

    await loginAs(pageA, 'admin', 'testpass');
    await loginAs(pageB, 'admin', 'testpass');

    // Both open the same task editor
    await goToBoard(pageA);
    await createTask(pageA, 'LiveCommentTask');
    await openTaskEditor(pageA, 'LiveCommentTask');
    await goToBoard(pageB);
    await openTaskEditor(pageB, 'LiveCommentTask');

    // B posts a comment
    await postComment(pageB, 'From session B');

    // A sees the comment without reloading
    await expect(pageA.locator('.kb-comment-item', { hasText: 'From session B' })).toBeVisible();

    await ctxA.close();
    await ctxB.close();
  });

  // ── Test 6: Live update — edit ────────────────────────────────────────────────

  test('comments: comment edited in session B updates in session A without reload', async ({ browser }) => {
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const pageA = await ctxA.newPage();
    const pageB = await ctxB.newPage();

    await loginAs(pageA, 'admin', 'testpass');
    await loginAs(pageB, 'admin', 'testpass');

    // A posts a comment, then both open the same editor
    await goToBoard(pageA);
    await openTaskEditor(pageA, 'LiveCommentTask');
    await postComment(pageA, 'Original from A');

    await goToBoard(pageB);
    await openTaskEditor(pageB, 'LiveCommentTask');

    // B edits the comment
    await pageB.locator('.kb-comment-item', { hasText: 'Original from A' })
      .locator('button', { hasText: 'Edit' }).click();
    await pageB.locator('.kb-comment-edit-area textarea').fill('Edited by B');
    await pageB.locator('.kb-comment-edit-area button', { hasText: 'Save' }).click();

    // A sees the updated body and "Edited by" label without reloading
    await expect(pageA.locator('.kb-comment-body').first()).toContainText('Edited by B');
    await expect(pageA.locator('.kb-comment-meta').first()).toContainText('Edited by admin');

    await ctxA.close();
    await ctxB.close();
  });

  // ── Test 7: Live update — delete ─────────────────────────────────────────────

  test('comments: comment deleted in session B shows placeholder in session A without reload', async ({ browser }) => {
    const ctxA = await browser.newContext();
    const ctxB = await browser.newContext();
    const pageA = await ctxA.newPage();
    const pageB = await ctxB.newPage();

    await loginAs(pageA, 'admin', 'testpass');
    await loginAs(pageB, 'admin', 'testpass');

    await goToBoard(pageA);
    await openTaskEditor(pageA, 'LiveCommentTask');
    await goToBoard(pageB);
    await openTaskEditor(pageB, 'LiveCommentTask');

    // B deletes the "Edited by B" comment from the previous test
    await pageB.locator('.kb-comment-item', { hasText: 'Edited by B' })
      .locator('button', { hasText: 'Delete' }).click();
    await pageB.getByRole('dialog').locator('button', { hasText: 'Delete' }).click();

    // A sees the placeholder without reloading
    await expect(pageA.locator('.kb-comment-deleted')).toBeVisible();

    await ctxA.close();
    await ctxB.close();
  });
  ```

- [ ] **Step 2: Run the E2E tests**

  Run: `npx playwright test e2e/specs/comments.spec.ts --reporter=list`

  Expected: All 7 tests pass.

- [ ] **Step 3: Run the full test suite**

  Run:
  ```bash
  cmake --build build --parallel $(nproc) && \
  ctest --test-dir build --output-on-failure && \
  npx playwright test --reporter=list
  ```

  Expected: All Catch2 tests and all Playwright tests pass.

- [ ] **Step 4: Commit**

  ```bash
  git add e2e/specs/comments.spec.ts
  git commit -m "test(comments): add E2E tests for post, edit, delete, archive, live updates"
  ```
