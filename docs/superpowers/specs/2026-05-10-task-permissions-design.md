# Task Permissions — Design Spec

**Date:** 2026-05-10

## Goal

Expand task access so that:
- Any team member can edit non-archived tasks on their team.
- Org members who are not on a team can view that team's board and tasks in read-only mode.
- Archived tasks remain invisible and uneditable for everyone except team leads, org leads, and admins.

## Current State

| Actor | Board view | Task edit |
|---|---|---|
| Admin | Yes | Full |
| Org lead | Yes | Full |
| Team lead | Yes | Full |
| Team member | Yes | Own assigned tasks only; can self-assign to unassigned tasks |
| Org member (not on team) | No | No |
| Non-member | No | No |

Task creation, archiving, and team management are lead-only. `can_view_board` and `can_edit_board` on `kanban_db` encode access as two separate booleans threaded through callers.

## Desired State

| Actor | Board view | Edit task fields | Self-assign | Reassign | Create | Archive | View archived | Comment |
|---|---|---|---|---|---|---|---|---|
| Admin | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Org lead | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Team lead | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Team member | Yes | Yes | Yes | No | No | No | No | Yes |
| Org member (not on team) | Yes (read-only) | No | No | No | No | No | No | No |
| Non-member | No | No | No | No | No | No | No | No |

"Edit task fields" means: title, description, status, start/end dates, type.

## Approach: Per-Decision Capability Bitfield (`alt::flags`)

Each callsite queries the exact capability it needs. Capabilities are computed once per route entry via a new `resolve_team_caps` method.

## Section 1 — Data Model

New header `src/kanban/team_cap.hpp`, following the `permission.hpp` / `alt::flags<bit>` convention:

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

    // Preset bundles
    inline constexpr flags org_viewer_caps  = view_board;
    inline constexpr flags team_member_caps = view_board | edit_task_fields | self_assign | comment;
    inline constexpr flags team_lead_caps   = team_member_caps | view_archived
                                            | reassign_task | create_task
                                            | archive_task | manage_team;
    inline constexpr flags org_lead_caps    = team_lead_caps;
    inline constexpr flags admin_caps       = ~flags{};
}
```

Callsites use `.has_any()` / `.has_all()` / `.has_none()` consistent with `permission::flags` usage in `altinf_app.cpp`.

## Section 2 — Capability Computation

New method on `altinf_app` (mirrors `resolve_is_org_lead`):

```cpp
// altinf_app.hpp
team_cap::flags resolve_team_caps(long long team_id, long long org_id);

// altinf_app.cpp
team_cap::flags altinf_app::resolve_team_caps(long long team_id, long long org_id)
{
    if(m_session.permissions.has_any(permission::admin))
        return team_cap::admin_caps;
    if(!m_session.logged_in)
        return {};
    if(m_org_db->is_org_lead(org_id, m_session.username))
        return team_cap::org_lead_caps;
    if(m_kanban_db->is_team_lead(team_id, m_session.username))
        return team_cap::team_lead_caps;
    if(m_kanban_db->is_member(team_id, m_session.username))
        return team_cap::team_member_caps;
    if(m_org_db->is_org_member(org_id, m_session.username))
        return team_cap::org_viewer_caps;
    return {};
}
```

Short-circuit order matters: admin and org lead skip team DB queries entirely. `is_org_member` must only match `status = 'active'` rows — verify the existing query filters by status; fix it if not.

`can_view_board` and `can_edit_board` on `kanban_db` are removed (only called from `altinf_app`). `resolve_is_org_lead` stays — it is still used for org-level routes.

The team routing block in `altinf_app` replaces the `is_org_lead` / `is_team_lead` / `is_lead` triple with a single `caps` value:

```cpp
const auto caps = resolve_team_caps(team_id, team->org_id);
```

## Section 3 — Enforcement Points

### A. Task Editor (`kanban_task_editor_page`)

Constructor signature: `bool is_lead` → `team_cap::flags caps`. Stored as `team_cap::flags m_caps` (replaces `bool m_is_lead`).

Locking derivation at construction time:

```cpp
const bool can_edit   = caps.has_any(team_cap::edit_task_fields)
                     && (!existing || !existing->is_archived);
const bool can_assign = caps.has_any(team_cap::reassign_task)
                     && (!existing || !existing->is_archived);
const bool locked_out = !can_assign
                     && !current_assignee.empty()
                     && current_assignee != session.username;
```

Field consequences:

| Field | New gate |
|---|---|
| Title (existing task) | `!can_edit` → read-only |
| Description | `!can_edit` → read-only (closes current gap — was unrestricted) |
| Status | `!can_edit` → disabled |
| Assignee options | `can_assign` → full member list; else self + unassigned only |
| Assignee widget | `locked_out` → disabled |
| Start / end dates | `!can_edit` → read-only (closes current gap — was unrestricted) |
| Type chips | `!can_edit` → disabled |
| Save button | hidden if `!can_edit` |
| Archive / Unarchive button | shown only if `caps.has_any(team_cap::archive_task)` |

`save()` re-checks `m_caps.has_any(team_cap::edit_task_fields)` before writing — UI locking alone is not sufficient.

### B. Comments

Compose area and per-comment edit/delete buttons only rendered when `caps.has_any(team_cap::comment)`. Org-only viewers see the comment list but no compose or action buttons.

### C. Board Page (`kanban_board_page`)

`bool is_lead` → `team_cap::flags caps`. The "New Task" button is shown only when `caps.has_any(team_cap::create_task)`. No other board-level behavior changes.

### D. Routing in `altinf_app`

| Route | Old gate | New gate |
|---|---|---|
| Board / Gantt | `can_view_board()` | `caps.has_any(team_cap::view_board)` |
| `/task/new` | `!is_lead` | `caps.has_any(team_cap::create_task)` |
| `/task/{id}/edit` | `can_view_board()` | `caps.has_any(team_cap::view_board)` + archived guard (see below) |
| `/archive` | `!is_lead` | `caps.has_any(team_cap::view_archived)` |
| `/manage` | `!is_lead` | `caps.has_any(team_cap::manage_team)` |

**Archived task guard** on the task edit route: after loading the task record, before constructing the editor:

```cpp
if(opt->is_archived && !caps.has_any(team_cap::view_archived))
{
    show_not_found();  // not show_forbidden() — task existence is invisible
    return;
}
```

## Section 4 — Edge Cases & Testing

**Edge cases:**

- `is_org_member` status filter: `resolve_team_caps` must only grant `org_viewer_caps` to active org members (`status = 'active'`). Verify the existing query; fix if needed.
- Unauthenticated users: `resolve_team_caps` returns `{}` immediately when `!m_session.logged_in` — no DB queries issued.
- Archived task via direct URL: `show_not_found()` (not `show_forbidden()`) keeps the existence of archived tasks invisible to non-leads.
- Save bypass: `save()` re-checks caps server-side before writing.
- `m_is_lead` member: replaced by `m_caps`; update all references in `save()` and comment handlers.

**Playwright E2E scenarios:**

| Actor | Action | Expected |
|---|---|---|
| Team member (non-lead) | Navigate to board | Board visible |
| Team member | Open non-archived task | All fields editable; no Archive button |
| Team member | Open archived task via direct URL | 404 |
| Team member | POST to `/task/new` | Forbidden |
| Org member (not on team) | Navigate to board | Board visible, no New Task button |
| Org member | Open task | All fields read-only, save button hidden, no compose |
| Org member | Open archived task via direct URL | 404 |
| Non-org user | Navigate to board | Forbidden |

**Catch2 unit tests:** Cover `resolve_team_caps` directly — one test per actor level verifying the exact capability bitfield returned.
