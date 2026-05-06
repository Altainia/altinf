# Task History & Soft Delete — Design

**Date:** 2026-05-06

## Overview

Track every meaningful change to a task (who changed what field, from what value to what value, and exactly when). No task row is ever physically removed from the database; user-initiated "delete" becomes "archive." Team and org deletions also archive rather than remove. Users who can view a task can see its full history.

---

## Database Schema

### Existing tables — new columns (added via ALTER TABLE migration)

| Table | New column | Type | Default |
|---|---|---|---|
| `kanban_task` | `is_archived` | INTEGER NOT NULL | 0 |
| `team` | `is_archived` | INTEGER NOT NULL | 0 |
| `org` | `is_archived` | INTEGER NOT NULL | 0 |

### New table: `task_event`

| Column | Type | Notes |
|---|---|---|
| `id` | INTEGER PK | Wt Dbo auto |
| `task_id` | INTEGER | FK to kanban_task |
| `actor` | TEXT | Username of the user who made the change |
| `occurred_at` | TEXT | ISO 8601 UTC, e.g. `2026-05-06T14:30:00Z` |
| `event_type` | TEXT | `"created"`, `"updated"`, `"archived"`, `"unarchived"` |

### New table: `task_field_change`

| Column | Type | Notes |
|---|---|---|
| `id` | INTEGER PK | Wt Dbo auto |
| `event_id` | INTEGER | FK to task_event |
| `field_name` | TEXT | `"status"`, `"title"`, `"description"`, `"assigned_to"`, `"start_date"`, `"end_date"`, `"type"` |
| `old_value` | TEXT | Previous value as string; `""` for unset/null |
| `new_value` | TEXT | New value as string |

**Field value encoding:**
- `sort_order` is excluded — changes are too frequent and carry no audit value.
- `type` stores the type **name** (e.g., `"Bug"`), not the ID, so history remains accurate if a type is later renamed or deleted.
- Dates are stored as `"yyyy-MM-dd"` or `""` for unset.
- `status` is stored as the internal value (`"todo"`, `"in_progress"`, `"review"`, `"done"`); the UI maps to human-readable labels on display.

---

## New Dbo Records and Entry Structs (kanban.hpp)

```cpp
struct task_event_record {
    long long   task_id{0};
    std::string actor;
    std::string occurred_at;
    std::string event_type;

    template<class Action>
    void persist(Action& a) {
        Wt::Dbo::field(a, task_id,     "task_id");
        Wt::Dbo::field(a, actor,       "actor");
        Wt::Dbo::field(a, occurred_at, "occurred_at");
        Wt::Dbo::field(a, event_type,  "event_type");
    }
};

struct task_field_change_record {
    long long   event_id{0};
    std::string field_name;
    std::string old_value;
    std::string new_value;

    template<class Action>
    void persist(Action& a) {
        Wt::Dbo::field(a, event_id,    "event_id");
        Wt::Dbo::field(a, field_name,  "field_name");
        Wt::Dbo::field(a, old_value,   "old_value");
        Wt::Dbo::field(a, new_value,   "new_value");
    }
};

struct task_field_change_entry {
    std::string field_name;
    std::string old_value;
    std::string new_value;
};

struct task_event_entry {
    long long   id{0};
    long long   task_id{0};
    std::string actor;
    std::string occurred_at;
    std::string event_type;
    std::vector<task_field_change_entry> changes;
};
```

---

## kanban_db API Changes

### Signature changes

| Old | New |
|---|---|
| `add_task(entry)` | `add_task(entry, actor)` |
| `update_task(entry)` | `update_task(entry, actor)` |
| `update_task_status(id, status, sort_order)` | `update_task_status(id, status, sort_order, actor)` |
| `delete_task(id)` | `archive_task(id, actor)` |
| `delete_team(id)` | `archive_team(id, actor)` |
| `self_assign(task_id, username)` | `self_assign(task_id, username)` — no signature change; records a history event for the `assigned_to` field change internally (actor = username) |

### New public methods

```cpp
void archive_task(long long id, const std::string& actor);
void unarchive_task(long long id, const std::string& actor);
void archive_team(long long id, const std::string& actor);
std::vector<task_event_entry> history_for_task(long long task_id);
```

### Query filter changes

- `tasks_for_team`: gains `AND is_archived = 0`.
- `find_task`: still returns archived tasks (history and editor still need them).
- `teams_for_org` and `all_teams`: gain `AND is_archived = 0`.

### Private helper

```cpp
// Called inside the caller's open transaction.
void record_event(long long task_id,
                  const std::string& actor,
                  const std::string& event_type,
                  const std::vector<task_field_change_entry>& changes);
```

`occurred_at` is computed at call time using `std::chrono` / `std::gmtime` to produce an ISO 8601 UTC string.

### Diff logic in update_task

`update_task` loads the existing row before writing. It compares each tracked field (status, title, description, assigned_to, start_date, end_date, type_id→type_name). Fields that differ produce a `task_field_change_entry`. If the resulting changes vector is empty, no event is written. The type name for the diff is resolved from `find_task_type` for old and new type_id values (0 maps to `""`).

### archive_team cascade

`archive_team` marks the team `is_archived=1`, then for each task in the team sets `is_archived=1` and calls `record_event` with `event_type="archived"` and an empty changes vector. All within one transaction. Team member rows are preserved.

---

## org_db API Changes

- `org_record` gets `is_archived INTEGER NOT NULL DEFAULT 0` via ALTER TABLE migration.
- `all_orgs` and `orgs_for_user` gain `WHERE is_archived = 0`.
- New method: `archive_org(long long id, const std::string& actor)` — marks org archived. No UI caller yet; infrastructure only. Does not cascade to teams/tasks (that cascade is left to the caller so the actor is threaded correctly).

---

## UI: kanban_task_editor_page

### Tab structure

The page body gains a `WTabWidget` with two tabs:

1. **Details** — the existing form, unchanged in content.
2. **History** — read-only, populated from `history_for_task`.

### Archive / Unarchive button

- The "Delete" button becomes "Archive" (calls `archive_task(id, m_username)`).
- For archived tasks opened by a lead, an "Unarchive" button appears instead.
- Non-leads never see Archive or Unarchive.

### History tab rendering

Events are shown newest-first. Each event renders as:

```
May 6, 2026 at 14:30  —  alice
  Status: To Do → In Progress
  Assigned to: (unassigned) → bob

May 5, 2026 at 09:15  —  alice
  [Task created]
```

- "created", "archived", "unarchived" events: header line only (no field rows).
- "updated" events: header + one line per `task_field_change_entry`.
- `field_name` → human-readable label mapping: `status`→"Status", `title`→"Title", `description`→"Description", `assigned_to`→"Assigned to", `start_date`→"Start date", `end_date`→"End date", `type`→"Type".
- Empty string values display as "(unset)".
- `status` raw values display as their human-readable label (same map used in the form).
- The history tab is rebuilt from the DB on every switch to it (no live-update subscription needed; the data is read-only and low-frequency).

---

## Callers

| Call site | File | Change |
|---|---|---|
| `delete_task` | `kanban_task_editor_page.cpp:283` | → `archive_task(id, m_username)` |
| `update_task` | `kanban_task_editor_page.cpp` (save) | → `update_task(entry, m_username)` |
| `add_task` | `kanban_task_editor_page.cpp` (save) | → `add_task(entry, m_username)` |
| `update_task_status` | `kanban_board_widget.cpp` | → `update_task_status(id, status, sort_order, actor)` — actor threaded from session |
| `delete_team` | `kanban_team_page.cpp:326` | → `archive_team(id, actor)` — actor from session |

---

## Testing

- Catch2: unit tests for `record_event`, `history_for_task`, `archive_task`, `archive_team`, and the diff logic in `update_task`.
- Playwright E2E: create a task, edit several fields, verify history tab shows correct before/after values and actor; archive a task and verify it disappears from the board; verify history tab is accessible on archived tasks opened by URL.
