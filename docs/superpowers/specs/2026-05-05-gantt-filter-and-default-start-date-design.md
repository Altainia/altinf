# Design: Gantt Status Filter and Default Start Date

**Date:** 2026-05-05

## Overview

Two small, focused changes to the Kanban/Gantt feature:

1. Hide "To Do" and "Done" tasks from the Gantt view (live, including on status change)
2. Default the start date to today when creating a new task

---

## Change 1 — Gantt Status Filter

### Requirement

Tasks with status `"todo"` or `"done"` must not appear in the Gantt view. This must apply live: when a task is dragged to "Done" or "To Do" on the board, other sessions viewing the Gantt see it disappear without a page reload.

### Architecture

The live update path is:

```
drag-drop callback
  → kanban_db::update_task_status()
  → live_hub::broadcast("team:<id>")
  → kanban_board_page::refresh()          [all subscribed sessions]
      → kanban_db::tasks_for_team()
      → gantt_view_widget::refresh(tasks)
          → serialize_tasks(tasks)         ← filter applied here
              → initGantt() JS call
```

`serialize_tasks()` is the single point through which all tasks pass before being sent to the browser — both on initial render (constructor) and every live refresh. Filtering here means no call-site changes are needed.

### Implementation

In `src/kanban/gantt_view_widget.cpp`, `serialize_tasks()`: add a guard at the top of the loop body:

```cpp
if(t.status == "todo" || t.status == "done")
    continue;
```

`kanban_task_entry::status` is already populated by `tasks_for_team()`. No schema or API changes required.

---

## Change 2 — Default Start Date to Today

### Requirement

When a user opens the new-task form, the start date field should be pre-populated with today's date. End date remains empty.

### Implementation

In `src/pages/kanban_task_editor_page.cpp`, after the start-date `WDateEdit` is created and before the existing-task branch populates it from the DB, set the default for new tasks:

```cpp
// new task path (no existing task)
m_start_date->setDate(Wt::WDate::currentDate());
```

This is inside the `else` branch (no existing task), so existing tasks continue to show their stored start date. The user can still clear or change the pre-filled date before saving.

---

## Files Changed

| File | Change |
|------|--------|
| `src/kanban/gantt_view_widget.cpp` | Skip `"todo"` and `"done"` tasks in `serialize_tasks()` |
| `src/pages/kanban_task_editor_page.cpp` | Pre-populate start date with today for new tasks |

## Testing

- Playwright E2E: verify "To Do" and "Done" tasks don't appear in Gantt; "In Progress" and "Review" do
- Playwright E2E: verify dragging a task to "Done" on the board removes it from a live Gantt session
- Playwright E2E: verify new-task form opens with today's date pre-filled in start date
- JS unit tests: no changes expected
- Catch2: no changes expected
