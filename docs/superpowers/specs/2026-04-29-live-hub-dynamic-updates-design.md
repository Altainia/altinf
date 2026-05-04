# Live Hub & Dynamic Updates — Design Spec

**Date:** 2026-04-29
**Branch:** enhancement/kanban-board

## Overview

Replace the narrow `notification_hub` (which only pushed bell/notification-page refreshes to a
single user) with a generalized `live_hub` keyed on channel strings. Any page that displays data
another user can mutate subscribes to the relevant channel; mutations broadcast to all affected
channels. The result is that every live session sees updates immediately, without a page reload.

Excluded from scope: blog post pages.

---

## Section 1 — `live_hub` (replaces `notification_hub`)

### Files

| Old | New |
|---|---|
| `src/widgets/notification_hub.hpp` | `src/widgets/live_hub.hpp` |
| `src/widgets/notification_hub.cpp` | `src/widgets/live_hub.cpp` |
| `tests/test_notification_hub.cpp` | `tests/test_live_hub.cpp` |

### API

```cpp
class live_hub
{
public:
    using post_fn_t =
      std::function<void(const std::string& session_id, std::function<void()> fn)>;

    static live_hub& instance();

    // Replace dispatch function and clear all subscriptions. For test setup only.
    void reset(post_fn_t post_fn = {});

    // Subscribe a session callback to a channel. Call from within that session's event loop.
    void subscribe(const std::string& channel,
                   const std::string& session_id,
                   std::function<void()> fn);

    // Remove a session's subscription. Call from within that session's event loop.
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

### Migration from `notification_hub`

All existing call sites change mechanically:

| Old | New |
|---|---|
| `#include "widgets/notification_hub.hpp"` | `#include "widgets/live_hub.hpp"` |
| `notification_hub::instance()` | `live_hub::instance()` |
| `hub.register_session(username, sid, fn)` | `hub.subscribe("user:" + username, sid, fn)` |
| `hub.deregister_session(username, sid)` | `hub.unsubscribe("user:" + username, sid)` |
| `hub.notify_user(username)` | `hub.broadcast("user:" + username)` |

The `altinf_app::register_with_hub()` callback is also updated: replace `m_nav->refresh_bell()`
with `m_nav->update()` so that org-selector changes (membership gained/lost, org renamed)
propagate to the nav bar whenever the user channel fires.

---

## Section 2 — Channel Taxonomy

| Channel | Broadcast when | Subscribed by |
|---|---|---|
| `"user:{username}"` | Any notification event (existing); org membership changes | `altinf_app` (hub registration) |
| `"org:{org_id}"` | Invite sent/accepted/declined/rescinded; member added/removed/role-changed; team created/renamed/deleted | `kanban_team_page`, `org_landing_page` |
| `"team:{team_id}"` | Task added/edited/deleted/status-changed; team renamed | `kanban_board_page` (board + gantt) |
| `"task:{task_id}"` | Task saved or deleted by any session | `kanban_task_editor_page` (edit mode only) |
| `"accounts"` | User created/edited/deleted | `account_manager_page` |

---

## Section 3 — Page Subscription Pattern

Every subscribing page follows the same RAII pattern:

**Constructor:**
```cpp
m_session_id = Wt::WApplication::instance()->sessionId();
live_hub::instance().subscribe("org:" + std::to_string(m_org_id), m_session_id,
    [this] { refresh(); triggerUpdate(); });  // triggerUpdate() needed for server push
```

**Destructor:**
```cpp
live_hub::instance().unsubscribe("org:" + std::to_string(m_org_id), m_session_id);
```

`Wt::WApplication::instance()->triggerUpdate()` must be called inside the callback so Wt
pushes the DOM diff to the client. Each page's callback calls its own `refresh()` then
`triggerUpdate()`.

### Per-page refresh strategy

| Page | Channel | Refresh action |
|---|---|---|
| `kanban_board_page` (board) | `"team:{id}"` | Re-query tasks; call `m_board_widget->refresh(tasks, m_is_lead)` |
| `kanban_board_page` (gantt) | `"team:{id}"` | Re-query tasks; call `m_gantt_widget->refresh(tasks)` |
| `kanban_team_page` | `"org:{id}"` | Call `refresh_members()`, `refresh_pending()`, `refresh_teams()` (already exist) |
| `org_landing_page` | `"org:{id}"` | Extract constructor body into `render()`; `refresh()` = `clear()` + `render()` |
| `account_manager_page` | `"accounts"` | Extract constructor body into `render()`; `refresh()` = `clear()` + `render()` |
| `kanban_task_editor_page` | `"task:{id}"` | Call `mark_stale()` (see Section 4) |

`kanban_board_page` must store `m_session_id`, `m_team_id`, `m_is_lead`, `m_show_gantt`, and
pointers to the active board or gantt widget as members (some of these are already stored; add
what is missing). `m_show_gantt` determines which widget `refresh()` targets.

`org_landing_page` and `account_manager_page` must store all constructor parameters as members
so `render()` can be called from `refresh()`.

---

## Section 4 — Task Editor Conflict Handling

`kanban_task_editor_page` gains two new members:

```cpp
Wt::WPushButton* m_save_btn{nullptr};
Wt::WPushButton* m_del_btn{nullptr};   // only non-null in edit mode
std::string      m_session_id;
```

In the constructor, after wiring up the save and delete buttons, the existing task id is
captured:

```cpp
// Edit mode only.
if(existing)
{
    m_session_id = Wt::WApplication::instance()->sessionId();
    live_hub::instance().subscribe(
        "task:" + std::to_string(existing->id),
        m_session_id,
        [this] { mark_stale(); triggerUpdate(); });
}
```

Destructor:
```cpp
if(m_task_id != 0)
    live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
```

`m_task_id` (long long, defaulting to 0) is stored at construction time from `existing->id` so
that the destructor does not dereference the externally-owned `m_existing` pointer.

`mark_stale()` (private):
```cpp
void kanban_task_editor_page::mark_stale()
{
    m_status_msg->setText(
        "This task was modified by another user \xe2\x80\x94 saving is disabled.");
    if(m_save_btn) m_save_btn->setDisabled(true);
    if(m_del_btn)  m_del_btn->setDisabled(true);
}
```

`save()` wraps the `update_task()` call:
```cpp
try
{
    m_db.update_task(t);
}
catch(const Wt::Dbo::StaleObjectException&)
{
    mark_stale();
    return;
}
```

After any successful save or delete, broadcast to both channels so other editors and board
viewers react:
```cpp
// After successful save (new or edit):
live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
if(m_existing)
    live_hub::instance().broadcast("task:" + std::to_string(m_existing->id));

// After successful delete:
live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
live_hub::instance().broadcast("task:" + std::to_string(m_existing->id));
```

---

## Section 5 — Broadcast Call Sites (complete list)

### `notifications_page` (accept / decline invite)
- Accept → `broadcast("org:{org_id}")` + `broadcast("user:{username}")` (nav update for self)
- Decline → `broadcast("org:{org_id}")`

### `kanban_team_page` (org manage page)
- Invite sent → `broadcast("org:{org_id}")` (notification path already does `broadcast("user:{invitee}")`)
- Invite withdrawn → `broadcast("org:{org_id}")` + `broadcast("user:{invitee}")`
- Org lead toggled (promote or demote) → `broadcast("org:{org_id}")` + `broadcast("user:{member}")`
- Member removed from org → `broadcast("org:{org_id}")` + `broadcast("team:{tid}")` for each team they were on + `broadcast("user:{member}")`
  - Note: must query the user's teams *before* calling `remove_org_member` / `remove_member_from_org_teams`
- Team member added → `broadcast("org:{org_id}")` + `broadcast("team:{tid}")` + `broadcast("user:{member}")`
- Team member removed → `broadcast("org:{org_id}")` + `broadcast("team:{tid}")` + `broadcast("user:{member}")`
- Team lead promoted or demoted → `broadcast("org:{org_id}")` + `broadcast("team:{tid}")` + `broadcast("user:{member}")`
- Team renamed → `broadcast("org:{org_id}")` + `broadcast("team:{tid}")`
- Team created → `broadcast("org:{org_id}")`
- Team deleted → `broadcast("org:{org_id}")`

### `kanban_board_page` (drag-drop status change)
- Task status / sort changed → `broadcast("team:{team_id}")`

### `kanban_task_editor_page` (save / delete)
- Covered in Section 4 above.

### `altinf_app` (user delete)
- Query the deleted user's orgs and teams *before* the delete cascade.
- After `delete_user()` + `remove_user_from_all_orgs()` + `remove_member_from_all_teams()`:
  - `broadcast("accounts")`
  - `broadcast("org:{id}")` for each org the user was in
  - `broadcast("team:{id}")` for each team the user was on

### `account_editor_page` (create / edit user)
- After successful save: `broadcast("accounts")`

---

## Section 6 — Testing

### Catch2 unit tests (`tests/test_live_hub.cpp`)

Rename and update `test_notification_hub.cpp`. Use the synchronous injection hook
(`hub.reset([](auto&, auto fn){ fn(); })`).

- Subscribe to `"org:5"`, broadcast `"org:5"` → callback fires.
- Subscribe to `"org:5"`, broadcast `"org:6"` → callback does not fire.
- Subscribe to `"org:5"` and `"user:alice"`, broadcast `"org:5"` → only org callback fires.
- Subscribe same session to same channel twice → callback fires once per broadcast (or twice,
  document the behavior and assert it).
- Unsubscribe → broadcast no longer fires.
- Two sessions subscribed to same channel → both callbacks fire on broadcast.

### Playwright E2E tests

All live-update tests open two browser contexts (Session A and Session B). Assertions are made
on the *other* session's page without any navigation or reload.

#### Kanban board — board view
- **Drag-drop:** A drags a task from "To Do" to "In Progress". Assert B's board moves the card.
- **Task created:** A creates a task via the new-task form. Assert B's board shows the new card.
- **Task edited (title change):** A edits a task title and saves. Assert B's board shows the updated title.
- **Task deleted:** A deletes a task from the editor. Assert B's board no longer shows the card.

#### Kanban board — Gantt view
- **Date change:** A edits start/end dates on a task. Assert B on `/board/{tid}/gantt` updates
  that task's bar.
- **Task created with dates:** A creates a task with dates. Assert B's Gantt view adds a bar.
- **Task deleted:** A deletes a task. Assert B's Gantt view removes the bar.

#### Task editor — conflict detection
- **Live-push path:** A and B both open `/board/{tid}/task/{id}/edit`. B saves a change. Assert
  A's editor shows the conflict message and Save is disabled *before* A attempts to save.
- **Delete-while-editing:** A opens the editor. B deletes the task. Assert A's editor shows the
  conflict message and Save is disabled.
- **Stale-exception path:** This is a race between the push arriving and the user clicking Save,
  which is not reliably reproducible in a black-box E2E test. Cover it at the unit/integration
  level: inject a `kanban_db` that throws `Wt::Dbo::StaleObjectException` from `update_task()`,
  call `save()` directly, and assert `mark_stale()` was invoked (Save button disabled, message
  set).

#### Org manage page — members
- **Accept invite:** A (lead) is on `/org/{id}/manage`. B accepts an invite on `/notifications`.
  Assert A's page moves B from Pending to Members.
- **Decline invite:** B declines an invite. Assert A's pending section removes B's row.
- **Invite rescinded:** A withdraws an invite while C (second lead) is on the same manage page.
  Assert C's pending section updates.
- **Promoted to org lead:** A promotes B. Assert C (another viewer) sees "(lead)" next to B.
- **Demoted from org lead:** A demotes B. Assert C sees the label removed.
- **Member removed from org:** A removes B. Assert C's member list no longer contains B.

#### Org manage page — teams
- **Team created:** A creates a team. Assert C (second lead on the page) sees the new team block.
- **Team renamed:** A renames a team. Assert C's page shows the new name in the team header.
- **Team deleted:** A deletes a team. Assert C's page loses that team block.
- **Member added to team:** A adds B to a team. Assert C sees B in that team's member list.
- **Member removed from team:** A removes B from a team. Assert C no longer sees B in that block.
- **Team lead promoted:** A makes B a team lead. Assert C sees "(lead)" next to B.
- **Team lead demoted:** A removes B's team lead status. Assert C sees the label removed.

#### Org landing page
- **Team created:** A (lead) creates a team via the manage page. Assert B on `/org/{id}` sees
  the new team appear.
- **Team renamed:** A renames a team. Assert B on the landing page sees the updated link text.
- **Team deleted:** A deletes a team. Assert B's landing page no longer shows it.
- **User added to team:** A adds B to a team. Assert B's landing page moves that team from
  "Other teams" to "Your teams".
- **User removed from team:** A removes B from a team. Assert B's landing page moves the team
  back to "Other teams".

#### Nav bar — org selector
- **Invite accepted:** B accepts an org invite. Assert B's nav bar gains the new org in its
  selector without navigating away.
- **Removed from org:** A (lead) removes B. Assert B's nav bar loses that org from its selector.

#### Account manager page
- **User created:** A (admin) creates a new account. Assert B (also on `/admin/accounts`) sees
  the new row.
- **User deleted:** A deletes a user. Assert B's table removes that row.
- **User edited (permissions changed):** A edits a user's permissions and saves. Assert B's table
  shows the updated permissions label for that user.

#### Notification bell (regression)
- **New notification:** A (lead) invites B. Assert B's nav bell badge increments.
- **Notification dismissed:** B dismisses a notification on `/notifications`. Assert B's bell
  badge decrements in any other open tab belonging to B.

---

## Open questions / non-goals

- Org rename is not currently implemented; if added later it should broadcast `"org:{id}"` and
  `"user:{u}"` for every member.
- The task editor form fields are not live-updated (only Save is blocked). If a future version
  wants to show the incoming changes in the form, that is a separate design.
- Blog post pages are explicitly out of scope.
