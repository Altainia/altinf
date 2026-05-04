# Live Hub & Dynamic Updates — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the per-user `notification_hub` with a channel-based `live_hub`, then wire every page that displays mutable data to subscribe/broadcast so changes made by one user appear immediately in every other open session.

**Architecture:** A singleton `live_hub` maps string channel keys to lists of (session_id, callback) entries. Pages subscribe in their constructors and unsubscribe in their destructors. Mutations call `broadcast(channel)`, which posts the stored callback into each subscribed session's event loop via `WServer::post()`.

**Tech Stack:** Wt C++ (WContainerWidget, WApplication, WServer::post, triggerUpdate), Wt Dbo (StaleObjectException), Catch2 (unit tests), Playwright (E2E).

---

## File Map

| Action | File |
|--------|------|
| Create | `src/widgets/live_hub.hpp` |
| Create | `src/widgets/live_hub.cpp` |
| Delete | `src/widgets/notification_hub.hpp` |
| Delete | `src/widgets/notification_hub.cpp` |
| Create | `tests/test_live_hub.cpp` |
| Modify | `tests/CMakeLists.txt` |
| Modify | `src/altinf_app.cpp` |
| Modify | `src/pages/kanban_team_page.hpp` |
| Modify | `src/pages/kanban_team_page.cpp` |
| Modify | `src/pages/kanban_task_editor_page.hpp` |
| Modify | `src/pages/kanban_task_editor_page.cpp` |
| Modify | `src/pages/kanban_board_page.hpp` |
| Modify | `src/pages/kanban_board_page.cpp` |
| Modify | `src/kanban/gantt_view_widget.hpp` |
| Modify | `src/kanban/gantt_view_widget.cpp` |
| Modify | `src/kanban/kanban_db.hpp` |
| Modify | `src/kanban/kanban_db.cpp` |
| Modify | `src/pages/org_landing_page.hpp` |
| Modify | `src/pages/org_landing_page.cpp` |
| Modify | `src/pages/account_manager_page.hpp` |
| Modify | `src/pages/account_manager_page.cpp` |
| Modify | `src/pages/notifications_page.cpp` |
| Modify | `src/pages/account_editor_page.cpp` |
| Create | `e2e/specs/live-board.spec.ts` |
| Create | `e2e/specs/live-task-editor.spec.ts` |
| Create | `e2e/specs/live-org-manage.spec.ts` |
| Create | `e2e/specs/live-org-landing.spec.ts` |
| Create | `e2e/specs/live-nav-accounts.spec.ts` |

---

## Task 1: Write failing Catch2 tests for `live_hub`

**Files:**
- Create: `tests/test_live_hub.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write `tests/test_live_hub.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "widgets/live_hub.hpp"

static live_hub::post_fn_t sync_post()
{
    return [](const std::string&, std::function<void()> fn) { fn(); };
}

TEST_CASE("live_hub - broadcast fires a subscribed callback")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    int count = 0;
    hub.subscribe("org:5", "sid1", [&count] { ++count; });
    hub.broadcast("org:5");

    CHECK(count == 1);
}

TEST_CASE("live_hub - broadcast is a no-op for unknown channel")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    CHECK_NOTHROW(hub.broadcast("org:999"));
}

TEST_CASE("live_hub - broadcast fires all sessions on the same channel")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    int a = 0, b = 0;
    hub.subscribe("org:5", "sid1", [&a] { ++a; });
    hub.subscribe("org:5", "sid2", [&b] { ++b; });
    hub.broadcast("org:5");

    CHECK(a == 1);
    CHECK(b == 1);
}

TEST_CASE("live_hub - broadcast does not fire other channels")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    int org5 = 0, org6 = 0;
    hub.subscribe("org:5", "sid1", [&org5] { ++org5; });
    hub.subscribe("org:6", "sid2", [&org6] { ++org6; });
    hub.broadcast("org:5");

    CHECK(org5 == 1);
    CHECK(org6 == 0);
}

TEST_CASE("live_hub - broadcast does not fire different channel types")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    int user_count = 0, org_count = 0;
    hub.subscribe("user:alice", "sid1", [&user_count] { ++user_count; });
    hub.subscribe("org:5",      "sid2", [&org_count]  { ++org_count; });
    hub.broadcast("org:5");

    CHECK(user_count == 0);
    CHECK(org_count  == 1);
}

TEST_CASE("live_hub - unsubscribe stops that session from firing")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    int count = 0;
    hub.subscribe("org:5", "sid1", [&count] { ++count; });
    hub.unsubscribe("org:5", "sid1");
    hub.broadcast("org:5");

    CHECK(count == 0);
}

TEST_CASE("live_hub - unsubscribe removes only the named session")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    int a = 0, b = 0;
    hub.subscribe("org:5", "sid1", [&a] { ++a; });
    hub.subscribe("org:5", "sid2", [&b] { ++b; });
    hub.unsubscribe("org:5", "sid1");
    hub.broadcast("org:5");

    CHECK(a == 0);
    CHECK(b == 1);
}

TEST_CASE("live_hub - unsubscribe on unknown session is a no-op")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    CHECK_NOTHROW(hub.unsubscribe("org:999", "ghost"));
}

TEST_CASE("live_hub - broadcast can be called multiple times")
{
    auto& hub = live_hub::instance();
    hub.reset(sync_post());

    int count = 0;
    hub.subscribe("org:5", "sid1", [&count] { ++count; });
    hub.broadcast("org:5");
    hub.broadcast("org:5");

    CHECK(count == 2);
}
```

- [ ] **Step 2: Update `tests/CMakeLists.txt` — replace the `test_notification_hub` block**

Replace the block:
```cmake
# test_notification_hub — notification_hub with injected synchronous dispatcher
add_executable(test_notification_hub test_notification_hub.cpp
  ${CMAKE_SOURCE_DIR}/src/widgets/notification_hub.cpp)
target_include_directories(test_notification_hub PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_notification_hub PRIVATE Catch2::Catch2WithMain Wt::Wt)
catch_discover_tests(test_notification_hub)
```

With:
```cmake
# test_live_hub — live_hub with injected synchronous dispatcher
add_executable(test_live_hub test_live_hub.cpp
  ${CMAKE_SOURCE_DIR}/src/widgets/live_hub.cpp)
target_include_directories(test_live_hub PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_live_hub PRIVATE Catch2::Catch2WithMain Wt::Wt)
catch_discover_tests(test_live_hub)
```

- [ ] **Step 3: Build and confirm the tests fail to compile** (live_hub.hpp does not exist yet)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -E "error|warning" | head -20
```

Expected: compile error — `widgets/live_hub.hpp` not found.

- [ ] **Step 4: Commit**

```bash
git add tests/test_live_hub.cpp tests/CMakeLists.txt
git commit -m "test: write failing Catch2 tests for live_hub"
```

---

## Task 2: Implement `live_hub`, delete `notification_hub`

**Files:**
- Create: `src/widgets/live_hub.hpp`
- Create: `src/widgets/live_hub.cpp`
- Delete: `src/widgets/notification_hub.hpp`
- Delete: `src/widgets/notification_hub.cpp`
- Delete: `tests/test_notification_hub.cpp`

- [ ] **Step 1: Create `src/widgets/live_hub.hpp`**

```cpp
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// Thread-safe pub/sub hub keyed on channel strings.
// Channels follow the convention "type:id", e.g. "org:5", "user:alice".
// Subscribe from within the target session's event loop.
// Broadcast is thread-safe and posts each callback via WServer::post().
class live_hub
{
public:
    using post_fn_t =
      std::function<void(const std::string& session_id, std::function<void()> fn)>;

    static live_hub& instance();

    // Replace dispatch function and clear all subscriptions. For test setup only.
    void reset(post_fn_t post_fn = {});

    // Subscribe a session callback to a channel.
    void subscribe(const std::string&    channel,
                   const std::string&    session_id,
                   std::function<void()> fn);

    // Remove a session's subscription from a channel.
    void unsubscribe(const std::string& channel,
                     const std::string& session_id);

    // Post fn into every session subscribed to channel. Thread-safe.
    void broadcast(const std::string& channel);

private:
    live_hub() = default;

    struct entry
    {
        std::string           session_id;
        std::function<void()> update_fn;
    };

    std::mutex                                m_mutex;
    std::map<std::string, std::vector<entry>> m_sessions; // channel → entries
    post_fn_t                                 m_post_fn;  // null = use WServer::post
};
```

- [ ] **Step 2: Create `src/widgets/live_hub.cpp`**

```cpp
#include "live_hub.hpp"

#include <Wt/WServer.h>

live_hub& live_hub::instance()
{
    static live_hub hub;
    return hub;
}

void live_hub::reset(post_fn_t post_fn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.clear();
    m_post_fn = std::move(post_fn);
}

void live_hub::subscribe(const std::string&    channel,
                         const std::string&    session_id,
                         std::function<void()> update_fn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions[channel].push_back({session_id, std::move(update_fn)});
}

void live_hub::unsubscribe(const std::string& channel,
                           const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto                        it = m_sessions.find(channel);
    if(it == m_sessions.end())
    {
        return;
    }
    auto& v = it->second;
    v.erase(std::remove_if(v.begin(),
                           v.end(),
                           [&](const entry& e) { return e.session_id == session_id; }),
            v.end());
    if(v.empty())
    {
        m_sessions.erase(it);
    }
}

void live_hub::broadcast(const std::string& channel)
{
    std::vector<entry> entries;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto                        it = m_sessions.find(channel);
        if(it == m_sessions.end())
        {
            return;
        }
        entries = it->second;
    }
    for(const auto& e: entries)
    {
        if(m_post_fn)
        {
            m_post_fn(e.session_id, e.update_fn);
        }
        else
        {
            Wt::WServer::instance()->post(e.session_id, e.update_fn);
        }
    }
}
```

- [ ] **Step 3: Delete the old files**

```bash
rm src/widgets/notification_hub.hpp src/widgets/notification_hub.cpp tests/test_notification_hub.cpp
```

- [ ] **Step 4: Build — expect link errors because notification_hub call sites still reference it**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -20
```

Expected: errors in `altinf_app.cpp`, `kanban_team_page.cpp`, `kanban_task_editor_page.cpp` — they still include the deleted header.

- [ ] **Step 5: Run the live_hub tests** (only the test_live_hub target, since the main app won't link yet)

```bash
cmake --build build --target test_live_hub --parallel $(nproc) && cd build && ctest -R test_live_hub --output-on-failure
```

Expected: 9 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/widgets/live_hub.hpp src/widgets/live_hub.cpp
git rm src/widgets/notification_hub.hpp src/widgets/notification_hub.cpp tests/test_notification_hub.cpp
git commit -m "feat: implement live_hub; remove notification_hub"
```

---

## Task 3: Migrate `altinf_app.cpp` to `live_hub`

**Files:**
- Modify: `src/altinf_app.cpp`

The file has four notification_hub call sites. Also update the `register_with_hub()` callback to call `m_nav->update()` (not `refresh_bell()`) so that org-selector changes propagate on any user-channel broadcast.

- [ ] **Step 1: Replace the include**

In `src/altinf_app.cpp`, line 31:

Old:
```cpp
#include "widgets/notification_hub.hpp"
```
New:
```cpp
#include "widgets/live_hub.hpp"
```

- [ ] **Step 2: Update the destructor (line 63)**

Old:
```cpp
notification_hub::instance().deregister_session(m_session.username, sessionId());
```
New:
```cpp
live_hub::instance().unsubscribe("user:" + m_session.username, sessionId());
```

- [ ] **Step 3: Update `register_with_hub()` (lines 111–124)**

Old:
```cpp
void altinf_app::register_with_hub()
{
    notification_hub::instance().register_session(
      m_session.username,
      sessionId(),
      [this] {
          m_nav->refresh_bell();
          if(m_notifications_page)
          {
              m_notifications_page->refresh();
          }
          triggerUpdate();
      });
}
```
New:
```cpp
void altinf_app::register_with_hub()
{
    live_hub::instance().subscribe(
      "user:" + m_session.username,
      sessionId(),
      [this] {
          m_nav->update();
          if(m_notifications_page)
          {
              m_notifications_page->refresh();
          }
          triggerUpdate();
      });
}
```

- [ ] **Step 4: Update the logout handler (line 535)**

Old:
```cpp
notification_hub::instance().deregister_session(m_session.username, sessionId());
```
New:
```cpp
live_hub::instance().unsubscribe("user:" + m_session.username, sessionId());
```

- [ ] **Step 5: Build (will still fail on kanban_team_page.cpp and kanban_task_editor_page.cpp)**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:" | head -10
```

Expected: two remaining files still include `notification_hub.hpp`.

- [ ] **Step 6: Commit**

```bash
git add src/altinf_app.cpp
git commit -m "refactor: migrate altinf_app to live_hub"
```

---

## Task 4: Migrate `kanban_team_page` and `kanban_task_editor_page` to `live_hub`

**Files:**
- Modify: `src/pages/kanban_team_page.cpp`
- Modify: `src/pages/kanban_task_editor_page.cpp`

This is a mechanical substitution only — broadcasts for org/team channels are added in Task 8.

- [ ] **Step 1: Update `kanban_team_page.cpp` include and all 6 call sites**

Replace the include at line 11:
```cpp
// Old:
#include "widgets/notification_hub.hpp"
// New:
#include "widgets/live_hub.hpp"
```

Replace all 6 occurrences of `notification_hub::instance().notify_user(u)` with `live_hub::instance().broadcast("user:" + u)`. The six lines are:
- Line 86: `notification_hub::instance().notify_user(u)` (invite sent)
- Line 164: `notification_hub::instance().notify_user(uid)` (lead toggled)
- Line 219: `notification_hub::instance().notify_user(uid)` (invite withdrawn)
- Line 298: `notification_hub::instance().notify_user(uid)` (team lead toggled)
- Line 309: `notification_hub::instance().notify_user(uid)` (team member removed)
- Line 363: `notification_hub::instance().notify_user(u)` (team member added)

Apply the same pattern to each:
```cpp
// Old (example):
notification_hub::instance().notify_user(u);
// New:
live_hub::instance().broadcast("user:" + u);
```

- [ ] **Step 2: Update `kanban_task_editor_page.cpp` include and call site**

Replace the include at line 14:
```cpp
// Old:
#include "widgets/notification_hub.hpp"
// New:
#include "widgets/live_hub.hpp"
```

Replace the single call site at line 314:
```cpp
// Old:
notification_hub::instance().notify_user(new_assignee);
// New:
live_hub::instance().broadcast("user:" + new_assignee);
```

- [ ] **Step 3: Build and confirm everything compiles cleanly**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Run all C++ tests**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/pages/kanban_team_page.cpp src/pages/kanban_task_editor_page.cpp
git commit -m "refactor: migrate kanban pages to live_hub"
```

---

## Task 5: Add `gantt_view_widget::refresh()`

**Files:**
- Modify: `src/kanban/gantt_view_widget.hpp`
- Modify: `src/kanban/gantt_view_widget.cpp`

- [ ] **Step 1: Add `refresh()` declaration to `src/kanban/gantt_view_widget.hpp`**

After the constructor declaration, add:
```cpp
void refresh(std::vector<kanban_task_entry> tasks);
```

Full updated header:
```cpp
#pragma once

#include "kanban.hpp"

#include <Wt/WContainerWidget.h>

#include <vector>

class gantt_view_widget: public Wt::WContainerWidget
{
public:
    explicit gantt_view_widget(std::vector<kanban_task_entry> tasks);

    void refresh(std::vector<kanban_task_entry> tasks);

private:
    static std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks);
};
```

- [ ] **Step 2: Implement `refresh()` in `src/kanban/gantt_view_widget.cpp`**

Add after the constructor:
```cpp
void gantt_view_widget::refresh(std::vector<kanban_task_entry> tasks)
{
    doJavaScript("initGantt('" + id() + "'," + serialize_tasks(tasks) + ");");
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add src/kanban/gantt_view_widget.hpp src/kanban/gantt_view_widget.cpp
git commit -m "feat: add gantt_view_widget::refresh()"
```

---

## Task 6: Add live refresh to `kanban_board_page`

**Files:**
- Modify: `src/pages/kanban_board_page.hpp`
- Modify: `src/pages/kanban_board_page.cpp`

The board page subscribes to `"team:{id}"` and refreshes whichever widget (board or gantt) is active. The drag-drop callback also broadcasts after updating the DB so other sessions see the card move.

- [ ] **Step 1: Update `src/pages/kanban_board_page.hpp`**

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/kanban_board_widget.hpp"
#include "kanban/gantt_view_widget.hpp"

#include <Wt/WContainerWidget.h>

#include <string>

class kanban_board_page: public Wt::WContainerWidget
{
public:
    kanban_board_page(kanban_db&          db,
                      const session_data& session,
                      long long           team_id,
                      bool                is_lead,
                      bool                show_gantt);

    ~kanban_board_page() override;

private:
    kanban_db&           m_db;
    long long            m_team_id{0};
    bool                 m_is_lead{false};
    bool                 m_show_gantt{false};
    std::string          m_session_id;
    kanban_board_widget* m_board_widget{nullptr};
    gantt_view_widget*   m_gantt_widget{nullptr};

    void refresh();
};
```

- [ ] **Step 2: Update `src/pages/kanban_board_page.cpp`**

Full replacement:
```cpp
#include "kanban_board_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "kanban/gantt_view_widget.hpp"
#include "widgets/live_hub.hpp"

kanban_board_page::kanban_board_page(kanban_db&          db,
                                     const session_data& session,
                                     long long           team_id,
                                     bool                is_lead,
                                     bool                show_gantt):
  m_db{db},
  m_team_id{team_id},
  m_is_lead{is_lead},
  m_show_gantt{show_gantt}
{
    setStyleClass("page kb-page");

    const auto team = db.find_team(team_id);
    if(!team)
    {
        addNew<Wt::WText>("Team not found.", Wt::TextFormat::Plain);
        return;
    }

    const std::string team_url = "/board/" + std::to_string(team_id);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* hdr = addNew<Wt::WContainerWidget>();
    hdr->setStyleClass("kb-page-hdr");

    hdr->addNew<Wt::WText>("<h1>" + team->name + "</h1>",
                           Wt::TextFormat::UnsafeXHTML);

    auto* tabs = hdr->addNew<Wt::WContainerWidget>();
    tabs->setStyleClass("kb-view-tabs");

    tabs->addNew<Wt::WAnchor>(
          Wt::WLink{Wt::LinkType::InternalPath, team_url}, "Board")
      ->setStyleClass(show_gantt ? "kb-tab" : "kb-tab kb-tab--active");

    tabs->addNew<Wt::WAnchor>(
          Wt::WLink{Wt::LinkType::InternalPath, team_url + "/gantt"}, "Gantt")
      ->setStyleClass(show_gantt ? "kb-tab kb-tab--active" : "kb-tab");

    if(m_is_lead)
    {
        hdr->addNew<Wt::WAnchor>(
             Wt::WLink{Wt::LinkType::InternalPath, team_url + "/task/new"},
             "+ New Task")
          ->setStyleClass("editor-btn kb-new-btn");

        hdr->addNew<Wt::WAnchor>(
             Wt::WLink{Wt::LinkType::InternalPath, team_url + "/manage"},
             "Manage Team")
          ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
    }

    // ── Content ───────────────────────────────────────────────────────────────
    const auto tasks = db.tasks_for_team(team_id);

    if(show_gantt)
    {
        m_gantt_widget = addNew<gantt_view_widget>(tasks);
    }
    else
    {
        m_board_widget = addNew<kanban_board_widget>(
          tasks,
          m_is_lead,
          [this](long long tid, const std::string& status, int sort) {
              m_db.update_task_status(tid, status, sort);
              live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
              m_board_widget->refresh(m_db.tasks_for_team(m_team_id), m_is_lead);
          },
          [this](long long tid) {
              Wt::WApplication::instance()->setInternalPath(
                "/board/" + std::to_string(m_team_id) + "/task/" +
                  std::to_string(tid) + "/edit",
                true);
          });
    }

    // Subscribe to live updates for this team.
    m_session_id = Wt::WApplication::instance()->sessionId();
    live_hub::instance().subscribe(
      "team:" + std::to_string(m_team_id),
      m_session_id,
      [this] { refresh(); triggerUpdate(); });
}

kanban_board_page::~kanban_board_page()
{
    if(!m_session_id.empty())
    {
        live_hub::instance().unsubscribe(
          "team:" + std::to_string(m_team_id), m_session_id);
    }
}

void kanban_board_page::refresh()
{
    const auto tasks = m_db.tasks_for_team(m_team_id);
    if(m_show_gantt && m_gantt_widget)
    {
        m_gantt_widget->refresh(tasks);
    }
    else if(m_board_widget)
    {
        m_board_widget->refresh(tasks, m_is_lead);
    }
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add src/pages/kanban_board_page.hpp src/pages/kanban_board_page.cpp
git commit -m "feat: live refresh for kanban board and gantt pages"
```

---

## Task 7: Add `kanban_db::team_ids_for_user()`

**Files:**
- Modify: `src/kanban/kanban_db.hpp`
- Modify: `src/kanban/kanban_db.cpp`

Needed by `altinf_app` to collect team IDs before deleting a user, so the correct channels can be broadcast after deletion.

- [ ] **Step 1: Add declaration to `src/kanban/kanban_db.hpp`**

In the Members section, after `is_member`:
```cpp
std::vector<long long>         team_ids_for_user(const std::string& username);
```

- [ ] **Step 2: Implement in `src/kanban/kanban_db.cpp`**

Add after `is_member()`:
```cpp
std::vector<long long> kanban_db::team_ids_for_user(const std::string& username)
{
    Wt::Dbo::Transaction t{m_dbo};
    const auto           results = m_dbo.find<team_member_record>()
                           .where("username = ?")
                           .bind(username)
                           .resultList();
    std::vector<long long> out;
    for(const auto& r: results)
    {
        out.push_back(r->team_id);
    }
    return out;
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Run all C++ tests to confirm nothing regressed**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/kanban/kanban_db.hpp src/kanban/kanban_db.cpp
git commit -m "feat: add kanban_db::team_ids_for_user()"
```

---

## Task 8: Add org broadcasts + subscription to `kanban_team_page`

**Files:**
- Modify: `src/pages/kanban_team_page.hpp`
- Modify: `src/pages/kanban_team_page.cpp`

The page subscribes to `"org:{id}"` so that a second lead viewing the same manage page sees changes. All mutation handlers broadcast the relevant channels after their DB call.

- [ ] **Step 1: Update `src/pages/kanban_team_page.hpp`**

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

#include <string>

class kanban_team_page: public Wt::WContainerWidget
{
public:
    kanban_team_page(org_db&             odb,
                     kanban_db&          kdb,
                     user_db&            udb,
                     long long           org_id,
                     const session_data& session,
                     const std::string&  back_url = {});

    ~kanban_team_page() override;

private:
    org_db&               m_odb;
    kanban_db&            m_kdb;
    user_db&              m_udb;
    long long             m_org_id;
    std::string           m_org_name;
    const session_data&   m_session;
    std::string           m_session_id;

    Wt::WContainerWidget* m_members_section{nullptr};
    Wt::WContainerWidget* m_pending_section{nullptr};
    Wt::WLineEdit*        m_invite_input{nullptr};
    Wt::WCheckBox*        m_invite_lead{nullptr};
    Wt::WText*            m_invite_msg{nullptr};
    Wt::WContainerWidget* m_teams_section{nullptr};
    Wt::WLineEdit*        m_new_team_input{nullptr};

    void refresh();
    void refresh_members();
    void refresh_pending();
    void refresh_teams();
    void build_team_block(Wt::WContainerWidget* parent, const team_entry& team);
};
```

- [ ] **Step 2: Add destructor + subscription at end of constructor in `kanban_team_page.cpp`**

At the end of the constructor (just before the closing `}` of the constructor body, after all the widget setup), add:
```cpp
    // Subscribe to live updates for this org.
    m_session_id = Wt::WApplication::instance()->sessionId();
    live_hub::instance().subscribe(
      "org:" + std::to_string(m_org_id),
      m_session_id,
      [this] { refresh(); triggerUpdate(); });
```

Add the destructor definition:
```cpp
kanban_team_page::~kanban_team_page()
{
    if(!m_session_id.empty())
    {
        live_hub::instance().unsubscribe(
          "org:" + std::to_string(m_org_id), m_session_id);
    }
}
```

Add the `refresh()` method definition:
```cpp
void kanban_team_page::refresh()
{
    refresh_members();
    refresh_pending();
    refresh_teams();
}
```

- [ ] **Step 3: Add `#include "widgets/live_hub.hpp"` at the top of kanban_team_page.cpp** (already done in Task 4)

- [ ] **Step 4: Add broadcasts to the invite-sent handler**

In the invite-sent lambda (after `notification_hub::instance().notify_user(u)` was replaced in Task 4), add a broadcast for the org channel. The relevant block currently reads:
```cpp
m_odb.invite_to_org(m_org_id, u, m_invite_lead->isChecked());
live_hub::instance().broadcast("user:" + u);
m_invite_input->setText("");
m_invite_lead->setChecked(false);
m_invite_msg->setText("Invite sent to " + u + ".");
refresh_pending();
```
Add after the `broadcast("user:" + u)` line:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
```

- [ ] **Step 5: Add broadcasts to the invite-withdrawn handler**

The withdraw lambda currently reads:
```cpp
m_odb.rescind_invite_notification(m_org_id, uid);
live_hub::instance().broadcast("user:" + uid);
m_odb.remove_org_member(m_org_id, uid);
refresh_pending();
```
Add after the `broadcast("user:" + uid)` line:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
```

- [ ] **Step 6: Add broadcasts to the lead-toggle handler**

The toggle lambda currently reads:
```cpp
if(!m_odb.set_org_lead(m_org_id, uid, !currently_lead))
{
    m_invite_msg->setText(
      "Cannot demote: organization must have at least one lead.");
}
else
{
    const std::string type =
      currently_lead ? "org_lead_demoted" : "org_lead_promoted";
    m_odb.push_notification(
      uid, type, make_org_event_payload(m_org_id, m_org_name));
    live_hub::instance().broadcast("user:" + uid);
    m_invite_msg->setText("");
}
refresh_members();
```
Add after the `broadcast("user:" + uid)` line:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
```

- [ ] **Step 7: Add broadcasts to the member-removed-from-org handler**

The remove-member lambda must collect team IDs *before* removal. Replace the existing else-branch:
```cpp
// Old else-branch:
else
{
    m_odb.push_notification(
      uid, "org_removed", make_org_event_payload(m_org_id, m_org_name));
    live_hub::instance().broadcast("user:" + uid);
    m_kdb.remove_member_from_org_teams(m_org_id, uid);
    m_invite_msg->setText("");
    refresh_teams();
}
refresh_members();
```
Replace with:
```cpp
else
{
    // Collect team IDs before removal so we can broadcast them afterward.
    const auto org_teams = m_kdb.teams_for_org(m_org_id);
    std::vector<long long> member_team_ids;
    for(const auto& t: org_teams)
    {
        if(m_kdb.is_member(t.id, uid))
        {
            member_team_ids.push_back(t.id);
        }
    }

    m_odb.push_notification(
      uid, "org_removed", make_org_event_payload(m_org_id, m_org_name));
    m_kdb.remove_member_from_org_teams(m_org_id, uid);
    m_invite_msg->setText("");

    live_hub::instance().broadcast("user:" + uid);
    live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
    for(const auto tid: member_team_ids)
    {
        live_hub::instance().broadcast("team:" + std::to_string(tid));
    }

    refresh_teams();
}
refresh_members();
```

- [ ] **Step 8: Add broadcasts to the team-lead-toggle handler**

In `build_team_block`, the lead-toggle lambda currently reads:
```cpp
m_kdb.set_team_lead(tid, uid, !is_lead);
const std::string type =
  is_lead ? "team_lead_demoted" : "team_lead_promoted";
m_odb.push_notification(uid, type, make_team_lead_payload(tid, tname));
live_hub::instance().broadcast("user:" + uid);
refresh_teams();
```
Add after the `broadcast("user:" + uid)` line:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
live_hub::instance().broadcast("team:" + std::to_string(tid));
```

- [ ] **Step 9: Add broadcasts to the remove-from-team handler**

In `build_team_block`, the remove-from-team lambda currently reads:
```cpp
m_kdb.remove_member(tid, uid);
m_odb.push_notification(
  uid, "team_removed", make_team_event_payload(tid, tname, m_org_id, m_org_name));
live_hub::instance().broadcast("user:" + uid);
refresh_teams();
```
Add after the `broadcast("user:" + uid)` line:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
live_hub::instance().broadcast("team:" + std::to_string(tid));
```

- [ ] **Step 10: Add broadcasts to the add-to-team handler**

In `build_team_block`, the add-to-team lambda currently reads:
```cpp
m_kdb.add_member(tid, u);
m_odb.push_notification(
  u, "team_added", make_team_event_payload(tid, tname, m_org_id, m_org_name));
live_hub::instance().broadcast("user:" + u);
refresh_teams();
```
Add after the `broadcast("user:" + u)` line:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
live_hub::instance().broadcast("team:" + std::to_string(tid));
```

- [ ] **Step 11: Add broadcasts to the rename-team handler**

In `build_team_block`, the rename-team lambda currently reads:
```cpp
const std::string n = name_input->text().toUTF8();
if(!n.empty())
{
    m_kdb.rename_team(tid, n);
}
```
Add after `rename_team`:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
live_hub::instance().broadcast("team:" + std::to_string(tid));
```

- [ ] **Step 12: Add broadcasts to the delete-team handler**

In `build_team_block`, the delete-team lambda currently reads:
```cpp
m_kdb.delete_team(tid);
refresh_teams();
```
Add after `delete_team`:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
```

- [ ] **Step 13: Add broadcasts to the create-team handler**

In the constructor, the create-team lambda currently reads:
```cpp
m_kdb.create_team(name, m_org_id);
m_new_team_input->setText("");
refresh_teams();
```
Add after `create_team`:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
```

- [ ] **Step 14: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 15: Run C++ tests**

```bash
cd build && ctest --output-on-failure
```

Expected: all PASS.

- [ ] **Step 16: Commit**

```bash
git add src/pages/kanban_team_page.hpp src/pages/kanban_team_page.cpp
git commit -m "feat: live hub subscription and broadcasts in kanban_team_page"
```

---

## Task 9: Refactor `org_landing_page` for live refresh

**Files:**
- Modify: `src/pages/org_landing_page.hpp`
- Modify: `src/pages/org_landing_page.cpp`

The constructor body is extracted into `render()` so `refresh()` can call `clear(); render();`.

- [ ] **Step 1: Update `src/pages/org_landing_page.hpp`**

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

#include <string>

class org_landing_page: public Wt::WContainerWidget
{
public:
    org_landing_page(org_db&             odb,
                     kanban_db&          kdb,
                     long long           org_id,
                     const session_data& session,
                     bool                is_org_lead);

    ~org_landing_page() override;

private:
    org_db&             m_odb;
    kanban_db&          m_kdb;
    long long           m_org_id;
    const session_data& m_session;
    bool                m_is_org_lead;
    std::string         m_session_id;

    void render();
    void refresh();
};
```

- [ ] **Step 2: Rewrite `src/pages/org_landing_page.cpp`**

```cpp
#include "org_landing_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "widgets/live_hub.hpp"

org_landing_page::org_landing_page(org_db&             odb,
                                   kanban_db&          kdb,
                                   long long           org_id,
                                   const session_data& session,
                                   bool                is_org_lead):
  m_odb{odb},
  m_kdb{kdb},
  m_org_id{org_id},
  m_session{session},
  m_is_org_lead{is_org_lead}
{
    setStyleClass("page org-landing-page");
    render();

    m_session_id = Wt::WApplication::instance()->sessionId();
    live_hub::instance().subscribe(
      "org:" + std::to_string(m_org_id),
      m_session_id,
      [this] { refresh(); triggerUpdate(); });
}

org_landing_page::~org_landing_page()
{
    if(!m_session_id.empty())
    {
        live_hub::instance().unsubscribe(
          "org:" + std::to_string(m_org_id), m_session_id);
    }
}

void org_landing_page::render()
{
    const auto org = m_odb.find_org(m_org_id);
    if(!org)
    {
        addNew<Wt::WText>("Organization not found.", Wt::TextFormat::Plain);
        return;
    }

    addNew<Wt::WText>("<h1>" + org->name + "</h1>", Wt::TextFormat::UnsafeXHTML);

    if(m_is_org_lead)
    {
        auto* actions = addNew<Wt::WContainerWidget>();
        actions->setStyleClass("org-lead-actions");

        actions->addNew<Wt::WAnchor>(
                 Wt::WLink{Wt::LinkType::InternalPath,
                           "/org/" + std::to_string(m_org_id) + "/manage"},
                 "Manage organization")
          ->setStyleClass("editor-btn");

        actions->addNew<Wt::WAnchor>(
                 Wt::WLink{Wt::LinkType::InternalPath,
                           "/org/" + std::to_string(m_org_id) + "/board"},
                 "View all teams\xe2\x80\x99 board")
          ->setStyleClass("editor-btn editor-btn-cancel");
    }

    const auto  all_teams = m_kdb.teams_for_org(m_org_id);
    const auto& username  = m_session.username;

    addNew<Wt::WText>("<h2>Your teams</h2>", Wt::TextFormat::UnsafeXHTML);

    bool has_own = false;
    for(const auto& t: all_teams)
    {
        if(!m_kdb.is_member(t.id, username) && !m_is_org_lead)
        {
            continue;
        }
        has_own   = true;
        auto* row = addNew<Wt::WContainerWidget>();
        row->setStyleClass("org-team-row");
        row->addNew<Wt::WAnchor>(
             Wt::WLink{Wt::LinkType::InternalPath,
                       "/board/" + std::to_string(t.id)},
             t.name)
          ->setStyleClass("org-team-link");
    }
    if(!has_own)
    {
        addNew<Wt::WText>("You are not a member of any team in this organization.",
                          Wt::TextFormat::Plain)
          ->setStyleClass("org-empty-note");
    }

    bool has_other = false;
    for(const auto& t: all_teams)
    {
        if(m_kdb.is_member(t.id, username) || m_is_org_lead)
        {
            continue;
        }
        if(!has_other)
        {
            addNew<Wt::WText>("<h2>Other teams</h2>", Wt::TextFormat::UnsafeXHTML);
            has_other = true;
        }
        auto* row = addNew<Wt::WContainerWidget>();
        row->setStyleClass("org-team-row org-team-row--other");
        row->addNew<Wt::WText>(t.name, Wt::TextFormat::Plain)
          ->setStyleClass("org-team-name");
    }
}

void org_landing_page::refresh()
{
    clear();
    render();
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add src/pages/org_landing_page.hpp src/pages/org_landing_page.cpp
git commit -m "feat: live refresh for org landing page"
```

---

## Task 10: Refactor `account_manager_page` for live refresh

**Files:**
- Modify: `src/pages/account_manager_page.hpp`
- Modify: `src/pages/account_manager_page.cpp`

- [ ] **Step 1: Update `src/pages/account_manager_page.hpp`**

```cpp
#pragma once

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

#include <Wt/WContainerWidget.h>

#include <functional>
#include <string>

class account_manager_page: public Wt::WContainerWidget
{
public:
    account_manager_page(user_db&                                db,
                         const session_data&                     session,
                         std::function<void(const std::string&)> on_delete);

    ~account_manager_page() override;

private:
    user_db&                                m_db;
    const session_data&                     m_session;
    std::function<void(const std::string&)> m_on_delete;
    std::string                             m_session_id;

    void render();
    void refresh();
};
```

- [ ] **Step 2: Rewrite `src/pages/account_manager_page.cpp`**

Extract the constructor body (starting after `setStyleClass(...)`) into `render()`, call `render()` from the constructor, then subscribe, add destructor, add `refresh()`.

Full replacement:
```cpp
#include "account_manager_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WDialog.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>

#include <string>
#include <vector>

#include "auth/permission.hpp"
#include "widgets/live_hub.hpp"

static std::string permissions_label(permission::flags perms)
{
    std::vector<std::string> parts;
    if(perms.has_any(permission::admin))        parts.emplace_back("Admin");
    if(perms.has_any(permission::manage_users)) parts.emplace_back("Manage Users");
    if(perms.has_any(permission::post_write))   parts.emplace_back("Write Posts");
    if(perms.has_any(permission::org_create))   parts.emplace_back("Create Orgs");
    if(parts.empty()) return "None";
    std::string out;
    for(std::size_t i = 0; i < parts.size(); ++i)
    {
        if(i) out += ", ";
        out += parts[i];
    }
    return out;
}

account_manager_page::account_manager_page(
  user_db&                                db,
  const session_data&                     session,
  std::function<void(const std::string&)> on_delete):
  m_db{db},
  m_session{session},
  m_on_delete{std::move(on_delete)}
{
    setStyleClass("page account-manager-page");
    render();

    m_session_id = Wt::WApplication::instance()->sessionId();
    live_hub::instance().subscribe(
      "accounts",
      m_session_id,
      [this] { refresh(); triggerUpdate(); });
}

account_manager_page::~account_manager_page()
{
    if(!m_session_id.empty())
    {
        live_hub::instance().unsubscribe("accounts", m_session_id);
    }
}

void account_manager_page::render()
{
    addNew<Wt::WText>("<h1>Accounts</h1>", Wt::TextFormat::UnsafeXHTML);

    auto* new_btn = addNew<Wt::WPushButton>("New User");
    new_btn->setStyleClass("editor-btn account-new-btn");
    new_btn->clicked().connect([] {
        Wt::WApplication::instance()->setInternalPath("/admin/accounts/new", true);
    });

    const auto users = m_db.list_users();
    if(users.empty())
    {
        addNew<Wt::WText>("<p class=\"accounts-empty\">No users found.</p>",
                          Wt::TextFormat::UnsafeXHTML);
        return;
    }

    auto* tbl = addNew<Wt::WTable>();
    tbl->setStyleClass("account-table");
    tbl->setHeaderCount(1);

    tbl->elementAt(0, 0)->addNew<Wt::WText>("Username",     Wt::TextFormat::Plain);
    tbl->elementAt(0, 1)->addNew<Wt::WText>("Display Name", Wt::TextFormat::Plain);
    tbl->elementAt(0, 2)->addNew<Wt::WText>("Permissions",  Wt::TextFormat::Plain);
    tbl->elementAt(0, 3)->addNew<Wt::WText>("Actions",      Wt::TextFormat::Plain);

    for(int row = 0; row < static_cast<int>(users.size()); ++row)
    {
        const auto& u     = users[row];
        const bool  is_me = (u.username == m_session.username);

        tbl->elementAt(row + 1, 0)->addNew<Wt::WText>(u.username, Wt::TextFormat::Plain);
        tbl->elementAt(row + 1, 1)->addNew<Wt::WText>(u.display_name.empty() ? "\xe2\x80\x94" : u.display_name, Wt::TextFormat::Plain);
        tbl->elementAt(row + 1, 2)->addNew<Wt::WText>(permissions_label(u.permissions), Wt::TextFormat::Plain);

        auto* actions = tbl->elementAt(row + 1, 3)->addNew<Wt::WContainerWidget>();
        actions->setStyleClass("account-actions");

        auto* edit_anchor = actions->addNew<Wt::WAnchor>(
          Wt::WLink{Wt::LinkType::InternalPath, "/admin/accounts/edit/" + u.username}, "Edit");
        edit_anchor->setStyleClass("link-action-link");

        if(is_me)
        {
            auto* note =
              actions->addNew<Wt::WText>("cannot delete own account", Wt::TextFormat::Plain);
            note->setStyleClass("account-self-note");
        }
        else
        {
            const std::string del_username = u.username;
            auto*             del_btn      = actions->addNew<Wt::WPushButton>("Delete");
            del_btn->setStyleClass("link-action-btn link-delete-btn");
            del_btn->clicked().connect([this, del_username] {
                auto* d = new Wt::WDialog("Confirm Delete");
                d->contents()->addNew<Wt::WText>(
                  "Delete user \"" + del_username + "\"? This cannot be undone.",
                  Wt::TextFormat::Plain);
                auto* yes = d->footer()->addNew<Wt::WPushButton>("Delete");
                yes->setStyleClass("editor-btn");
                auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
                no->setStyleClass("editor-btn editor-btn-cancel");
                yes->clicked().connect([this, d, del_username] {
                    d->accept();
                    m_on_delete(del_username);
                });
                no->clicked().connect([d] { d->reject(); });
                d->finished().connect([d](Wt::DialogCode) { delete d; });
                d->show();
            });
        }
    }
}

void account_manager_page::refresh()
{
    clear();
    render();
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add src/pages/account_manager_page.hpp src/pages/account_manager_page.cpp
git commit -m "feat: live refresh for account manager page"
```

---

## Task 11: Broadcasts in `notifications_page` (accept / decline invite)

**Files:**
- Modify: `src/pages/notifications_page.cpp`

When a user accepts or declines an org invite, the org manage page on another session should update.

- [ ] **Step 1: Add `#include "widgets/live_hub.hpp"` to `notifications_page.cpp`**

After the existing includes:
```cpp
#include "widgets/live_hub.hpp"
```

- [ ] **Step 2: Add broadcasts in the accept handler**

The accept lambda currently reads:
```cpp
m_db.accept_invite(org_id, m_session.username);
m_db.mark_read(nid);
if(m_on_read) { m_on_read(); }
refresh();
```
After `accept_invite`, add:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(org_id));
live_hub::instance().broadcast("user:" + m_session.username);
```

Full updated accept handler:
```cpp
accept->clicked().connect(
  [this, nid = n.id, org_id] {
      m_db.accept_invite(org_id, m_session.username);
      m_db.mark_read(nid);
      live_hub::instance().broadcast("org:" + std::to_string(org_id));
      live_hub::instance().broadcast("user:" + m_session.username);
      if(m_on_read)
      {
          m_on_read();
      }
      refresh();
  });
```

- [ ] **Step 3: Add broadcast in the decline handler**

The decline lambda currently reads:
```cpp
m_db.decline_invite(org_id, m_session.username);
m_db.mark_read(nid);
if(m_on_read) { m_on_read(); }
refresh();
```
After `decline_invite`, add:
```cpp
live_hub::instance().broadcast("org:" + std::to_string(org_id));
```

Full updated decline handler:
```cpp
decline->clicked().connect(
  [this, nid = n.id, org_id] {
      m_db.decline_invite(org_id, m_session.username);
      m_db.mark_read(nid);
      live_hub::instance().broadcast("org:" + std::to_string(org_id));
      if(m_on_read)
      {
          m_on_read();
      }
      refresh();
  });
```

- [ ] **Step 4: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 5: Commit**

```bash
git add src/pages/notifications_page.cpp
git commit -m "feat: broadcast org channel on invite accept/decline"
```

---

## Task 12: Broadcasts in `account_editor_page` and `altinf_app` user-delete

**Files:**
- Modify: `src/pages/account_editor_page.cpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Add broadcast to `account_editor_page::save()`**

Add `#include "widgets/live_hub.hpp"` after the existing includes in `account_editor_page.cpp`.

In `save()`, just before `m_on_save()`:
```cpp
live_hub::instance().broadcast("accounts");
m_on_save();
```

Full updated tail of `save()`:
```cpp
    if(m_existing)
    {
        m_db->update_user(username, display_name, perms);
        if(!password.empty())
        {
            m_db->set_password(username, password);
        }
    }
    else
    {
        if(m_db->username_exists(username))
        {
            m_status->setText("Username already exists.");
            return;
        }
        m_db->create_user(username, password, perms, display_name);
    }

    live_hub::instance().broadcast("accounts");
    m_on_save();
}
```

- [ ] **Step 2: Update the user-delete callback in `altinf_app.cpp`**

The delete callback is the lambda at line ~470. Replace:
```cpp
m_content->addNew<account_manager_page>(
  *m_user_db, m_session, [this](const std::string& username) {
      if(username == m_session.username)
      {
          return;
      }
      m_user_db->delete_user(username);
      m_org_db->remove_user_from_all_orgs(username);
      m_kanban_db->remove_member_from_all_teams(username);
      handle_path("/admin/accounts");
  });
```
With:
```cpp
m_content->addNew<account_manager_page>(
  *m_user_db, m_session, [this](const std::string& username) {
      if(username == m_session.username)
      {
          return;
      }
      // Collect memberships before deletion so we can broadcast them.
      const auto del_orgs     = m_org_db->orgs_for_user(username);
      const auto del_team_ids = m_kanban_db->team_ids_for_user(username);

      m_user_db->delete_user(username);
      m_org_db->remove_user_from_all_orgs(username);
      m_kanban_db->remove_member_from_all_teams(username);

      live_hub::instance().broadcast("accounts");
      for(const auto& org: del_orgs)
      {
          live_hub::instance().broadcast("org:" + std::to_string(org.id));
      }
      for(const auto tid: del_team_ids)
      {
          live_hub::instance().broadcast("team:" + std::to_string(tid));
      }

      handle_path("/admin/accounts");
  });
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Run all C++ tests**

```bash
cd build && ctest --output-on-failure
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src/pages/account_editor_page.cpp src/altinf_app.cpp
git commit -m "feat: broadcast accounts/org/team channels on user create/edit/delete"
```

---

## Task 13: Task editor — conflict detection

**Files:**
- Modify: `src/pages/kanban_task_editor_page.hpp`
- Modify: `src/pages/kanban_task_editor_page.cpp`

When another session modifies or deletes a task that is currently open in edit mode, `mark_stale()` fires and disables Save/Delete. The same `mark_stale()` runs if a `Wt::Dbo::StaleObjectException` is caught on save.

- [ ] **Step 1: Update `src/pages/kanban_task_editor_page.hpp`**

Add members and declarations:
```cpp
#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WColorPicker.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WDateEdit.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <functional>
#include <string>
#include <vector>

class kanban_task_editor_page: public Wt::WContainerWidget
{
public:
    kanban_task_editor_page(kanban_db&                      db,
                            org_db&                         odb,
                            long long                       team_id,
                            const session_data&             session,
                            bool                            is_lead,
                            const kanban_task_entry*        existing,
                            const std::vector<std::string>& members,
                            std::function<void()>           on_save);

    ~kanban_task_editor_page() override;

private:
    kanban_db&               m_db;
    org_db&                  m_odb;
    long long                m_team_id;
    std::string              m_username;
    bool                     m_is_lead;
    const kanban_task_entry* m_existing;
    std::function<void()>    m_on_save;

    long long          m_task_id{0};   // 0 = new task or after unsubscribe
    std::string        m_session_id;

    Wt::WLineEdit*    m_title{nullptr};
    Wt::WTextArea*    m_description{nullptr};
    Wt::WComboBox*    m_status{nullptr};
    Wt::WComboBox*    m_assigned_to{nullptr};
    Wt::WDateEdit*    m_start_date{nullptr};
    Wt::WDateEdit*    m_end_date{nullptr};
    Wt::WColorPicker* m_color{nullptr};
    Wt::WText*        m_status_msg{nullptr};
    Wt::WPushButton*  m_save_btn{nullptr};
    Wt::WPushButton*  m_del_btn{nullptr};

    std::vector<std::string> m_assignee_values;

    static const std::vector<std::string> k_status_vals;
    static const std::vector<std::string> k_status_labels;

    void save();
    void mark_stale();
};
```

- [ ] **Step 2: Add the destructor and `mark_stale()` to `kanban_task_editor_page.cpp`**

Add destructor after the constructor definition:
```cpp
kanban_task_editor_page::~kanban_task_editor_page()
{
    if(m_task_id != 0)
    {
        live_hub::instance().unsubscribe(
          "task:" + std::to_string(m_task_id), m_session_id);
    }
}
```

Add `mark_stale()`:
```cpp
void kanban_task_editor_page::mark_stale()
{
    m_status_msg->setText(
      "This task was modified by another user \xe2\x80\x94 saving is disabled.");
    if(m_save_btn) m_save_btn->setDisabled(true);
    if(m_del_btn)  m_del_btn->setDisabled(true);
}
```

- [ ] **Step 3: Store `m_save_btn` and `m_del_btn` pointers**

In the constructor, the save button is currently a local `auto* save_btn`. Change it to store the pointer:

Old:
```cpp
auto* save_btn =
  btn_row->addNew<Wt::WPushButton>(is_new ? "Create Task" : "Save Changes");
save_btn->setStyleClass("editor-btn");
save_btn->clicked().connect([this] { save(); });
```
New:
```cpp
m_save_btn =
  btn_row->addNew<Wt::WPushButton>(is_new ? "Create Task" : "Save Changes");
m_save_btn->setStyleClass("editor-btn");
m_save_btn->clicked().connect([this] { save(); });
```

The delete button is in a conditional block. Change:
```cpp
auto* del_btn = btn_row->addNew<Wt::WPushButton>("Delete");
del_btn->setStyleClass("editor-btn editor-btn-danger");
del_btn->clicked().connect(
  [this] {
      m_db.delete_task(m_existing->id);
      m_on_save();
  });
```
To (just store the pointer, actual broadcast is in Task 14):
```cpp
m_del_btn = btn_row->addNew<Wt::WPushButton>("Delete");
m_del_btn->setStyleClass("editor-btn editor-btn-danger");
m_del_btn->clicked().connect(
  [this] {
      m_db.delete_task(m_existing->id);
      m_on_save();
  });
```

- [ ] **Step 4: Subscribe in edit mode**

After the buttons are wired, and only when `existing != nullptr`, add:
```cpp
    // Subscribe to conflict notifications in edit mode.
    if(existing)
    {
        m_task_id    = existing->id;
        m_session_id = Wt::WApplication::instance()->sessionId();
        live_hub::instance().subscribe(
          "task:" + std::to_string(m_task_id),
          m_session_id,
          [this] { mark_stale(); triggerUpdate(); });
    }
```

- [ ] **Step 5: Add `#include <Wt/Dbo/Exception.h>` and wrap `update_task()` in `save()`**

Add the include at the top of `kanban_task_editor_page.cpp`:
```cpp
#include <Wt/Dbo/Exception.h>
```

In `save()`, replace:
```cpp
if(m_existing)
{
    t.id         = m_existing->id;
    t.sort_order = m_existing->sort_order;
    m_db.update_task(t);
}
```
With:
```cpp
if(m_existing)
{
    t.id         = m_existing->id;
    t.sort_order = m_existing->sort_order;
    try
    {
        m_db.update_task(t);
    }
    catch(const Wt::Dbo::StaleObjectException&)
    {
        mark_stale();
        return;
    }
}
```

- [ ] **Step 6: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 7: Commit**

```bash
git add src/pages/kanban_task_editor_page.hpp src/pages/kanban_task_editor_page.cpp
git commit -m "feat: conflict detection in task editor (mark_stale, hub subscription)"
```

---

## Task 14: Task editor and board broadcast after save / delete

**Files:**
- Modify: `src/pages/kanban_task_editor_page.cpp`

After a successful save or delete, the editor self-unsubscribes from the task channel (to avoid receiving its own broadcast) then broadcasts to both `"task:{id}"` and `"team:{team_id}"`.

- [ ] **Step 1: Update the delete-button handler to broadcast**

Replace the delete handler wired in Task 13 Step 3:
```cpp
m_del_btn->clicked().connect(
  [this] {
      m_db.delete_task(m_existing->id);
      m_on_save();
  });
```
With:
```cpp
m_del_btn->clicked().connect(
  [this] {
      const long long tid = m_existing->id;
      // Unsubscribe before broadcasting so this session doesn't receive the
      // mark_stale callback for its own deletion.
      if(m_task_id != 0)
      {
          live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
          m_task_id = 0;
      }
      m_db.delete_task(tid);
      live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
      live_hub::instance().broadcast("task:" + std::to_string(tid));
      m_on_save();
  });
```

- [ ] **Step 2: Update `save()` to broadcast after successful save**

After the `if(m_existing) { ... } else { t.id = m_db.add_task(t); }` block and the assignee-notification block, and before `m_on_save()`:

Replace:
```cpp
    // Fire a task_assigned notification if the assignee changed to a new person.
    if(!new_assignee.empty() && new_assignee != old_assignee &&
       new_assignee != m_username)
    {
        const auto team = m_db.find_team(m_team_id);
        m_odb.push_notification(
          new_assignee,
          "task_assigned",
          make_task_assigned_payload(t.id, title, m_team_id, team ? team->name : ""));
        live_hub::instance().broadcast("user:" + new_assignee);
    }

    m_on_save();
}
```
With:
```cpp
    // Fire a task_assigned notification if the assignee changed to a new person.
    if(!new_assignee.empty() && new_assignee != old_assignee &&
       new_assignee != m_username)
    {
        const auto team = m_db.find_team(m_team_id);
        m_odb.push_notification(
          new_assignee,
          "task_assigned",
          make_task_assigned_payload(t.id, title, m_team_id, team ? team->name : ""));
        live_hub::instance().broadcast("user:" + new_assignee);
    }

    // Unsubscribe before broadcasting to avoid self-triggering mark_stale.
    if(m_task_id != 0)
    {
        live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
        m_task_id = 0;
    }

    live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
    if(m_existing)
    {
        live_hub::instance().broadcast("task:" + std::to_string(m_existing->id));
    }

    m_on_save();
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

Expected: no errors.

- [ ] **Step 4: Run all C++ tests**

```bash
cd build && ctest --output-on-failure
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src/pages/kanban_task_editor_page.cpp
git commit -m "feat: broadcast team/task channels after task save or delete"
```

---

## Task 15: Playwright — `live-board.spec.ts`

**Files:**
- Create: `e2e/specs/live-board.spec.ts`

Two admin sessions open the same board (or gantt). One makes a change; the other's page updates without a reload.

- [ ] **Step 1: Create `e2e/specs/live-board.spec.ts`**

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

async function navigateToBoard(page: Page, orgName: string) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${orgName}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

async function navigateToGantt(page: Page, orgName: string) {
  await navigateToBoard(page, orgName);
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
}

async function openTaskEditor(page: Page, title: string) {
  // Click the task card's edit area to navigate to the editor.
  await page.locator('.kb-card', { hasText: title }).click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

async function saveTaskEditor(page: Page) {
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create org and team for board live tests.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill('LiveBoardOrg');
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: /^LiveBoardOrg$/ })).toBeVisible();
  await page.locator('.org-list-link', { hasText: /^LiveBoardOrg$/ }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('input[placeholder="Team name"]').fill('LiveBoardTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  await page.close();
});

test('board: task created by A appears on B without reload', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToBoard(pageB, 'LiveBoardOrg');

  // A creates a new task.
  await pageA.locator('.kb-new-btn').click();
  await expect(pageA.locator('.kb-editor-page')).toBeVisible();
  await pageA.locator('input[placeholder="Task title"]').fill('LiveNewTask');
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  // B should see the new card without reloading.
  await expect(pageB.locator('.kb-card', { hasText: 'LiveNewTask' })).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('board: task title edited by A updates on B', async ({ browser }) => {
  // State: LiveNewTask card exists from previous test.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToBoard(pageB, 'LiveBoardOrg');

  // A opens the task editor and changes the title.
  await openTaskEditor(pageA, 'LiveNewTask');
  await pageA.locator('input[placeholder="Task title"]').fill('LiveRenamedTask');
  await saveTaskEditor(pageA);

  // B sees the renamed card.
  await expect(pageB.locator('.kb-card', { hasText: 'LiveRenamedTask' })).toBeVisible();
  await expect(pageB.locator('.kb-card', { hasText: 'LiveNewTask' })).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('board: task deleted by A disappears from B', async ({ browser }) => {
  // State: LiveRenamedTask exists from previous test.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToBoard(pageB, 'LiveBoardOrg');

  // A deletes the task.
  await openTaskEditor(pageA, 'LiveRenamedTask');
  await pageA.locator('.editor-btn-danger').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  // B no longer sees the card.
  await expect(pageB.locator('.kb-card', { hasText: 'LiveRenamedTask' })).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('board: drag-drop by A moves card on B', async ({ browser }) => {
  // Create a task first.
  const setup = await browser.newContext();
  const setupPage = await setup.newPage();
  await loginAs(setupPage, 'admin', 'testpass');
  await navigateToBoard(setupPage, 'LiveBoardOrg');
  await setupPage.locator('.kb-new-btn').click();
  await expect(setupPage.locator('.kb-editor-page')).toBeVisible();
  await setupPage.locator('input[placeholder="Task title"]').fill('DragDropTask');
  await setupPage.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(setupPage.locator('.kb-board')).toBeVisible();
  await setup.close();

  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToBoard(pageB, 'LiveBoardOrg');

  // Locate the card in the "To Do" column and drag it to "In Progress".
  const card  = pageA.locator('.kb-col[data-status="todo"] .kb-card', { hasText: 'DragDropTask' });
  const inProg = pageA.locator('.kb-col[data-status="in_progress"]');
  await card.dragTo(inProg);

  // B should see the card in the "In Progress" column.
  await expect(
    pageB.locator('.kb-col[data-status="in_progress"] .kb-card', { hasText: 'DragDropTask' })
  ).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('gantt: task created with dates by A appears on B gantt view', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToGantt(pageB, 'LiveBoardOrg');

  // A creates a task with dates from board view.
  await pageA.locator('.kb-new-btn').click();
  await expect(pageA.locator('.kb-editor-page')).toBeVisible();
  await pageA.locator('input[placeholder="Task title"]').fill('GanttTask');
  const startInput = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
  const endInput   = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
  await startInput.fill('2026-06-01');
  await startInput.press('Tab');
  await endInput.fill('2026-06-15');
  await endInput.press('Tab');
  await pageA.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  // B's gantt view shows a bar for the new task.
  await expect(pageB.locator('.gv-bar[data-title="GanttTask"]')).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('gantt: task deleted by A disappears from B gantt view', async ({ browser }) => {
  // State: GanttTask exists from previous test.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToGantt(pageB, 'LiveBoardOrg');

  await openTaskEditor(pageA, 'GanttTask');
  await pageA.locator('.editor-btn-danger').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  await expect(pageB.locator('.gv-bar[data-title="GanttTask"]')).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});
```

- [ ] **Step 2: Build the app**

```bash
cmake --build build --parallel $(nproc) 2>&1 | grep "error:"
```

- [ ] **Step 3: Run just these Playwright tests**

```bash
cd e2e && npx playwright test live-board.spec.ts
```

Expected: all PASS.

- [ ] **Step 4: Commit**

```bash
git add e2e/specs/live-board.spec.ts
git commit -m "test(e2e): live board and gantt refresh tests"
```

---

## Task 16: Playwright — `live-task-editor.spec.ts`

**Files:**
- Create: `e2e/specs/live-task-editor.spec.ts`

Tests the conflict detection paths: B saves first → A's editor is blocked; B deletes the task → A's editor is blocked.

- [ ] **Step 1: Create `e2e/specs/live-task-editor.spec.ts`**

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

async function navigateToBoard(page: Page, orgName: string) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${orgName}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

async function openTaskEditorViaPath(page: Page, orgName: string, title: string) {
  await navigateToBoard(page, orgName);
  await page.locator('.kb-card', { hasText: title }).click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create org, team, and a task for conflict tests.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill('LiveEditorOrg');
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: /^LiveEditorOrg$/ })).toBeVisible();
  await page.locator('.org-list-link', { hasText: /^LiveEditorOrg$/ }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('input[placeholder="Team name"]').fill('EditorTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  // Create the conflict task.
  await page.locator('.org-list-link', { hasText: /^LiveEditorOrg$/ }).click().catch(() => {});
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('.org-list-link', { hasText: /^LiveEditorOrg$/ }).click();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('ConflictTask');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.close();
});

test('task editor: B saves a change → A sees conflict message and Save is disabled', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  // Both open the same task editor.
  await openTaskEditorViaPath(pageA, 'LiveEditorOrg', 'ConflictTask');
  await openTaskEditorViaPath(pageB, 'LiveEditorOrg', 'ConflictTask');

  // B saves a change.
  await pageB.locator('input[placeholder="Task title"]').fill('ConflictTaskEdited');
  await pageB.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(pageB.locator('.kb-board')).toBeVisible();

  // A should see the conflict message and Save should be disabled — without reloading.
  await expect(pageA.locator('.editor-status')).toContainText('modified by another user');
  await expect(
    pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')
  ).toBeDisabled();

  await ctxA.close();
  await ctxB.close();
});

test('task editor: B deletes the task → A sees conflict message and Save is disabled', async ({ browser }) => {
  // State: task is now titled 'ConflictTaskEdited' from previous test.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await openTaskEditorViaPath(pageA, 'LiveEditorOrg', 'ConflictTaskEdited');
  await openTaskEditorViaPath(pageB, 'LiveEditorOrg', 'ConflictTaskEdited');

  // B deletes the task.
  await pageB.locator('.editor-btn-danger').click();
  await expect(pageB.locator('.kb-board')).toBeVisible();

  // A's editor shows the conflict message and Save is disabled.
  await expect(pageA.locator('.editor-status')).toContainText('modified by another user');
  await expect(
    pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')
  ).toBeDisabled();

  await ctxA.close();
  await ctxB.close();
});
```

- [ ] **Step 2: Run these tests**

```bash
cd e2e && npx playwright test live-task-editor.spec.ts
```

Expected: all PASS.

- [ ] **Step 3: Commit**

```bash
git add e2e/specs/live-task-editor.spec.ts
git commit -m "test(e2e): task editor conflict detection tests"
```

---

## Task 17: Playwright — `live-org-manage.spec.ts`

**Files:**
- Create: `e2e/specs/live-org-manage.spec.ts`

Two leads are on the org manage page. One makes a change; the other's page updates live.

- [ ] **Step 1: Create `e2e/specs/live-org-manage.spec.ts`**

```typescript
import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG = 'LiveManageOrg';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function goToManage(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create a second user 'bob' for the org tests.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
  await page.locator('.account-new-btn').click();
  await expect(page.locator('.account-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Username (required)"]').fill('bob');
  await page.locator('input[placeholder="Password (required)"]').fill('bobpass');
  await page.locator('input[placeholder="Confirm password"]').fill('bobpass');
  // Give bob org_create so he can be an org lead.
  await page.locator('label', { hasText: 'Create Orgs' }).click();
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  // Create the org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();

  // Invite bob as a lead so he can also navigate to the manage page.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('.kb-member-input').fill('bob');
  await page.locator('.kb-lead-check').click();
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to bob');

  // Accept the invite as bob.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await bobPage.locator('.nav-bell-link').click();
  await expect(bobPage.locator('.notifications-page')).toBeVisible();
  await bobPage.getByRole('button', { name: 'Accept' }).click();
  await expect(bobPage.locator('.nav-bell-badge')).not.toBeVisible();
  await bobCtx.close();

  // Create a team for the org.
  await goToManage(page);
  await page.locator('input[placeholder="Team name"]').fill('ManageTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  await page.close();
});

test('org manage: accept invite adds member on second lead\'s page', async ({ browser }) => {
  // Create a third user 'carol' and invite her.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');
  await adminPage.locator('.nav-link', { hasText: 'Accounts' }).click();
  await adminPage.locator('.account-new-btn').click();
  await adminPage.locator('input[placeholder="Username (required)"]').fill('carol');
  await adminPage.locator('input[placeholder="Password (required)"]').fill('carolpass');
  await adminPage.locator('input[placeholder="Confirm password"]').fill('carolpass');
  await adminPage.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(adminPage.locator('.account-manager-page')).toBeVisible();
  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('carol');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to carol');

  // Bob is also watching the manage page.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await goToManage(bobPage);
  const membersBefore = await bobPage.locator('.kb-members-container .kb-member-row').count();

  // Carol accepts the invite.
  const carolCtx = await browser.newContext();
  const carolPage = await carolCtx.newPage();
  await loginAs(carolPage, 'carol', 'carolpass');
  await carolPage.locator('.nav-bell-link').click();
  await carolPage.getByRole('button', { name: 'Accept' }).click();

  // Bob's manage page shows carol in Members without reload.
  await expect(bobPage.locator('.kb-members-container .kb-member-row')).toHaveCount(membersBefore + 1);
  await expect(bobPage.locator('.kb-members-container')).toContainText('carol');

  await adminCtx.close();
  await bobCtx.close();
  await carolCtx.close();
});

test('org manage: decline invite removes pending row on second lead\'s page', async ({ browser }) => {
  // Invite a new user 'dave' and have bob watch the pending section.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');

  // Create dave.
  await adminPage.locator('.nav-link', { hasText: 'Accounts' }).click();
  await adminPage.locator('.account-new-btn').click();
  await adminPage.locator('input[placeholder="Username (required)"]').fill('dave');
  await adminPage.locator('input[placeholder="Password (required)"]').fill('davepass');
  await adminPage.locator('input[placeholder="Confirm password"]').fill('davepass');
  await adminPage.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(adminPage.locator('.account-manager-page')).toBeVisible();

  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('dave');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to dave');

  // Bob watches the manage page.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await goToManage(bobPage);
  await expect(bobPage.locator('.kb-pending-section .kb-member-row', { hasText: 'dave' })).toBeVisible();

  // Dave declines.
  const daveCtx = await browser.newContext();
  const davePage = await daveCtx.newPage();
  await loginAs(davePage, 'dave', 'davepass');
  await davePage.locator('.nav-bell-link').click();
  await davePage.getByRole('button', { name: 'Decline' }).click();

  // Bob's pending section no longer shows dave.
  await expect(bobPage.locator('.kb-pending-section .kb-member-row', { hasText: 'dave' })).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
  await daveCtx.close();
});

test('org manage: withdraw invite updates second lead\'s pending section', async ({ browser }) => {
  // Invite carol again (she's a member now; invite a new user 'eve').
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');

  await adminPage.locator('.nav-link', { hasText: 'Accounts' }).click();
  await adminPage.locator('.account-new-btn').click();
  await adminPage.locator('input[placeholder="Username (required)"]').fill('eve');
  await adminPage.locator('input[placeholder="Password (required)"]').fill('evepass');
  await adminPage.locator('input[placeholder="Confirm password"]').fill('evepass');
  await adminPage.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(adminPage.locator('.account-manager-page')).toBeVisible();

  // Bob watches the manage page.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await goToManage(bobPage);

  // Admin invites eve from the manage page.
  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('eve');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to eve');
  // Bob sees the pending invite.
  await expect(bobPage.locator('.kb-pending-section .kb-member-row', { hasText: 'eve' })).toBeVisible();

  // Admin withdraws the invite.
  await adminPage.locator('.kb-pending-section .kb-member-row', { hasText: 'eve' })
    .getByRole('button', { name: 'Withdraw' }).click();

  // Bob's pending section no longer shows eve.
  await expect(bobPage.locator('.kb-pending-section .kb-member-row', { hasText: 'eve' })).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: promote to org lead updates second lead\'s members section', async ({ browser }) => {
  // carol is a member. Admin promotes her; bob should see "(lead)" appear.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
    .getByRole('button', { name: 'Make lead' }).click();

  await expect(
    bobPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
  ).toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: demote from org lead updates second lead\'s members section', async ({ browser }) => {
  // carol is now a lead. Admin demotes; bob should see "(lead)" disappear.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
    .getByRole('button', { name: 'Demote' }).click();

  await expect(
    bobPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
  ).not.toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: remove member updates second lead\'s members section', async ({ browser }) => {
  // carol is a plain member. Admin removes her; bob should not see carol anymore.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
    .getByRole('button', { name: 'Remove' }).click();

  await expect(
    bobPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
  ).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: create team appears on second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('input[placeholder="Team name"]').fill('NewLiveTeam');
  await adminPage.getByRole('button', { name: 'Create' }).click();
  await expect(adminPage.locator('.kb-team-block', { hasText: 'NewLiveTeam' })).toBeVisible();

  await expect(bobPage.locator('.kb-team-block', { hasText: 'NewLiveTeam' })).toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: rename team updates second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  const teamBlock = adminPage.locator('.kb-team-block', { hasText: 'NewLiveTeam' });
  await teamBlock.locator('input.editor-field').fill('RenamedLiveTeam');
  await teamBlock.getByRole('button', { name: 'Rename' }).click();

  await expect(bobPage.locator('.kb-team-block', { hasText: 'RenamedLiveTeam' })).toBeVisible();
  await expect(bobPage.locator('.kb-team-block', { hasText: 'NewLiveTeam' })).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: delete team disappears from second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-team-block', { hasText: 'RenamedLiveTeam' })
    .getByRole('button', { name: 'Delete team' }).click();

  await expect(bobPage.locator('.kb-team-block', { hasText: 'RenamedLiveTeam' })).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: add member to team updates second lead\'s page', async ({ browser }) => {
  // bob is already a member of the org. Add bob to ManageTeam.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  const teamBlock = adminPage.locator('.kb-team-block', { hasText: 'ManageTeam' });
  await teamBlock.locator('.gv-range-select').selectOption('bob');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();

  await expect(
    bobPage.locator('.kb-team-block', { hasText: 'ManageTeam' }).locator('.kb-member-row', { hasText: 'bob' })
  ).toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: remove member from team updates second lead\'s page', async ({ browser }) => {
  // bob is on ManageTeam from previous test. Admin removes him.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-team-block', { hasText: 'ManageTeam' })
    .locator('.kb-member-row', { hasText: 'bob' })
    .getByRole('button', { name: 'Remove' }).click();

  await expect(
    bobPage.locator('.kb-team-block', { hasText: 'ManageTeam' }).locator('.kb-member-row', { hasText: 'bob' })
  ).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: promote team lead updates second lead\'s page', async ({ browser }) => {
  // Add bob back to ManageTeam first, then promote.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  const teamBlock = adminPage.locator('.kb-team-block', { hasText: 'ManageTeam' });
  await teamBlock.locator('.gv-range-select').selectOption('bob');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();
  await expect(adminPage.locator('.kb-team-block', { hasText: 'ManageTeam' }).locator('.kb-member-row', { hasText: 'bob' })).toBeVisible();

  await teamBlock.locator('.kb-member-row', { hasText: 'bob' })
    .getByRole('button', { name: 'Make lead' }).click();

  await expect(
    bobPage.locator('.kb-team-block', { hasText: 'ManageTeam' }).locator('.kb-member-row', { hasText: 'bob' })
  ).toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: demote team lead updates second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-team-block', { hasText: 'ManageTeam' })
    .locator('.kb-member-row', { hasText: 'bob' })
    .getByRole('button', { name: 'Remove lead' }).click();

  await expect(
    bobPage.locator('.kb-team-block', { hasText: 'ManageTeam' }).locator('.kb-member-row', { hasText: 'bob' })
  ).not.toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});
```

- [ ] **Step 2: Run these tests**

```bash
cd e2e && npx playwright test live-org-manage.spec.ts
```

Expected: all PASS.

- [ ] **Step 3: Commit**

```bash
git add e2e/specs/live-org-manage.spec.ts
git commit -m "test(e2e): live org manage page refresh tests"
```

---

## Task 18: Playwright — `live-org-landing.spec.ts`

**Files:**
- Create: `e2e/specs/live-org-landing.spec.ts`

- [ ] **Step 1: Create `e2e/specs/live-org-landing.spec.ts`**

```typescript
import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG = 'LiveLandingOrg';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function goToLanding(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
}

async function goToManage(page: Page) {
  await goToLanding(page);
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create a fresh user 'frank' for landing page tests.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await page.locator('.account-new-btn').click();
  await page.locator('input[placeholder="Username (required)"]').fill('frank');
  await page.locator('input[placeholder="Password (required)"]').fill('frankpass');
  await page.locator('input[placeholder="Confirm password"]').fill('frankpass');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  // Create the org and invite frank.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('.kb-member-input').fill('frank');
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to frank');

  // Create a team.
  await page.locator('input[placeholder="Team name"]').fill('LandingTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block', { hasText: 'LandingTeam' })).toBeVisible();

  // Frank accepts the invite.
  const frankCtx = await browser.newContext();
  const frankPage = await frankCtx.newPage();
  await loginAs(frankPage, 'frank', 'frankpass');
  await frankPage.locator('.nav-bell-link').click();
  await frankPage.getByRole('button', { name: 'Accept' }).click();
  await expect(frankPage.locator('.nav-bell-badge')).not.toBeVisible();
  await frankCtx.close();

  await page.close();
});

test('org landing: team created by lead appears on member\'s landing page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  await adminPage.locator('input[placeholder="Team name"]').fill('NewLandingTeam');
  await adminPage.getByRole('button', { name: 'Create' }).click();
  await expect(adminPage.locator('.kb-team-block', { hasText: 'NewLandingTeam' })).toBeVisible();

  // Frank's landing page gains the team in "Other teams" (he's not a member yet).
  await expect(frankPage.locator('.org-team-row--other', { hasText: 'NewLandingTeam' })).toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: team renamed by lead updates member\'s landing page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  const teamBlock = adminPage.locator('.kb-team-block', { hasText: 'LandingTeam' });
  await teamBlock.locator('input.editor-field').fill('RenamedLandingTeam');
  await teamBlock.getByRole('button', { name: 'Rename' }).click();

  await expect(frankPage.locator('.org-team-row', { hasText: 'RenamedLandingTeam' })).toBeVisible();
  await expect(frankPage.locator('.org-team-row', { hasText: 'LandingTeam' }).filter({ hasText: /^LandingTeam$/ })).not.toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: team deleted by lead disappears from member\'s landing page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  await adminPage.locator('.kb-team-block', { hasText: 'NewLandingTeam' })
    .getByRole('button', { name: 'Delete team' }).click();

  await expect(frankPage.locator('.org-team-row', { hasText: 'NewLandingTeam' })).not.toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: added to team moves row from Other to Your teams on member\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  // Frank is not in RenamedLandingTeam — it should be in "Other teams".
  await expect(frankPage.locator('.org-team-row--other', { hasText: 'RenamedLandingTeam' })).toBeVisible();

  const teamBlock = adminPage.locator('.kb-team-block', { hasText: 'RenamedLandingTeam' });
  await teamBlock.locator('.gv-range-select').selectOption('frank');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();

  // Frank's row moves to "Your teams" (no --other class).
  await expect(frankPage.locator('.org-team-row:not(.org-team-row--other)', { hasText: 'RenamedLandingTeam' })).toBeVisible();
  await expect(frankPage.locator('.org-team-row--other', { hasText: 'RenamedLandingTeam' })).not.toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: removed from team moves row back to Other teams', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  await adminPage.locator('.kb-team-block', { hasText: 'RenamedLandingTeam' })
    .locator('.kb-member-row', { hasText: 'frank' })
    .getByRole('button', { name: 'Remove' }).click();

  await expect(frankPage.locator('.org-team-row--other', { hasText: 'RenamedLandingTeam' })).toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});
```

- [ ] **Step 2: Run these tests**

```bash
cd e2e && npx playwright test live-org-landing.spec.ts
```

Expected: all PASS.

- [ ] **Step 3: Commit**

```bash
git add e2e/specs/live-org-landing.spec.ts
git commit -m "test(e2e): live org landing page refresh tests"
```

---

## Task 19: Playwright — `live-nav-accounts.spec.ts`

**Files:**
- Create: `e2e/specs/live-nav-accounts.spec.ts`

Tests nav bar org selector live updates, account manager live updates, and regression coverage for the notification bell.

- [ ] **Step 1: Create `e2e/specs/live-nav-accounts.spec.ts`**

```typescript
import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG = 'LiveNavOrg';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function goToManage(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create user 'grace'.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await page.locator('.account-new-btn').click();
  await page.locator('input[placeholder="Username (required)"]').fill('grace');
  await page.locator('input[placeholder="Password (required)"]').fill('gracepass');
  await page.locator('input[placeholder="Confirm password"]').fill('gracepass');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  // Create org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();

  await page.close();
});

test('nav bar: accepting invite adds org to selector without navigating', async ({ browser }) => {
  // Admin invites grace.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');
  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('grace');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to grace');
  await adminCtx.close();

  // Grace is on the home page. Her org selector should not yet include LiveNavOrg.
  const graceCtx = await browser.newContext();
  const gracePage = await graceCtx.newPage();
  await loginAs(gracePage, 'grace', 'gracepass');
  await expect(gracePage.locator('.nav-org-selector')).not.toContainText(ORG);

  // Grace accepts the invite from the notifications page.
  await gracePage.locator('.nav-bell-link').click();
  await gracePage.getByRole('button', { name: 'Accept' }).click();

  // The org selector should now include LiveNavOrg (server push via user channel).
  await expect(gracePage.locator('.nav-org-selector')).toContainText(ORG);

  await graceCtx.close();
});

test('nav bar: removed from org removes org from selector without navigating', async ({ browser }) => {
  // Grace is a member of LiveNavOrg. Admin removes her.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');

  // Grace is on the home page with LiveNavOrg visible in her selector.
  const graceCtx = await browser.newContext();
  const gracePage = await graceCtx.newPage();
  await loginAs(gracePage, 'grace', 'gracepass');
  await expect(gracePage.locator('.nav-org-selector')).toContainText(ORG);

  await goToManage(adminPage);
  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'grace' })
    .getByRole('button', { name: 'Remove' }).click();

  // Grace's nav bar no longer shows LiveNavOrg.
  await expect(gracePage.locator('.nav-org-selector')).not.toContainText(ORG);

  await adminCtx.close();
  await graceCtx.close();
});

test('account manager: new user created by A appears on B\'s accounts page', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  // Both navigate to accounts.
  await pageA.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();
  await pageB.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageB.locator('.account-manager-page')).toBeVisible();

  // A creates a new user.
  await pageA.locator('.account-new-btn').click();
  await expect(pageA.locator('.account-editor-page')).toBeVisible();
  await pageA.locator('input[placeholder="Username (required)"]').fill('newliveuser');
  await pageA.locator('input[placeholder="Password (required)"]').fill('pass');
  await pageA.locator('input[placeholder="Confirm password"]').fill('pass');
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();

  // B's accounts page shows the new user without reload.
  await expect(pageB.locator('.account-table td', { hasText: 'newliveuser' })).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('account manager: user deleted by A disappears from B\'s accounts page', async ({ browser }) => {
  // State: 'newliveuser' exists from previous test.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await pageA.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();
  await pageB.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageB.locator('.account-manager-page')).toBeVisible();

  // A deletes 'newliveuser'.
  const row = pageA.locator('.account-table tr', { hasText: 'newliveuser' });
  await row.getByRole('button', { name: 'Delete' }).click();
  // Confirm the dialog.
  await pageA.getByRole('button', { name: 'Delete' }).last().click();

  // B's accounts page no longer shows 'newliveuser'.
  await expect(pageB.locator('.account-table td', { hasText: 'newliveuser' })).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('account manager: user permissions edited by A update on B\'s accounts page', async ({ browser }) => {
  // State: 'grace' exists. Admin edits her permissions; B should see the new label.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await pageA.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();
  await pageB.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageB.locator('.account-manager-page')).toBeVisible();

  // A navigates to grace's edit page and adds the "Create Orgs" permission.
  const graceRow = pageA.locator('.account-table tr', { hasText: 'grace' });
  await graceRow.locator('.link-action-link', { hasText: 'Edit' }).click();
  await expect(pageA.locator('.account-editor-page')).toBeVisible();
  await pageA.locator('label', { hasText: 'Create Orgs' }).click();
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();

  // B sees the updated permissions label for grace.
  await expect(
    pageB.locator('.account-table tr', { hasText: 'grace' }).locator('td').nth(2)
  ).toContainText('Create Orgs');

  await ctxA.close();
  await ctxB.close();
});

test('bell regression: invite sent to grace increments bell badge on her active session', async ({ browser }) => {
  // Admin creates a fresh org and invites grace. Grace's bell should increment.
  const adminCtx = await browser.newContext();
  const graceCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const gracePage = await graceCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(gracePage, 'grace', 'gracepass');
  await expect(gracePage.locator('.nav-bell-badge')).not.toBeVisible();

  // Admin creates a new org and invites grace.
  await adminPage.locator('.nav-link', { hasText: 'Orgs' }).click();
  await adminPage.locator('input[placeholder="Organization name"]').fill('BellRegressionOrg');
  await adminPage.locator('.org-create-form .editor-btn').click();
  await expect(adminPage.locator('.org-list-link', { hasText: /^BellRegressionOrg$/ })).toBeVisible();
  await adminPage.locator('.org-list-link', { hasText: /^BellRegressionOrg$/ }).click();
  await adminPage.getByRole('link', { name: 'Manage organization' }).click();
  await expect(adminPage.locator('.kb-team-page')).toBeVisible();
  await adminPage.locator('.kb-member-input').fill('grace');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to grace');

  // Grace's bell badge appears.
  await expect(gracePage.locator('.nav-bell-badge')).toBeVisible();

  await adminCtx.close();
  await graceCtx.close();
});
```

- [ ] **Step 2: Run these tests**

```bash
cd e2e && npx playwright test live-nav-accounts.spec.ts
```

Expected: all PASS.

- [ ] **Step 3: Run all three test suites**

```bash
cd build && ctest --output-on-failure
cd ..
cd e2e && npx playwright test
```

Expected: all C++ tests PASS, all Playwright tests PASS.

- [ ] **Step 4: Commit**

```bash
git add e2e/specs/live-nav-accounts.spec.ts
git commit -m "test(e2e): live nav bar, account manager, and bell regression tests"
```

---

## Summary

19 tasks, ~35 commits. Each task builds on the previous. The critical path is:

1. Tasks 1–2: live_hub in place and tested
2. Tasks 3–4: all call sites migrated (app builds cleanly)
3. Tasks 5–14: all subscriptions and broadcasts wired
4. Tasks 15–19: full Playwright coverage

Every test suite must pass at the end of each task before proceeding.
