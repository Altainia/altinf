# Task Popup Enhancements — Design

**Date**: 2026-05-17
**Branch**: feat/task-popup-widget

## Overview

Eleven improvements to the task popup widget and the boards that open it. The centrepiece is extracting a shared `task_editor_form_widget` so the popup and the dedicated task-editor page share one implementation.

---

## 1. Architecture: Shared Form Component

### New file: `src/kanban/task_editor_form_widget.hpp/cpp`

A `WContainerWidget` containing the complete task-editing UI. Both the popup dialog and the full-page editor embed this widget.

**Constructor:**
```cpp
task_editor_form_widget(
    kanban_db&                              db,
    org_db&                                 odb,
    long long                               task_id,
    long long                               team_id,
    const session_data&                     session,
    team_cap::flags                         caps,
    const std::map<long long, std::string>& type_colors,
    std::function<void()>                   on_saved,   // after save or archive
    std::function<void()>                   on_cancel   // cancel button clicked
);
```

**Public interface:**
```cpp
bool is_dirty() const;
bool is_stale() const;
```

**Contents (top to bottom):**

1. Stale banner (hidden until a live-hub broadcast marks the task stale)
2. `WTabWidget` with two tabs:
   - **Details** — all inplace-edit fields (title, description, status, assignee, dates, type chips) plus the comments section
   - **History** — lazy-loaded on first tab switch; same rendering logic as `kanban_task_editor_page::rebuild_history()`
3. Button row:
   - **Save Changes** — disabled while `m_dirty_fields` is empty or `m_stale` is true
   - **Archive / Unarchive** — visible only when caps include `archive_task`
   - **Cancel** — rendered only when `on_cancel` is non-null; calls `on_cancel()`

### Modified: `task_popup_widget` (stays a `WDialog`)

Drops all field/history/comment logic and becomes a thin wrapper (~60 lines):

- Constructs `task_editor_form_widget` inside `contents()`
  - `on_saved`  = `[this]{ accept(); }`
  - `on_cancel` = `[this]{ try_close(); }`
- Title bar: task title (set via `setWindowTitle`) + "Open full editor ↗" anchor
- Footer: single **Close** button wired to `try_close()`
- After `show()`: fires JS (via hidden-input callback, same pattern as `kanban_board_widget`) to wire Escape-key and click-outside to `try_close()`

`try_close()`:
```
if is_dirty():
    show WDialog "Discard unsaved changes?"
        Discard → reject() outer popup
        Keep editing → dismiss confirm dialog only
else:
    reject()
```

### Modified: `kanban_task_editor_page` (thin page wrapper)

Drops its ~800 lines of field/history/comment logic and becomes ~50 lines:

- `<h1>Edit Task</h1>` header
- Constructs `task_editor_form_widget`
  - `on_saved`  = `m_on_save` (existing callback, navigates back to board)
  - `on_cancel` = navigate to `/board/{team_id}` (same as existing Cancel anchor)

---

## 2. Popup Scrolling (item 1)

**Problem:** Tall popups overflow the viewport without a scrollbar; background cannot be scrolled when popup is open.

**Fix:** Add to `_kanban.scss`:
```scss
.kb-task-popup {
  .Wt-dialog-contents {
    max-height: calc(90vh - 120px);  // 120px ≈ title bar + footer
    overflow-y: auto;
  }
}
```
The modal overlay already blocks background scrolling.

---

## 3. Revert Field on Focus-Loss with No Change (item 2)

**Problem:** Clicking an inplace-edit field and then clicking away (without changing the value) leaves the field in edit mode.

**Fix in `task_editor_form_widget`:**

- `enter_edit_mode()` stores the pre-edit display string in a per-field variable (e.g., `m_title_entry_val`).
- Each field has two blur-handling paths:
  - **Changed:** call `exit_edit_mode()` + `mark_field_dirty()` (existing behaviour for text/textarea; extended to cover dates)
  - **Unchanged:** call `exit_edit_mode()` + `unmark_field_dirty()` (new path)
- For `WComboBox` (status, assignee): add `blurred()` handler guarded by `if (edit->isHidden()) return` (because `changed()` may have already called `exit_edit_mode()`). The guard prevents double-transition when an option is selected.
- `unmark_field_dirty(field, container)`:
  - Removes `field` from `m_dirty_fields`
  - Removes `kb-popup-field--dirty` CSS class from `container`
  - Calls `m_save_btn->setEnabled(!m_dirty_fields.empty() && !m_stale)`

---

## 4. Description: Markdown Display + "Description" Header (item 3)

**Problem:** Description is shown as plain text in display mode, and has no visible section header.

**Fix:**

- Add a `<h2>Description</h2>` heading (using `kb-editor-form h2` style) above the description field, separate from the small field-label.
- The display `WText` for description uses `TextFormat::UnsafeXHTML` with `cmark_markdown_to_html()` output (same pattern as comment bodies). When the description is empty, display `"(none)"` as plain text.
- The `WTextArea` edit widget is unchanged.

---

## 5. Title Label Inline with Value (item 4)

**Problem:** The "Title" label and title value appear on separate lines.

**Fix:** Change the title field container to a horizontal flex layout:
```scss
.kb-popup-title-field {
  display:     flex;
  align-items: baseline;
  gap:         0.5rem;

  .kb-field-label { flex-shrink: 0; }
  .kb-popup-display,
  .editor-field   { flex: 1; }
}
```
In edit mode the `WLineEdit` occupies the same `flex: 1` slot as the display text, so the label stays on the same line.

---

## 6. Save Changes Disabled When No Dirty Fields (item 5)

**Problem:** Save button state does not reflect the actual dirty state (e.g., after reverting a change).

**Fix:** Already partially addressed by item 2. Additionally:

- Type chip selection: after setting `m_type_id`, compare to `m_original.type_id`. If equal, call `unmark_field_dirty("type", nullptr)` instead of `mark_field_dirty`.
- Save button starts disabled. It enables only when `!m_dirty_fields.empty() && !m_stale`. `unmark_field_dirty` re-evaluates this condition.

---

## 7. Confirm Close with Pending Changes (item 6)

**Problem:** Clicking Close/Cancel discards changes silently.

**Fix:** Route all close actions through `task_popup_widget::try_close()` (described in §1). The form's Cancel button calls `on_cancel` which is wired to `try_close()` in the popup context.

---

## 8. Escape and Click-Outside Act as Cancel (item 7)

**Problem:** Pressing Escape or clicking outside the popup does nothing.

**Fix:** After `show()`, `task_popup_widget` fires:
```js
(function(cbId) {
  function fireClose() {
    var inp = document.getElementById(cbId);
    if (!inp) return;
    inp.value = 'CLOSE';
    inp.dispatchEvent(new Event('change'));
  }
  // Escape key — one-shot so it doesn't accumulate across re-renders
  document.addEventListener('keydown', function onEsc(e) {
    if (e.key === 'Escape') { document.removeEventListener('keydown', onEsc); fireClose(); }
  });
  // Click outside (modal cover)
  var cover = document.querySelector('.Wt-dialogcover');
  if (cover) cover.addEventListener('click', fireClose, { once: true });
})(cbId);
```
The hidden-input `changed()` handler in `task_popup_widget` calls `try_close()`.

---

## 9. History Tab + Archive Button in Popup (item 8)

Covered by the shared form component (§1). The form's `WTabWidget` adds History. The Archive/Unarchive button appears in the button row per caps. Lazy-loading of history is identical to the existing `rebuild_history()` in `kanban_task_editor_page`.

---

## 10. Task Editor Page as Shared Widget (item 9)

Covered by §1. `kanban_task_editor_page` is reduced to a thin wrapper. The "Open full editor ↗" link in the popup title bar is retained.

---

## 11. Kanban: Click Card Opens Popup; Remove Edit Button (item 10)

**Changes to `resources/js/kanban.js`:**

- Remove the `if (canEdit)` block that creates `.kb-card-edit`
- Remove the corresponding `.kb-card-edit` styles from `_kanban.scss`
- Add to `.kb-card`: `cursor: pointer`
- In `makeCard()`, add to all cards (not just canEdit):
  ```js
  var _dragged = false;
  card.addEventListener('dragstart', function() { _dragged = true; });
  card.addEventListener('dragend',   function() { setTimeout(function() { _dragged = false; }, 0); });
  card.addEventListener('click',     function() { if (!_dragged) triggerCallback(cbId, 'EDIT:' + task.id); });
  ```
  Drag-start flag prevents click events that fire immediately after a drag-and-drop.

---

## 12. Gantt: Click Label Opens Popup; Bar No Longer Clickable (item 11)

**Changes to `resources/js/gantt.js`:**

- Remove `bar.style.cursor = 'pointer'` and the `bar.addEventListener('click', ...)` block.
- After drawing the task label text, append a transparent `<rect>` over the full label-column area for that row:
  ```js
  var hitRect = svgRect(0, ry, LABEL_W, ROW_H, 'fill:transparent;cursor:pointer');
  (function(taskId) {
    hitRect.addEventListener('click', function() {
      var inp = document.getElementById(cbId);
      if (inp) { inp.value = 'EDIT:' + taskId; inp.dispatchEvent(new Event('change')); }
    });
  }(task.id));
  svg.appendChild(hitRect);
  ```
  Using a full-row rect (not just the text glyph bounding box) gives a comfortable click target.

---

## 13. Testing

### Updated E2E tests (`e2e/specs/task-popup.spec.ts`)
- Replace `.locator('.kb-card-edit').click()` with `.locator('.kb-card').click()` in all existing tests that open the popup via the Kanban board.

### New E2E tests
| Test | Description |
|------|-------------|
| Revert on no-change blur | Click a display field, do not change value, tab away → field reverts to display mode, save button stays disabled |
| Dirty-confirm on Close | Make a change, click Close → confirm dialog appears; clicking "Keep editing" dismisses the confirm; clicking "Discard" closes popup |
| Dirty-confirm on Escape | Make a change, press Escape → same confirm flow |
| Click-outside cancel | Make a change, click the modal overlay → same confirm flow |
| History tab | Open popup, switch to History tab → history entries visible |
| Archive in popup | Open popup, click Archive → popup closes, task disappears from active board |
| Description markdown | Task with `**bold**` description → display mode shows `<strong>bold</strong>` rendered |
| Gantt label click | Click label area in Gantt → popup opens |
| Gantt bar no-op | Click colored bar in Gantt → popup does not open |

---

## File Change Summary

| File | Change |
|------|--------|
| `src/kanban/task_editor_form_widget.hpp` | **New** |
| `src/kanban/task_editor_form_widget.cpp` | **New** (~600 lines) |
| `src/kanban/task_popup_widget.hpp` | Simplified (fewer members) |
| `src/kanban/task_popup_widget.cpp` | Simplified to thin wrapper + `try_close()` |
| `src/pages/kanban_task_editor_page.hpp` | Simplified |
| `src/pages/kanban_task_editor_page.cpp` | Simplified to thin wrapper |
| `resources/js/kanban.js` | Remove edit button; add card click |
| `resources/js/gantt.js` | Move click from bar to label hit-rect |
| `resources/scss/_kanban.scss` | Scrolling, inline title, `.kb-card` cursor, remove `.kb-card-edit` |
| `e2e/specs/task-popup.spec.ts` | Update + add tests |
