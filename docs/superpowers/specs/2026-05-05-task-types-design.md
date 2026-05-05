# Task Types Design

**Date:** 2026-05-05
**Status:** Approved

## Overview

Introduce a "task type" concept: org leads create named types for their org, each with a color. A task's color is derived entirely from its type. Tasks with no type are light gray (`#cccccc`). The per-task color picker is removed.

---

## 1. Data Model

### New table: `task_type`

```sql
CREATE TABLE task_type (
    id     INTEGER PRIMARY KEY AUTOINCREMENT,
    org_id INTEGER NOT NULL,   -- FK → organization.id
    name   TEXT    NOT NULL,
    color  TEXT    NOT NULL    -- hex string e.g. "#e07b54"
);
```

New Dbo record and plain entry structs added to `kanban.hpp`:

```cpp
struct task_type_record {
    long long   org_id{0};
    std::string name;
    std::string color;
    // persist(...)
};

struct task_type_entry {
    long long   id{0};
    long long   org_id{0};
    std::string name;
    std::string color;
};
```

### Modified table: `kanban_task`

The `color TEXT` column is removed and replaced with:

```sql
type_id INTEGER NOT NULL DEFAULT 0   -- 0 = no type
```

`kanban_task_record` and `kanban_task_entry` in `kanban.hpp` change accordingly: `color: string` → `type_id: long long`.

### Schema migration

Drop and recreate all kanban/org tables on startup (existing policy — app not yet deployed).

---

## 2. `kanban_db` Changes

New methods added to `kanban_db`:

| Method | Description |
|--------|-------------|
| `create_task_type(org_id, name, color) → long long` | Insert a new type row, return its id |
| `update_task_type(id, name, color)` | Update name and color for an existing type |
| `delete_task_type(id)` | Zero out `type_id` on all tasks using this type, then delete the row |
| `types_for_org(org_id) → vector<task_type_entry>` | Return all types for the org, ordered by id |
| `find_task_type(id) → optional<task_type_entry>` | Look up a single type by id |

`delete_task_type` runs both steps in one transaction:
1. `UPDATE kanban_task SET type_id=0 WHERE type_id=?`
2. `DELETE FROM task_type WHERE id=?`

---

## 3. Color Resolution (Board & Gantt)

`kanban.js` reads `task.color` as a left-border color. `gantt.js` reads `task.color` as the bar fill. Neither JS file changes.

On the C++ side:

- `kanban_task_entry` no longer has a `color` field — it has `type_id`.
- `kanban_board_widget` and `gantt_view_widget` each gain a `const std::map<long long, std::string>& type_colors` constructor/refresh parameter.
- When building the JSON for the JS, they resolve: `color = type_colors.count(type_id) ? type_colors.at(type_id) : "#cccccc"`.

`kanban_board_page` already calls `db.find_team(team_id)` (which returns `org_id`). It calls `db.types_for_org(org_id)` and builds the map before constructing or refreshing the widgets.

---

## 4. Task Editor Changes

`kanban_task_editor_page` constructor gains:

```cpp
const std::vector<task_type_entry>& types
```

### Type selector (Option C — labeled chips)

The "Color" section is renamed "Type". The `WColorPicker` is replaced by a chip row:

- A "(None)" chip (gray dot + text) is always first.
- One chip per type: small colored circle + type name.
- Each chip is a `WContainerWidget` with a clicked signal.
- Clicking a chip removes the `selected` CSS class from all chips, adds it to the clicked chip, and sets `m_type_id` (a `long long` member, 0 = none).
- When editing an existing task, the chip matching `existing->type_id` is pre-selected. If that type_id is not present in the `types` list (the type was deleted after the task was created), the editor defaults to "(None)" (type_id = 0).

On save, `kanban_task_entry::type_id = m_type_id`.

### Routing changes (`altinf_app.cpp`)

Both task editor construction sites (new task at `/board/{id}/task/new` and edit task at `/board/{id}/task/{tid}/edit`) already resolve `team->org_id`. Each site adds:

```cpp
const auto types = m_kanban_db->types_for_org(team->org_id);
```

and passes `types` to the `kanban_task_editor_page` constructor.

---

## 5. Type Manager Page

### New page: `org_type_manager_page`

- **Route:** `/org/{id}/types`
- **Access:** Org leads only (checked in routing, same pattern as other org-lead pages).
- **Constructor:** `org_type_manager_page(kanban_db&, org_db&, long long org_id, const session_data&)`

### Layout

**Existing types list** — one row per type:
- Small colored circle (16px), type name, "Edit" button, "Delete" button.

**Add new type form** (below the list):
- Name `WLineEdit`, `WColorPicker`, "Add Type" `WPushButton`, status message `WText`.
- Default color cycles through a hardcoded palette of ~10 visually distinct colors: `palette[types.size() % palette.size()]`. The palette is chosen so consecutive defaults are clearly distinguishable and none clash with `#cccccc` (the "no type" gray).

**Edit behavior:** Clicking "Edit" replaces that type's row with an inline form (name input + color picker + "Save" and "Cancel" buttons). Saving calls `update_task_type` and broadcasts `"org:{id}:types"` via `live_hub` so open boards re-fetch their type map and re-render.

**Delete behavior:** Clicking "Delete" calls `delete_task_type(id)`, which resets affected tasks to type 0 (gray) before removing the row. No tasks are lost.

### Navigation

`org_board_page` gains a "Manage Types" link in its header, visible to org leads only — same pattern as "Manage Team" on the team board page.

---

## 6. Live Updates

When a type's color or name changes (or a type is deleted), the type manager page broadcasts `"org:{id}:types"` via `live_hub`. `kanban_board_page` subscribes to this channel at construction time (alongside the existing `"team:{id}"` subscription). On receipt, it calls `types_for_org` again, rebuilds the type colors map, and passes it to the board or Gantt widget's refresh method to re-render.

---

## 7. Affected Files

| File | Change |
|------|--------|
| `src/kanban/kanban.hpp` | Add `task_type_record`, `task_type_entry`; replace `color` with `type_id` in task structs |
| `src/kanban/kanban_db.hpp` | Declare new type CRUD methods |
| `src/kanban/kanban_db.cpp` | Implement new type CRUD; update `to_entry` and task CRUD for `type_id` |
| `src/kanban/kanban_board_widget.hpp/.cpp` | Add `type_colors` param; resolve color in JSON builder |
| `src/kanban/gantt_view_widget.hpp/.cpp` | Add `type_colors` param; resolve color in JSON builder |
| `src/pages/kanban_board_page.hpp/.cpp` | Load `types_for_org`; build `type_colors` map; pass to widgets; subscribe to `"org:{id}:types"` |
| `src/pages/kanban_task_editor_page.hpp/.cpp` | Add `types` param; replace color picker with chip selector; write `type_id` on save |
| `src/pages/org_type_manager_page.hpp/.cpp` | New page: full type CRUD UI |
| `src/pages/org_board_page.hpp/.cpp` | Add "Manage Types" link for org leads |
| `src/altinf_app.cpp` | Add `/org/{id}/types` route; pass `types` to task editor construction sites |
| `resources/scss/` | Styles for chip selector (`kb-type-chip`, `.selected`) and type manager rows |
