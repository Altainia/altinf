# Phase 2: Notification Dispatch, User Preferences, and Team Settings UI — Design

## Goal

Wire up the three notification event types deferred from Phase 1 (`task_available`, `task_unassigned`, `task_abandoned`), add a `task_coassignee_changed` notification, and build the two settings UIs (user notification preferences at `/settings`; team permission settings at `/board/{team_id}/settings`).

## Architecture

Notification logic is extracted into a standalone helper module (`kanban_notifications`) with two free functions called by `task_editor_form_widget` after successful assignee mutations. This keeps the widget readable and the notification logic independently testable without requiring a full widget tree. The two new settings pages follow the same widget-per-page pattern used by the rest of the app.

## Tech Stack

C++17, Wt 4.x (Wt::Dbo ORM on SQLite), Catch2 v3 for unit tests. Existing `kanban_db`, `org_db`, `live_hub` infrastructure.

---

## Section 1: Notification Dispatch

### New module: `src/kanban/kanban_notifications.hpp/cpp`

Two free functions. Both are called from `task_editor_form_widget` chip handlers after a successful `add_assignee()` or `remove_assignee()` call.

```cpp
// Call after a successful add_assignee().
void notify_assignee_added(
    kanban_db&         db,
    org_db&            odb,
    long long          task_id,
    long long          team_id,
    long long          org_id,
    const std::string& added_user,
    const std::string& actor);

// Call after a successful remove_assignee().
// remaining_assignees: result of db.assignees_for_task(task_id) *after* removal.
void notify_assignee_removed(
    kanban_db&                       db,
    org_db&                          odb,
    long long                        task_id,
    long long                        team_id,
    long long                        org_id,
    const std::string&               removed_user,
    const std::string&               actor,
    const std::vector<std::string>&  remaining_assignees);
```

### `notify_assignee_added` — what fires

| Event | Recipient | Pref check |
|---|---|---|
| `task_assigned` | `added_user` (if `added_user != actor`) | None — direct act |
| `task_coassignee_changed` | Every other current assignee (not `added_user`) | `notify_coassignee_changed` |

After all pushes: `live_hub::instance().broadcast("user:{u}")` for each user notified.

"Other current assignees" is obtained by calling `db.assignees_for_task(task_id)` inside the helper. This is called after `add_assignee()` has already committed, so `added_user` is present in the result; exclude `added_user` from the coassignee loop.

### `notify_assignee_removed` — what fires

| Condition | Event | Recipient | Pref check |
|---|---|---|---|
| `actor != removed_user` | `task_unassigned` | `removed_user` | `notify_task_unassigned` |
| `actor == removed_user` and `remaining_assignees.empty()` | `task_abandoned` | All team members except `removed_user` | `notify_task_abandoned` |
| `remaining_assignees.empty()` (any actor) | `task_available` | All team members except `removed_user` | `notify_task_available` |
| Always (if any remain) | `task_coassignee_changed` | Each member of `remaining_assignees` | `notify_coassignee_changed` |

After all pushes: `live_hub::instance().broadcast("user:{u}")` for each user notified.

"All team members" is obtained via `db.members_for_team(team_id)`. Pref check uses `odb.get_user_org_pref(username, org_id)`.

Note: when a sole assignee self-removes, both `task_abandoned` and `task_available` may fire to the same recipient if they have both prefs enabled. This is intentional — they are distinct opt-in signals.

### No notifications from `maybe_clear_assignees_for_done`

The Done-column assignee clearing (`kanban_db::maybe_clear_assignees_for_done`) stays silent. No notification helpers are called from that path.

### Replace existing inline dispatch

The inline `task_assigned` push in `task_editor_form_widget.cpp`'s "Add" button handler (currently at the add-assignee click site) is replaced by a call to `notify_assignee_added`.

### New payload helpers added to `src/org/org.hpp`

```cpp
inline std::string make_task_available_payload(
    long long task_id, const std::string& task_title,
    long long team_id, const std::string& team_name);

inline std::string make_task_unassigned_payload(
    long long task_id, const std::string& task_title,
    long long team_id, const std::string& team_name);

inline std::string make_task_abandoned_payload(
    long long task_id, const std::string& task_title,
    long long team_id, const std::string& team_name,
    const std::string& abandoned_by);

inline std::string make_task_coassignee_changed_payload(
    long long task_id, const std::string& task_title,
    long long team_id, const std::string& team_name,
    const std::string& changed_user, const std::string& action);
// action = "added" | "removed"
```

### New rendering branches in `src/pages/notifications_page.cpp`

Four new `else if(n.type == ...)` branches for: `task_available`, `task_unassigned`, `task_abandoned`, `task_coassignee_changed`. Each renders a human-readable message with a link to the task editor and a dismiss button.

---

## Section 2: User Notification Preferences — `/settings`

### New files

- `src/pages/settings_page.hpp`
- `src/pages/settings_page.cpp`

### Route

`/settings` registered in `altinf_app.cpp`. Accessible to any logged-in user. Not accessible when logged out (redirects to login).

### Layout

One section per org the user actively belongs to (via `org_db::orgs_for_user()`). Each section:

- Org name as a heading
- Four `Wt::WCheckBox` widgets, pre-populated from `org_db::get_user_org_pref(username, org_id)` on load

| Label | Pref field |
|---|---|
| New task available | `notify_task_available` |
| Removed from task | `notify_task_unassigned` |
| Co-assignee changed | `notify_coassignee_changed` |
| Task abandoned | `notify_task_abandoned` |

Each checkbox saves immediately on `changed()` via `org_db::set_user_org_pref()`. No Save button.

If the user belongs to no active orgs, the page shows a brief message: "You are not a member of any organizations."

### Nav bar change

A "Settings" text link is added to the nav bar (alongside the notification bell), visible to all logged-in users. Points to `/settings`.

---

## Section 3: Team Settings Page — `/board/{team_id}/settings`

### New files

- `src/pages/team_settings_page.hpp`
- `src/pages/team_settings_page.cpp`

### Route

`/board/{team_id}/settings` registered in `altinf_app.cpp`. Gated on `team_cap::manage_team` (leads only). Non-leads visiting this URL are redirected to `/board/{team_id}`.

### Layout — two sections

**Team name**

A text input pre-filled with the current team name (from `kanban_db::find_team()`). A Save button calls `kanban_db::rename_team(team_id, new_name)`. On save, broadcasts `live_hub::instance().broadcast("team:" + std::to_string(team_id))` so any open board view updates.

**Member permissions**

Four `Wt::WCheckBox` widgets, pre-populated from `kanban_db::settings_for_team(team_id)` on load. Saves immediately on `changed()` via `kanban_db::set_team_settings()`.

| Label | Settings field |
|---|---|
| Members can move tasks between columns | `allow_member_move_columns` |
| Members can self-assign unassigned tasks | `allow_self_assign_unassigned` |
| Members can self-assign already-assigned tasks | `allow_self_assign_assigned` |
| Members can abandon tasks | `allow_abandon` |

### Org manage page change (`src/pages/kanban_team_page.cpp`)

Remove the rename button and its handler (lines 338–345, the `rename_btn` block). The team name continues to display as a read-only header on that page.

### Board page header change (`src/pages/kanban_board_page.cpp`)

A "Settings" link is added to the board header, visible only when `m_caps.has_any(team_cap::manage_team)`. Points to `/board/{team_id}/settings`.

---

## File Map

| File | Change |
|---|---|
| `src/kanban/kanban_notifications.hpp` | Create: `notify_assignee_added`, `notify_assignee_removed` declarations |
| `src/kanban/kanban_notifications.cpp` | Create: implementations of both helpers |
| `src/org/org.hpp` | Add four payload helper inline functions |
| `src/kanban/task_editor_form_widget.cpp` | Replace inline task_assigned push with `notify_assignee_added`; add `notify_assignee_removed` call in chip remove handler; include `kanban_notifications.hpp` |
| `src/pages/notifications_page.cpp` | Add four new notification type rendering branches |
| `src/pages/settings_page.hpp` | Create: `settings_page` widget class |
| `src/pages/settings_page.cpp` | Create: per-org pref toggle UI |
| `src/widgets/nav_bar.hpp/cpp` | Add "Settings" link |
| `src/altinf_app.cpp` | Register `/settings` and `/board/{team_id}/settings` routes |
| `src/pages/team_settings_page.hpp` | Create: `team_settings_page` widget class |
| `src/pages/team_settings_page.cpp` | Create: rename + permission toggles UI |
| `src/pages/kanban_team_page.cpp` | Remove team rename button and handler |
| `src/pages/kanban_board_page.cpp` | Add "Settings" link to board header for leads |

---

## Testing

**Unit tests (`tests/test_kanban_notifications.cpp` — new)**
- `notify_assignee_added`: added_user gets `task_assigned`; self-add fires no `task_assigned`; other assignees get `task_coassignee_changed` if pref enabled; pref-disabled users get nothing
- `notify_assignee_removed`: lead-remove fires `task_unassigned` to removed user with pref; self-remove as sole assignee fires `task_abandoned`; sole-assignee removal fires `task_available`; remaining assignees get `task_coassignee_changed`; pref-disabled users excluded

**Existing tests**
- `tests/test_kanban_db.cpp`: no changes required (notification helpers are above the DB layer)
- `tests/test_org_db.cpp`: no changes required

**Manual / E2E**
- Lead assigns Alice: Alice gets `task_assigned` notification; Bob (other assignee) gets `task_coassignee_changed`
- Lead removes Alice: Alice gets `task_unassigned`; remaining assignees get `task_coassignee_changed`
- Alice self-removes as sole assignee: all team members with `notify_task_abandoned` get `task_abandoned`; all with `notify_task_available` get `task_available`
- User disables `notify_task_available` on `/settings`: they no longer receive that notification
- Lead visits `/board/{team_id}/settings`: can rename team and toggle permission flags
- Non-lead visiting `/board/{team_id}/settings`: redirected to board
- Rename on team settings page updates board page title; rename form no longer appears on org manage page
