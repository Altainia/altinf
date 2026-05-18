# Task Popup Enhancements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement all 11 popup/board improvements from the design spec, centred on a new `task_editor_form_widget` shared by the popup dialog and the full-page editor.

**Architecture:** Extract a standalone `task_editor_form_widget` (WContainerWidget) that owns all field logic, tabs, dirty state, and buttons. `task_popup_widget` becomes a thin WDialog wrapper; `kanban_task_editor_page` becomes a thin page wrapper. JS changes make the Kanban card clickable (removing the separate Edit button) and move the Gantt click target from the bar to the label column.

**Tech Stack:** C++/Wt, cmark, SCSS (compiled via cmake+sass), Playwright E2E, Node.js JS unit tests.

---

## File Map

| Action | Path |
|--------|------|
| **Create** | `src/kanban/task_editor_form_widget.hpp` |
| **Create** | `src/kanban/task_editor_form_widget.cpp` |
| **Rewrite** | `src/kanban/task_popup_widget.hpp` |
| **Rewrite** | `src/kanban/task_popup_widget.cpp` |
| **Rewrite** | `src/pages/kanban_task_editor_page.hpp` |
| **Rewrite** | `src/pages/kanban_task_editor_page.cpp` |
| **Modify** | `src/pages/kanban_board_page.cpp` (remove `type_colors` from popup call) |
| **Modify** | `resources/js/gantt.js` |
| **Modify** | `resources/js/kanban.js` |
| **Modify** | `resources/scss/_kanban.scss` |
| **Modify** | `e2e/specs/task-popup.spec.ts` |
| **Modify** | `e2e/specs/board.spec.ts` |
| **Modify** | `e2e/specs/task-history.spec.ts` |
| **Modify** | `e2e/specs/live-task-editor.spec.ts` |
| **Modify** | `e2e/specs/live-board.spec.ts` |
| **Modify** | `e2e/specs/comments.spec.ts` |
| **Modify** | `e2e/specs/task-permissions.spec.ts` |
| **Modify** | `tests/js/test_gantt.js` |

---

## Task 1: Gantt — move click from bar to label, update JS unit tests

**Files:**
- Modify: `resources/js/gantt.js`
- Modify: `tests/js/test_gantt.js`

- [ ] **Step 1: Update the existing bar-click unit test to assert the bar does NOT fire a callback, and add a new test asserting the label hit-rect does**

  In `tests/js/test_gantt.js`, find the test block that dispatches a click on `.gantt-bar` and expects `'EDIT:77'`. Replace it with two tests:

  ```js
  // ── Test: bar click no longer fires callback ──────────────────────────────
  test('gantt bar click does not trigger EDIT callback', function () {
    const dom = new JSDOM(
      '<!DOCTYPE html><body>' +
      '<input id="cb-test2" style="position:absolute;left:-9999px">' +
      '<div id="gv-mount2"></div>' +
      '</body>',
      { runScripts: 'dangerously' }
    );
    const { window } = dom;
    window.eval(GANTT_SRC);

    let fired = '';
    dom.window.document.getElementById('cb-test2')
      .addEventListener('change', function () { fired = this.value; });

    const today = new Date();
    today.setHours(0, 0, 0, 0);
    const tasks = [{
      id: 77, title: 'Bar Task', assigned_to: '', color: '#aabbcc',
      start_date: isoDate(addDays(today, -1)),
      end_date:   isoDate(addDays(today, 5))
    }];

    dom.window.initGantt('gv-mount2', tasks, 'cb-test2');

    const bar = dom.window.document.querySelector('#gv-mount2 .gantt-bar');
    assert(bar !== null, 'gantt-bar element must exist');
    bar.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
    assert(fired === '', 'bar click must not fire callback, got "' + fired + '"');
  });

  // ── Test: label hit-rect click fires EDIT callback ────────────────────────
  test('gantt label hit-rect click triggers EDIT callback', function () {
    const dom = new JSDOM(
      '<!DOCTYPE html><body>' +
      '<input id="cb-test3" style="position:absolute;left:-9999px">' +
      '<div id="gv-mount3"></div>' +
      '</body>',
      { runScripts: 'dangerously' }
    );
    const { window } = dom;
    window.eval(GANTT_SRC);

    let fired = '';
    dom.window.document.getElementById('cb-test3')
      .addEventListener('change', function () { fired = this.value; });

    const today = new Date();
    today.setHours(0, 0, 0, 0);
    const tasks = [{
      id: 88, title: 'Label Task', assigned_to: '', color: '#aabbcc',
      start_date: isoDate(addDays(today, -1)),
      end_date:   isoDate(addDays(today, 5))
    }];

    dom.window.initGantt('gv-mount3', tasks, 'cb-test3');

    // The label hit-rect is the <rect> with class 'gantt-label-hit'
    const hit = dom.window.document.querySelector('#gv-mount3 .gantt-label-hit');
    assert(hit !== null, 'gantt-label-hit rect must exist');
    hit.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
    assert(fired === 'EDIT:88', 'expected EDIT:88, got "' + fired + '"');
  });
  ```

- [ ] **Step 2: Run the JS unit tests to verify the bar-no-op test passes (bar still fires in old code) and the label test fails**

  ```bash
  cd tests/js && node test_gantt.js
  ```

  Expected: the new bar test FAILS (bar still fires `EDIT:77` in old code) and the label test FAILS (no hit-rect yet). The old bar test that expected `EDIT:77` should now be gone (replaced above).

- [ ] **Step 3: Update `resources/js/gantt.js` — remove bar click, add label hit-rect**

  Inside the `byAssignee[assignee].forEach` loop, find the block that sets up the bar click handler and the block that appends the bar. Replace both with:

  ```js
  // Remove these lines:
  //   bar.style.cursor = 'pointer';
  //   (function (taskId) {
  //     bar.addEventListener('click', function () { ... });
  //   }(task.id));

  // After `svg.appendChild(bar);`, add a transparent hit-rect over the label column:
  var hitRect = svgEl('rect');
  hitRect.setAttribute('x', 0);
  hitRect.setAttribute('y', ry);
  hitRect.setAttribute('width', LABEL_W);
  hitRect.setAttribute('height', ROW_H);
  hitRect.setAttribute('style', 'fill:transparent;cursor:pointer');
  hitRect.setAttribute('class', 'gantt-label-hit');
  (function (taskId) {
    hitRect.addEventListener('click', function () {
      var inp = document.getElementById(cbId);
      if (inp) {
        inp.value = 'EDIT:' + taskId;
        inp.dispatchEvent(new Event('change'));
      }
    });
  }(task.id));
  svg.appendChild(hitRect);
  ```

  The full diff: in the section starting at `var bar = svgRect(bx, ry + ROW_H * 0.2 ...)`:
  - Keep the `bar` rect and `svg.appendChild(bar)` unchanged but remove `bar.style.cursor` and the `bar.addEventListener` block.
  - After `svg.appendChild(bar)`, insert the `hitRect` block above.

- [ ] **Step 4: Run JS unit tests — all must pass**

  ```bash
  cd tests/js && node test_gantt.js
  ```

  Expected: `✓ N passed, 0 failed` (where N includes the two new tests).

- [ ] **Step 5: Commit**

  ```bash
  git add resources/js/gantt.js tests/js/test_gantt.js
  git commit -m "$(cat <<'EOF'
  feat: gantt label hit-rect opens popup; bar click removed

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 2: Kanban — remove Edit button, add card click, update all E2E specs

**Files:**
- Modify: `resources/js/kanban.js`
- Modify: `resources/scss/_kanban.scss`
- Modify: `e2e/specs/board.spec.ts`
- Modify: `e2e/specs/task-popup.spec.ts`
- Modify: `e2e/specs/task-history.spec.ts`
- Modify: `e2e/specs/live-task-editor.spec.ts`
- Modify: `e2e/specs/live-board.spec.ts`
- Modify: `e2e/specs/comments.spec.ts`
- Modify: `e2e/specs/task-permissions.spec.ts`

- [ ] **Step 1: Update `resources/js/kanban.js` — remove Edit button, add card click with drag guard**

  In `makeCard()`, remove the entire `if (canEdit) { var editBtn = ... }` block (lines that create `.kb-card-edit`).

  Add click and drag handlers to all cards (outside the `if (canEdit)` block, applies to everyone):

  ```js
  // Add after the existing dragend handler (or after the draggable setup block):
  var _dragged = false;
  if (canEdit) {
    card.addEventListener('dragstart', function () { _dragged = true; });
    card.addEventListener('dragend',   function () { setTimeout(function () { _dragged = false; }, 0); });
  }
  card.addEventListener('click', function () {
    if (!_dragged) { triggerCallback(cbId, 'EDIT:' + task.id); }
  });
  ```

  The card element should already exist at this point. The full `makeCard` function after changes:

  ```js
  function makeCard(task, canEdit, cbId) {
    var card = document.createElement('div');
    card.className = 'kb-card';
    card.dataset.id = task.id;

    if (task.color) {
      card.style.borderLeftColor = task.color;
    }

    var _dragged = false;
    if (canEdit) {
      card.setAttribute('draggable', 'true');
      card.addEventListener('dragstart', function (e) {
        _dragged = true;
        e.dataTransfer.setData('text/plain', String(task.id));
        e.dataTransfer.effectAllowed = 'move';
        card.classList.add('kb-card--dragging');
      });
      card.addEventListener('dragend', function () {
        card.classList.remove('kb-card--dragging');
        setTimeout(function () { _dragged = false; }, 0);
        document.querySelectorAll('.kb-column--over').forEach(function (c) {
          c.classList.remove('kb-column--over');
        });
      });
    }

    card.addEventListener('click', function () {
      if (!_dragged) { triggerCallback(cbId, 'EDIT:' + task.id); }
    });

    var title = document.createElement('div');
    title.className = 'kb-card-title';
    title.textContent = task.title;
    card.appendChild(title);

    if (task.assigned_to) {
      var assignee = document.createElement('div');
      assignee.className = 'kb-card-assignee';
      assignee.textContent = task.assigned_to;
      card.appendChild(assignee);
    }

    if (task.start_date || task.end_date) {
      var dates = document.createElement('div');
      dates.className = 'kb-card-dates';
      dates.textContent = (task.start_date || '?') + ' – ' + (task.end_date || '?');
      card.appendChild(dates);
    }

    return card;
  }
  ```

- [ ] **Step 2: Update `resources/scss/_kanban.scss` — remove `.kb-card-edit` block, add cursor to `.kb-card`**

  In the `.kb-card` rule, change `cursor: default` to `cursor: pointer`:
  ```scss
  .kb-card {
    // ...existing rules...
    cursor: pointer;   // was: cursor: default
    // ...
    &[draggable="true"] { cursor: grab; }
    // ...
  }
  ```

  Delete the entire `.kb-card-edit { ... }` rule block (about 15 lines starting with `.kb-card-edit {`).

- [ ] **Step 3: Update `e2e/specs/board.spec.ts`**

  a. Find and replace all `.locator('.kb-card-edit').click()` with `.click()` on the parent card locator. Concretely:
  - `page.locator('.kb-card', { hasText: '...' }).locator('.kb-card-edit').click()` → `page.locator('.kb-card', { hasText: '...' }).click()`
  - `card.locator('.kb-card-edit').click()` → `card.click()`

  b. Find the test `'created task card shows Edit button'` and replace it with a test that the card is clickable and opens the popup:
  ```ts
  test('clicking a task card opens the popup', async ({ page }) => {
    await loginAndGoToBoard(page);
    await createTask(page, 'Board Test Task Zeta');
    await page.locator('.kb-card', { hasText: 'Board Test Task Zeta' }).click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    await page.locator('.kb-task-popup .footer .editor-btn-cancel').click();
  });
  ```

  c. Find the test `'Edit button opens task popup with task title'` and rename it to `'clicking a card opens the task popup with its title'`, replacing the click:
  ```ts
  test('clicking a card opens the task popup with its title', async ({ page }) => {
    await loginAndGoToBoard(page);
    await createTask(page, 'Board Test Task Eta');
    await page.locator('.kb-card', { hasText: 'Board Test Task Eta' }).click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toContainText('Board Test Task Eta');
  });
  ```

- [ ] **Step 4: Update remaining E2E specs — mechanical `.kb-card-edit` → card click**

  In each file below, every occurrence of `.locator('.kb-card-edit').click()` becomes `.click()` on the parent card locator (drop the `.locator('.kb-card-edit')` entirely):

  **`e2e/specs/task-popup.spec.ts`** — 7 occurrences. Example:
  ```ts
  // Before:
  await page.locator('.kb-card', { hasText: 'PopupTask' }).locator('.kb-card-edit').click();
  // After:
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  ```

  **`e2e/specs/task-history.spec.ts`** — update `openTaskEditor` helper:
  ```ts
  async function openTaskEditor(page: Page, title: string) {
    await page.locator('.kb-card', { hasText: title }).first().click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    await page.locator('.kb-popup-full-link').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
  }
  ```

  **`e2e/specs/live-task-editor.spec.ts`** — update `openTaskEditorViaPath` helper:
  ```ts
  async function openTaskEditorViaPath(page: Page, orgName: string, title: string) {
    await page.locator('.kb-card', { hasText: title }).locator('.kb-card-edit').click();
    // ↑ replace .locator('.kb-card-edit').click() with .click()
  ```
  becomes:
  ```ts
  async function openTaskEditorViaPath(page: Page, orgName: string, title: string) {
    await page.locator('.kb-card', { hasText: title }).click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    await page.locator('.kb-popup-full-link').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
  }
  ```

  **`e2e/specs/live-board.spec.ts`** — update `openTaskEditor` helper (same pattern as task-history).

  **`e2e/specs/comments.spec.ts`** — update `openTaskEditor` helper:
  ```ts
  async function openTaskEditor(page: Page, title: string) {
    await page.locator('.kb-card', { hasText: title }).first().click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    await page.locator('.kb-popup-full-link').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
  }
  ```

  **`e2e/specs/task-permissions.spec.ts`** — 2 occurrences, same pattern.

- [ ] **Step 5: Build (SCSS compile + resource copy; no C++ changes)**

  ```bash
  cmake --build build --parallel $(nproc)
  ```

  Expected: compiles without errors (SCSS recompiled, JS copied).

- [ ] **Step 6: Run the full E2E suite**

  ```bash
  cd e2e && npx playwright test
  ```

  Expected: all tests pass. Any failures at this stage are regressions from this task.

- [ ] **Step 7: Commit**

  ```bash
  git add resources/js/kanban.js resources/scss/_kanban.scss \
    e2e/specs/board.spec.ts e2e/specs/task-popup.spec.ts \
    e2e/specs/task-history.spec.ts e2e/specs/live-task-editor.spec.ts \
    e2e/specs/live-board.spec.ts e2e/specs/comments.spec.ts \
    e2e/specs/task-permissions.spec.ts
  git commit -m "$(cat <<'EOF'
  feat: kanban card click opens popup; remove per-card Edit button

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 3: Write failing E2E tests for new popup behaviors

**Files:**
- Modify: `e2e/specs/task-popup.spec.ts`

Add the following tests at the end of `task-popup.spec.ts`. They will fail until the C++ shared form widget is implemented (Tasks 4–8).

- [ ] **Step 1: Add failing tests**

  ```ts
  test('blur with no change reverts field to display mode', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl);
    await expect(page.locator('.kb-board')).toBeVisible();
    await page.locator('.kb-card').first().click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    // Click display to enter edit mode
    const display = page.locator('.kb-task-popup .kb-popup-display').first();
    await display.click();
    const input = page.locator('.kb-task-popup input[type="text"]').first();
    await expect(input).toBeVisible();
    // Tab away without changing value
    await input.press('Tab');
    // Display should reappear, save button must remain disabled
    await expect(display).toBeVisible();
    await expect(input).toBeHidden();
    const saveBtn = page.locator('.kb-task-popup .editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)');
    await expect(saveBtn).toBeDisabled();
  });

  test('close with pending changes shows confirmation dialog', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl);
    await expect(page.locator('.kb-board')).toBeVisible();
    await page.locator('.kb-card').first().click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    // Make a change
    await page.locator('.kb-task-popup .kb-popup-display').first().click();
    await page.locator('.kb-task-popup input[type="text"]').first().fill('DirtyTitle');
    await page.locator('.kb-task-popup input[type="text"]').first().press('Tab');
    // Click Close button
    await page.locator('.kb-task-popup .footer .editor-btn-cancel').click();
    // Confirmation dialog appears
    await expect(page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })).toBeVisible();
    // Click "Keep Editing" — popup stays open
    await page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })
      .locator('button', { hasText: 'Keep Editing' }).click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    // Click Close again, then Discard
    await page.locator('.kb-task-popup .footer .editor-btn-cancel').click();
    await page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })
      .locator('button', { hasText: 'Discard' }).click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
  });

  test('escape key with no changes closes popup immediately', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl);
    await expect(page.locator('.kb-board')).toBeVisible();
    await page.locator('.kb-card').first().click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    await page.keyboard.press('Escape');
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
  });

  test('escape key with pending changes shows confirmation', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl);
    await expect(page.locator('.kb-board')).toBeVisible();
    await page.locator('.kb-card').first().click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    await page.locator('.kb-task-popup .kb-popup-display').first().click();
    await page.locator('.kb-task-popup input[type="text"]').first().fill('EscDirty');
    await page.locator('.kb-task-popup input[type="text"]').first().press('Tab');
    await page.keyboard.press('Escape');
    await expect(page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })).toBeVisible();
  });

  test('popup has History tab that shows task history', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl);
    await expect(page.locator('.kb-board')).toBeVisible();
    await page.locator('.kb-card').first().click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    // History tab must exist
    const historyTab = page.locator('.kb-task-popup .Wt-tabs li', { hasText: 'History' });
    await expect(historyTab).toBeVisible();
    await historyTab.click();
    // At minimum the history panel should be present (task was created, so there is history)
    await expect(page.locator('.kb-task-popup .kb-history-entry')).toBeVisible();
  });

  test('popup has Archive button that archives the task', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl);
    await expect(page.locator('.kb-board')).toBeVisible();
    // Create a fresh task for this test
    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
    await page.locator('input[placeholder="Task title"]').fill('ArchiveViaPopup');
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible();
    // Open popup
    await page.locator('.kb-card', { hasText: 'ArchiveViaPopup' }).click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    // Click Archive
    await page.locator('.kb-task-popup .editor-btn-danger', { hasText: 'Archive' }).click();
    // Popup closes, task gone from board
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
    await expect(page.locator('.kb-card', { hasText: 'ArchiveViaPopup' })).not.toBeVisible();
  });

  test('description renders markdown in display mode', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl);
    await expect(page.locator('.kb-board')).toBeVisible();
    // Create task with markdown description via full editor
    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
    await page.locator('input[placeholder="Task title"]').fill('MarkdownDesc');
    await page.locator('textarea[placeholder="Description (optional)"]').fill('**bold text**');
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible();
    // Open popup — description display should contain <strong>
    await page.locator('.kb-card', { hasText: 'MarkdownDesc' }).click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
    const descDisplay = page.locator('.kb-task-popup .kb-desc-display');
    await expect(descDisplay).toBeVisible();
    await expect(descDisplay.locator('strong')).toBeVisible();
  });

  test('gantt label area click opens popup', async ({ page }) => {
    await loginAs(page, 'admin', 'testpass');
    await page.goto(teamUrl + '/gantt');
    await expect(page.locator('.gv-wrap')).toBeVisible();
    const hit = page.locator('.gantt-label-hit').first();
    // Only test if at least one task has dates and appears in the gantt
    const count = await hit.count();
    if (count === 0) { return; } // no dated tasks — skip
    await hit.click();
    await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  });
  ```

- [ ] **Step 2: Run the E2E suite and verify the new tests fail as expected**

  ```bash
  cd e2e && npx playwright test task-popup
  ```

  Expected: the new tests fail (behaviors not yet implemented). Existing tests continue to pass.

- [ ] **Step 3: Commit the failing tests**

  ```bash
  git add e2e/specs/task-popup.spec.ts
  git commit -m "$(cat <<'EOF'
  test(e2e): add failing tests for new popup behaviors (TDD)

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 4: Create `task_editor_form_widget.hpp`

**Files:**
- Create: `src/kanban/task_editor_form_widget.hpp`

- [ ] **Step 1: Create the header**

  ```cpp
  #pragma once

  #include "kanban.hpp"
  #include "kanban_db.hpp"
  #include "team_cap.hpp"
  #include "auth/session_data.hpp"
  #include "org/org_db.hpp"

  #include <Wt/WComboBox.h>
  #include <Wt/WContainerWidget.h>
  #include <Wt/WDateEdit.h>
  #include <Wt/WLineEdit.h>
  #include <Wt/WPushButton.h>
  #include <Wt/WText.h>
  #include <Wt/WTextArea.h>

  #include <functional>
  #include <map>
  #include <memory>
  #include <set>
  #include <string>
  #include <vector>

  class task_editor_form_widget : public Wt::WContainerWidget
  {
  public:
      // task_id == 0 → new-task creation mode.
      task_editor_form_widget(kanban_db&            db,
                              org_db&               odb,
                              long long             task_id,
                              long long             team_id,
                              const session_data&   session,
                              team_cap::flags       caps,
                              std::function<void()> on_saved,
                              std::function<void()> on_cancel);

      ~task_editor_form_widget() override;

      bool is_dirty() const;
      bool is_stale() const;

  private:
      kanban_db&            m_db;
      org_db&               m_odb;
      long long             m_task_id{0};
      long long             m_team_id{0};
      long long             m_org_id{0};
      std::string           m_username;
      team_cap::flags       m_caps;
      std::string           m_session_id;
      std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
      std::function<void()> m_on_saved;
      std::function<void()> m_on_cancel;

      kanban_task_entry m_original;

      Wt::WLineEdit*        m_title_edit{nullptr};
      Wt::WText*            m_title_display{nullptr};
      Wt::WContainerWidget* m_title_field{nullptr};

      Wt::WTextArea*        m_desc_edit{nullptr};
      Wt::WText*            m_desc_display{nullptr};
      Wt::WContainerWidget* m_desc_field{nullptr};

      Wt::WComboBox*        m_status_edit{nullptr};
      Wt::WText*            m_status_display{nullptr};
      Wt::WContainerWidget* m_status_field{nullptr};

      Wt::WComboBox*        m_assignee_edit{nullptr};
      Wt::WText*            m_assignee_display{nullptr};
      Wt::WContainerWidget* m_assignee_field{nullptr};

      Wt::WDateEdit*        m_start_date_edit{nullptr};
      Wt::WText*            m_start_date_display{nullptr};
      Wt::WContainerWidget* m_start_field{nullptr};

      Wt::WDateEdit*        m_end_date_edit{nullptr};
      Wt::WText*            m_end_date_display{nullptr};
      Wt::WContainerWidget* m_end_field{nullptr};

      long long                          m_type_id{0};
      std::vector<Wt::WContainerWidget*> m_type_chips;
      std::vector<std::string>           m_assignee_values;

      Wt::WContainerWidget* m_stale_banner{nullptr};
      bool                  m_stale{false};
      std::set<std::string> m_dirty_fields;

      Wt::WPushButton*      m_save_btn{nullptr};
      Wt::WContainerWidget* m_comment_list{nullptr};
      Wt::WContainerWidget* m_comment_compose{nullptr};
      Wt::WContainerWidget* m_history_panel{nullptr};

      static const std::vector<std::string> k_status_vals;
      static const std::vector<std::string> k_status_labels;

      void save();
      void mark_stale();
      void mark_field_dirty(const std::string& field, Wt::WContainerWidget* container);
      void unmark_field_dirty(const std::string& field, Wt::WContainerWidget* container);
      void enter_edit_mode(Wt::WText* display, Wt::WWidget* edit);
      void exit_edit_mode(Wt::WText* display, Wt::WWidget* edit, const std::string& new_text);
      void rebuild_comments();
      void rebuild_history();
      std::string render_markdown(const std::string& md) const;
  };
  ```

- [ ] **Step 2: Verify the header compiles (the .cpp will be empty for now — add a stub)**

  Create a minimal stub `src/kanban/task_editor_form_widget.cpp`:
  ```cpp
  #include "task_editor_form_widget.hpp"

  const std::vector<std::string> task_editor_form_widget::k_status_vals   = {"todo","in_progress","review","done"};
  const std::vector<std::string> task_editor_form_widget::k_status_labels = {"To Do","In Progress","Review","Done"};

  task_editor_form_widget::task_editor_form_widget(
    kanban_db& db, org_db& odb, long long task_id, long long team_id,
    const session_data& session, team_cap::flags caps,
    std::function<void()> on_saved, std::function<void()> on_cancel)
  : m_db{db}, m_odb{odb}, m_task_id{task_id}, m_team_id{team_id},
    m_username{session.username}, m_caps{caps},
    m_on_saved{std::move(on_saved)}, m_on_cancel{std::move(on_cancel)}
  {}

  task_editor_form_widget::~task_editor_form_widget() { *m_alive = false; }
  bool task_editor_form_widget::is_dirty() const { return !m_dirty_fields.empty(); }
  bool task_editor_form_widget::is_stale() const { return m_stale; }
  void task_editor_form_widget::save() {}
  void task_editor_form_widget::mark_stale() {}
  void task_editor_form_widget::mark_field_dirty(const std::string&, Wt::WContainerWidget*) {}
  void task_editor_form_widget::unmark_field_dirty(const std::string&, Wt::WContainerWidget*) {}
  void task_editor_form_widget::enter_edit_mode(Wt::WText*, Wt::WWidget*) {}
  void task_editor_form_widget::exit_edit_mode(Wt::WText*, Wt::WWidget*, const std::string&) {}
  void task_editor_form_widget::rebuild_comments() {}
  void task_editor_form_widget::rebuild_history() {}
  std::string task_editor_form_widget::render_markdown(const std::string& md) const { return md; }
  ```

  Add the new files to `CMakeLists.txt`. Find the `add_executable(altinf ...)` block and append:
  ```cmake
  src/kanban/task_editor_form_widget.cpp
  ```

  Build:
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --parallel $(nproc)
  ```

  Expected: compiles without errors.

- [ ] **Step 3: Commit the stub**

  ```bash
  git add src/kanban/task_editor_form_widget.hpp src/kanban/task_editor_form_widget.cpp CMakeLists.txt
  git commit -m "$(cat <<'EOF'
  feat: add task_editor_form_widget stub (header + empty impl)

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 5: Implement `task_editor_form_widget.cpp`

**Files:**
- Modify: `src/kanban/task_editor_form_widget.cpp`

Replace the stub with the full implementation. The complete file:

- [ ] **Step 1: Write the full implementation**

  ```cpp
  #include "task_editor_form_widget.hpp"

  #include <Wt/Dbo/Exception.h>
  #include <Wt/WAnchor.h>
  #include <Wt/WApplication.h>
  #include <Wt/WColor.h>
  #include <Wt/WDate.h>
  #include <Wt/WDialog.h>
  #include <Wt/WLink.h>
  #include <Wt/WTabWidget.h>
  #include <Wt/WText.h>
  #include <cmark.h>

  #include <algorithm>
  #include <cstdlib>
  #include <map>

  #include "org/org.hpp"
  #include "widgets/live_hub.hpp"

  // ── Static helpers ────────────────────────────────────────────────────────────

  static std::string fmt_ts(const std::string& iso)
  {
      if(iso.size() < 16) return iso;
      try {
          int yr = std::stoi(iso.substr(0, 4));
          int mo = std::stoi(iso.substr(5, 2));
          int dy = std::stoi(iso.substr(8, 2));
          static const char* months[] = {"","Jan","Feb","Mar","Apr","May","Jun",
                                          "Jul","Aug","Sep","Oct","Nov","Dec"};
          return std::string(months[mo]) + " " + std::to_string(dy) +
                 ", " + std::to_string(yr) +
                 " at " + iso.substr(11,2) + ":" + iso.substr(14,2);
      } catch(...) { return iso; }
  }

  static std::string date_disp(const Wt::WDate& d)
  {
      return d.isValid() ? d.toString("yyyy-MM-dd").toUTF8() : "(not set)";
  }

  static std::string status_lbl(const std::string& v)
  {
      static const std::map<std::string,std::string> m = {
          {"todo","To Do"},{"in_progress","In Progress"},{"review","Review"},{"done","Done"}};
      auto it = m.find(v);
      return it != m.end() ? it->second : v;
  }

  const std::vector<std::string> task_editor_form_widget::k_status_vals   =
      {"todo","in_progress","review","done"};
  const std::vector<std::string> task_editor_form_widget::k_status_labels =
      {"To Do","In Progress","Review","Done"};

  // ── Constructor ───────────────────────────────────────────────────────────────

  task_editor_form_widget::task_editor_form_widget(
      kanban_db& db, org_db& odb, long long task_id, long long team_id,
      const session_data& session, team_cap::flags caps,
      std::function<void()> on_saved, std::function<void()> on_cancel)
    : m_db{db}, m_odb{odb}, m_task_id{task_id}, m_team_id{team_id},
      m_username{session.username}, m_caps{caps},
      m_on_saved{std::move(on_saved)}, m_on_cancel{std::move(on_cancel)}
  {
      const bool is_new = (task_id == 0);

      if(!is_new)
      {
          const auto opt = m_db.find_task(task_id);
          if(!opt)
          {
              addNew<Wt::WText>("Task not found.", Wt::TextFormat::Plain);
              return;
          }
          m_original = *opt;
      }

      const auto team_opt = m_db.find_team(team_id);
      m_org_id = team_opt ? team_opt->org_id : 0;

      const bool can_edit   = caps.has_any(team_cap::edit_task_fields) &&
                              (is_new || !m_original.is_archived);
      const bool can_assign = caps.has_any(team_cap::reassign_task) &&
                              (is_new || !m_original.is_archived);
      const bool can_use_assignee =
          can_assign ||
          (!is_new && !m_original.assigned_to.empty() &&
           m_original.assigned_to == session.username);

      // ── Stale banner ─────────────────────────────────────────────────────────
      if(!is_new)
      {
          m_stale_banner = addNew<Wt::WContainerWidget>();
          m_stale_banner->setStyleClass("kb-popup-stale-banner");
          m_stale_banner->addNew<Wt::WText>(
              "This task was updated by another user.", Wt::TextFormat::Plain);
          auto* reload_btn = m_stale_banner->addNew<Wt::WPushButton>("Reload");
          reload_btn->setStyleClass("editor-btn");
          reload_btn->clicked().connect([this]{ if(m_on_cancel) m_on_cancel(); });
          m_stale_banner->hide();
      }

      // ── Tab widget (existing tasks) or plain container (new tasks) ────────────
      Wt::WContainerWidget* form = nullptr;
      if(!is_new)
      {
          auto* tabs = addNew<Wt::WTabWidget>();
          tabs->setStyleClass("kb-editor-tabs");
          auto* details = new Wt::WContainerWidget();
          details->setStyleClass("kb-editor-form");
          tabs->addTab(std::unique_ptr<Wt::WContainerWidget>(details), "Details");
          form = details;
          m_history_panel = new Wt::WContainerWidget();
          tabs->addTab(std::unique_ptr<Wt::WContainerWidget>(m_history_panel), "History");
          tabs->currentChanged().connect([this](int idx){ if(idx==1) rebuild_history(); });
      }
      else
      {
          setStyleClass("kb-editor-form");
          form = this;
      }

      // ── Title (inline label + value) ──────────────────────────────────────────
      m_title_field = form->addNew<Wt::WContainerWidget>();
      m_title_field->setStyleClass("kb-popup-field kb-popup-title-field");
      m_title_field->addNew<Wt::WText>("Title", Wt::TextFormat::Plain)
          ->setStyleClass("kb-field-label");

      const std::string title_val = is_new ? "" : m_original.title;
      m_title_display = m_title_field->addNew<Wt::WText>(title_val, Wt::TextFormat::Plain);
      m_title_display->setStyleClass(can_edit && !is_new ? "kb-popup-display" : "");
      if(is_new) m_title_display->hide();

      m_title_edit = m_title_field->addNew<Wt::WLineEdit>(title_val);
      m_title_edit->setPlaceholderText("Task title");
      m_title_edit->setStyleClass("editor-field");
      if(!is_new) m_title_edit->hide();

      if(can_edit && !is_new)
      {
          m_title_display->clicked().connect([this]{
              enter_edit_mode(m_title_display, m_title_edit);
          });
          m_title_edit->blurred().connect([this]{
              const std::string v  = m_title_edit->text().toUTF8();
              const std::string dv = v.empty() ? m_original.title : v;
              if(v.empty()) m_title_edit->setText(m_original.title);
              exit_edit_mode(m_title_display, m_title_edit, dv);
              if(dv == m_original.title) unmark_field_dirty("title", m_title_field);
              else                       mark_field_dirty  ("title", m_title_field);
          });
      }

      // ── Description ───────────────────────────────────────────────────────────
      form->addNew<Wt::WText>("<h2>Description</h2>", Wt::TextFormat::UnsafeXHTML);
      m_desc_field = form->addNew<Wt::WContainerWidget>();
      m_desc_field->setStyleClass("kb-popup-field");

      const std::string desc_val  = is_new ? "" : m_original.description;
      const std::string desc_html = render_markdown(desc_val);
      m_desc_display = m_desc_field->addNew<Wt::WText>(desc_html, Wt::TextFormat::UnsafeXHTML);
      m_desc_display->setStyleClass("kb-desc-display");
      if(can_edit && !is_new) m_desc_display->addStyleClass("kb-popup-display");
      if(is_new) m_desc_display->hide();

      m_desc_edit = m_desc_field->addNew<Wt::WTextArea>(desc_val);
      m_desc_edit->setPlaceholderText("Description (optional)");
      m_desc_edit->setStyleClass("editor-field kb-desc-field");
      if(!is_new) m_desc_edit->hide();

      if(can_edit && !is_new)
      {
          m_desc_display->clicked().connect([this]{
              enter_edit_mode(m_desc_display, m_desc_edit);
          });
          m_desc_edit->blurred().connect([this]{
              const std::string v = m_desc_edit->text().toUTF8();
              m_desc_edit->hide();
              m_desc_display->setText(render_markdown(v));
              m_desc_display->show();
              if(v == m_original.description) unmark_field_dirty("description", m_desc_field);
              else                            mark_field_dirty  ("description", m_desc_field);
          });
      }

      // ── Status + Assignee row ─────────────────────────────────────────────────
      auto* row = form->addNew<Wt::WContainerWidget>();
      row->setStyleClass("kb-editor-row");

      // Status
      m_status_field = row->addNew<Wt::WContainerWidget>();
      m_status_field->setStyleClass("kb-editor-field-wrap kb-popup-field");
      m_status_field->addNew<Wt::WText>("Status", Wt::TextFormat::Plain)
          ->setStyleClass("kb-field-label");
      const std::string status_init = is_new ? "todo" : m_original.status;
      m_status_display = m_status_field->addNew<Wt::WText>(
          status_lbl(status_init), Wt::TextFormat::Plain);
      m_status_display->setStyleClass(can_edit && !is_new ? "kb-popup-display" : "");
      if(is_new) m_status_display->hide();
      m_status_edit = m_status_field->addNew<Wt::WComboBox>();
      m_status_edit->setStyleClass("editor-field");
      for(const auto& lbl : k_status_labels) m_status_edit->addItem(lbl);
      {
          const auto it = std::find(k_status_vals.begin(), k_status_vals.end(), status_init);
          if(it != k_status_vals.end())
              m_status_edit->setCurrentIndex(
                  static_cast<int>(std::distance(k_status_vals.begin(), it)));
      }
      if(!is_new) m_status_edit->hide();

      if(can_edit && !is_new)
      {
          m_status_display->clicked().connect([this]{
              enter_edit_mode(m_status_display, m_status_edit);
          });
          m_status_edit->changed().connect([this]{
              const int si = m_status_edit->currentIndex();
              const std::string v = (si >= 0 && si < static_cast<int>(k_status_vals.size()))
                                  ? k_status_vals[si] : m_original.status;
              exit_edit_mode(m_status_display, m_status_edit, status_lbl(v));
              if(v == m_original.status) unmark_field_dirty("status", m_status_field);
              else                       mark_field_dirty  ("status", m_status_field);
          });
          m_status_edit->blurred().connect([this]{
              if(m_status_edit->isHidden()) return;
              const int si = m_status_edit->currentIndex();
              const std::string v = (si >= 0 && si < static_cast<int>(k_status_vals.size()))
                                  ? k_status_vals[si] : m_original.status;
              exit_edit_mode(m_status_display, m_status_edit, status_lbl(v));
              if(v == m_original.status) unmark_field_dirty("status", m_status_field);
              else                       mark_field_dirty  ("status", m_status_field);
          });
      }

      // Assignee
      m_assignee_field = row->addNew<Wt::WContainerWidget>();
      m_assignee_field->setStyleClass("kb-editor-field-wrap kb-popup-field");
      m_assignee_field->addNew<Wt::WText>("Assigned to", Wt::TextFormat::Plain)
          ->setStyleClass("kb-field-label");
      const std::string assignee_init = is_new ? "" : m_original.assigned_to;
      m_assignee_display = m_assignee_field->addNew<Wt::WText>(
          assignee_init.empty() ? "(unassigned)" : assignee_init, Wt::TextFormat::Plain);
      m_assignee_display->setStyleClass(can_use_assignee && !is_new ? "kb-popup-display" : "");
      if(is_new) m_assignee_display->hide();

      m_assignee_values.push_back("");
      m_assignee_edit = m_assignee_field->addNew<Wt::WComboBox>();
      m_assignee_edit->setStyleClass("editor-field");
      m_assignee_edit->addItem("(unassigned)");
      const auto members = m_db.members_for_team(team_id);
      if(can_assign)
      {
          for(const auto& mem : members)
          {
              m_assignee_values.push_back(mem);
              m_assignee_edit->addItem(mem);
          }
      }
      else
      {
          m_assignee_values.push_back(session.username);
          m_assignee_edit->addItem(session.username);
      }
      {
          const auto it = std::find(
              m_assignee_values.begin(), m_assignee_values.end(), assignee_init);
          if(it != m_assignee_values.end())
              m_assignee_edit->setCurrentIndex(
                  static_cast<int>(std::distance(m_assignee_values.begin(), it)));
      }
      if(!is_new) m_assignee_edit->hide();

      if(can_use_assignee && !is_new)
      {
          m_assignee_display->clicked().connect([this]{
              enter_edit_mode(m_assignee_display, m_assignee_edit);
          });
          m_assignee_edit->changed().connect([this]{
              const int ai = m_assignee_edit->currentIndex();
              const std::string v = (ai >= 0 && ai < static_cast<int>(m_assignee_values.size()))
                                  ? m_assignee_values[ai] : "";
              exit_edit_mode(m_assignee_display, m_assignee_edit,
                             v.empty() ? "(unassigned)" : v);
              if(v == m_original.assigned_to) unmark_field_dirty("assigned_to", m_assignee_field);
              else                            mark_field_dirty  ("assigned_to", m_assignee_field);
          });
          m_assignee_edit->blurred().connect([this]{
              if(m_assignee_edit->isHidden()) return;
              const int ai = m_assignee_edit->currentIndex();
              const std::string v = (ai >= 0 && ai < static_cast<int>(m_assignee_values.size()))
                                  ? m_assignee_values[ai] : "";
              exit_edit_mode(m_assignee_display, m_assignee_edit,
                             v.empty() ? "(unassigned)" : v);
              if(v == m_original.assigned_to) unmark_field_dirty("assigned_to", m_assignee_field);
              else                            mark_field_dirty  ("assigned_to", m_assignee_field);
          });
      }

      // ── Date fields ───────────────────────────────────────────────────────────
      auto* sched_row = form->addNew<Wt::WContainerWidget>();
      sched_row->setStyleClass("kb-editor-row");

      auto build_date = [&](Wt::WContainerWidget*  parent,
                            const std::string&     label,
                            const Wt::WDate&       orig,
                            const std::string&     field_name,
                            Wt::WText*&            disp,
                            Wt::WDateEdit*&        edit,
                            Wt::WContainerWidget*& wrap)
      {
          wrap = parent->addNew<Wt::WContainerWidget>();
          wrap->setStyleClass("kb-editor-field-wrap kb-popup-field");
          wrap->addNew<Wt::WText>(label, Wt::TextFormat::Plain)->setStyleClass("kb-field-label");
          disp = wrap->addNew<Wt::WText>(date_disp(orig), Wt::TextFormat::Plain);
          disp->setStyleClass(can_edit && !is_new ? "kb-popup-display" : "");
          if(is_new) disp->hide();
          edit = wrap->addNew<Wt::WDateEdit>();
          edit->setFormat("yyyy-MM-dd");
          edit->setStyleClass("editor-field");
          if(orig.isValid()) edit->setDate(orig);
          if(!is_new) edit->hide();
          if(can_edit && !is_new)
          {
              disp->clicked().connect([disp, edit, this]{ enter_edit_mode(disp, edit); });
              edit->blurred().connect([this, orig, field_name, disp, edit, wrap]{
                  const auto d = edit->date();
                  exit_edit_mode(disp, edit, date_disp(d));
                  if(d == orig) unmark_field_dirty(field_name, wrap);
                  else          mark_field_dirty  (field_name, wrap);
              });
          }
      };

      build_date(sched_row, "Start date", is_new ? Wt::WDate{} : m_original.start_date,
                 "start_date", m_start_date_display, m_start_date_edit, m_start_field);
      build_date(sched_row, "End date",   is_new ? Wt::WDate{} : m_original.end_date,
                 "end_date",   m_end_date_display,   m_end_date_edit,   m_end_field);

      // For new tasks, show date edits directly with default start = today
      if(is_new) m_start_date_edit->setDate(Wt::WDate::currentDate());

      // ── Type chips ────────────────────────────────────────────────────────────
      form->addNew<Wt::WText>("<h2>Type</h2>", Wt::TextFormat::UnsafeXHTML);
      auto* type_row = form->addNew<Wt::WContainerWidget>();
      type_row->setStyleClass("kb-type-chips");
      m_type_id = 0;
      const auto types = m_db.types_for_org(m_org_id);
      if(!is_new)
      {
          const bool valid = std::any_of(types.begin(), types.end(),
              [&](const task_type_entry& ty){ return ty.id == m_original.type_id; });
          if(valid) m_type_id = m_original.type_id;
      }

      auto add_chip = [&](long long chip_id, const std::string& lbl, const std::string& hex)
      {
          auto* chip = type_row->addNew<Wt::WContainerWidget>();
          chip->setStyleClass(chip_id == m_type_id ? "kb-type-chip selected" : "kb-type-chip");
          auto* dot = chip->addNew<Wt::WContainerWidget>();
          dot->setStyleClass("kb-type-chip__dot");
          if(hex.size() == 7 && hex[0] == '#')
          {
              try {
                  dot->decorationStyle().setBackgroundColor(Wt::WColor{
                      std::stoi(hex.substr(1,2),nullptr,16),
                      std::stoi(hex.substr(3,2),nullptr,16),
                      std::stoi(hex.substr(5,2),nullptr,16)});
              } catch(...) {}
          }
          chip->addNew<Wt::WText>(lbl, Wt::TextFormat::Plain);
          m_type_chips.push_back(chip);
          if(can_edit)
          {
              chip->clicked().connect([this, chip, chip_id]{
                  m_type_id = chip_id;
                  for(auto* c : m_type_chips) c->removeStyleClass("selected");
                  chip->addStyleClass("selected");
                  const bool same_as_orig = (!m_task_id ? false : chip_id == m_original.type_id);
                  if(same_as_orig) unmark_field_dirty("type", nullptr);
                  else             mark_field_dirty  ("type", nullptr);
              });
          }
      };
      add_chip(0, "(None)", "#cccccc");
      for(const auto& ty : types) add_chip(ty.id, ty.name, ty.color);
      if(!can_edit) for(auto* c : m_type_chips) c->setDisabled(true);

      // ── Comments (existing tasks only) ────────────────────────────────────────
      if(!is_new)
      {
          auto* cs = form->addNew<Wt::WContainerWidget>();
          cs->setStyleClass("kb-comment-section");
          cs->addNew<Wt::WText>("<h2>Comments</h2>", Wt::TextFormat::UnsafeXHTML);
          m_comment_list = cs->addNew<Wt::WContainerWidget>();
          m_comment_list->setStyleClass("kb-comment-list");
          if(caps.has_any(team_cap::comment) && !m_original.is_archived)
          {
              m_comment_compose = cs->addNew<Wt::WContainerWidget>();
              m_comment_compose->setStyleClass("kb-comment-compose");
          }
      }

      // ── Button row ────────────────────────────────────────────────────────────
      auto* btn_row = addNew<Wt::WContainerWidget>();
      btn_row->setStyleClass("editor-btn-row");

      m_save_btn = btn_row->addNew<Wt::WPushButton>(is_new ? "Create Task" : "Save Changes");
      m_save_btn->setStyleClass("editor-btn");
      m_save_btn->clicked().connect([this]{ save(); });
      if(!is_new) m_save_btn->setEnabled(false);
      if(!can_edit && !is_new) m_save_btn->hide();

      if(!is_new && caps.has_any(team_cap::archive_task))
      {
          if(m_original.is_archived)
          {
              auto* btn = btn_row->addNew<Wt::WPushButton>("Unarchive");
              btn->setStyleClass("editor-btn");
              btn->clicked().connect([this]{
                  m_db.unarchive_task(m_original.id, m_username);
                  live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
                  live_hub::instance().broadcast("task:" + std::to_string(m_original.id));
                  m_on_saved();
              });
          }
          else
          {
              auto* btn = btn_row->addNew<Wt::WPushButton>("Archive");
              btn->setStyleClass("editor-btn editor-btn-danger");
              btn->clicked().connect([this, alive = m_alive]{
                  if(!*alive) return;
                  const long long tid = m_original.id;
                  if(m_task_id != 0)
                  {
                      live_hub::instance().unsubscribe(
                          "task:" + std::to_string(m_task_id), m_session_id);
                      m_task_id = 0;
                  }
                  m_db.archive_task(tid, m_username);
                  live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
                  live_hub::instance().broadcast("task:" + std::to_string(tid));
                  m_on_saved();
              });
          }
      }

      if(m_on_cancel)
      {
          auto* cancel = btn_row->addNew<Wt::WPushButton>("Cancel");
          cancel->setStyleClass("editor-btn editor-btn-cancel");
          cancel->clicked().connect([this]{ if(m_on_cancel) m_on_cancel(); });
      }

      // ── Live-hub subscriptions (existing tasks) ───────────────────────────────
      if(!is_new)
      {
          m_session_id = Wt::WApplication::instance()->sessionId();
          live_hub::instance().subscribe(
              "task:" + std::to_string(task_id), m_session_id,
              [this, alive = m_alive]{
                  if(*alive){ mark_stale(); Wt::WApplication::instance()->triggerUpdate(); }
              });
          live_hub::instance().subscribe(
              "task:" + std::to_string(task_id) + ":comments", m_session_id,
              [this, alive = m_alive]{
                  if(*alive){ rebuild_comments(); Wt::WApplication::instance()->triggerUpdate(); }
              });
          rebuild_comments();
      }
  }

  // ── Destructor ────────────────────────────────────────────────────────────────

  task_editor_form_widget::~task_editor_form_widget()
  {
      *m_alive = false;
      if(m_task_id != 0)
      {
          live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
          live_hub::instance().unsubscribe(
              "task:" + std::to_string(m_task_id) + ":comments", m_session_id);
      }
  }

  // ── Public accessors ──────────────────────────────────────────────────────────

  bool task_editor_form_widget::is_dirty() const { return !m_dirty_fields.empty(); }
  bool task_editor_form_widget::is_stale() const { return m_stale; }

  // ── Private helpers ───────────────────────────────────────────────────────────

  std::string task_editor_form_widget::render_markdown(const std::string& md) const
  {
      if(md.empty()) return "<em>(none)</em>";
      char* raw = cmark_markdown_to_html(md.c_str(), md.size(), CMARK_OPT_DEFAULT);
      std::string html = raw ? std::string(raw) : md;
      if(raw) free(raw);
      return html;
  }

  void task_editor_form_widget::mark_stale()
  {
      m_stale = true;
      if(m_stale_banner) m_stale_banner->show();
      if(m_save_btn)     m_save_btn->setEnabled(false);
  }

  void task_editor_form_widget::mark_field_dirty(
      const std::string& field, Wt::WContainerWidget* container)
  {
      m_dirty_fields.insert(field);
      if(container) container->addStyleClass("kb-popup-field--dirty");
      if(m_save_btn && !m_stale) m_save_btn->setEnabled(true);
  }

  void task_editor_form_widget::unmark_field_dirty(
      const std::string& field, Wt::WContainerWidget* container)
  {
      m_dirty_fields.erase(field);
      if(container) container->removeStyleClass("kb-popup-field--dirty");
      if(m_save_btn) m_save_btn->setEnabled(!m_dirty_fields.empty() && !m_stale);
  }

  void task_editor_form_widget::enter_edit_mode(Wt::WText* display, Wt::WWidget* edit)
  {
      display->hide();
      edit->show();
  }

  void task_editor_form_widget::exit_edit_mode(
      Wt::WText* display, Wt::WWidget* edit, const std::string& new_text)
  {
      edit->hide();
      display->setText(new_text);
      display->show();
  }

  void task_editor_form_widget::save()
  {
      const bool is_new = (m_task_id == 0 && !m_on_cancel);
      // Distinguish new vs edit by checking m_original.id
      const bool creating = (m_original.id == 0);

      if(!creating && (!m_caps.has_any(team_cap::edit_task_fields) || m_original.is_archived))
          return;

      const std::string title = m_title_edit->text().toUTF8();
      if(title.empty()) return;

      const int si = m_status_edit->currentIndex();
      const std::string status =
          (si >= 0 && si < static_cast<int>(k_status_vals.size()))
          ? k_status_vals[si] : (creating ? "todo" : m_original.status);

      const int ai = m_assignee_edit->currentIndex();
      const std::string new_assignee =
          (ai >= 0 && ai < static_cast<int>(m_assignee_values.size()))
          ? m_assignee_values[ai] : "";
      const std::string old_assignee = creating ? "" : m_original.assigned_to;

      if(!creating && !m_caps.has_any(team_cap::reassign_task) && new_assignee != old_assignee)
      {
          if(!new_assignee.empty() && new_assignee != m_username) return;
          if(!old_assignee.empty() && old_assignee != m_username) return;
      }

      kanban_task_entry t;
      t.team_id     = m_team_id;
      t.status      = status;
      t.title       = title;
      t.description = m_desc_edit->text().toUTF8();
      t.assigned_to = new_assignee;
      t.type_id     = m_type_id;
      if(const auto d = m_start_date_edit->date(); d.isValid()) t.start_date = d;
      if(const auto d = m_end_date_edit->date();   d.isValid()) t.end_date   = d;

      if(creating)
      {
          t.id = m_db.add_task(t, m_username);
      }
      else
      {
          t.id         = m_original.id;
          t.sort_order = m_original.sort_order;
          try { m_db.update_task(t, m_username); }
          catch(const Wt::Dbo::StaleObjectException&) { mark_stale(); return; }

          if(!new_assignee.empty() && new_assignee != old_assignee &&
             new_assignee != m_username)
          {
              const auto team = m_db.find_team(m_team_id);
              m_odb.push_notification(new_assignee, "task_assigned",
                  make_task_assigned_payload(t.id, title, m_team_id,
                                             team ? team->name : ""));
              live_hub::instance().broadcast("user:" + new_assignee);
          }

          live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
          live_hub::instance().unsubscribe(
              "task:" + std::to_string(m_task_id) + ":comments", m_session_id);
          m_task_id = 0;
          live_hub::instance().broadcast("task:" + std::to_string(m_original.id));
      }

      live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
      m_on_saved();
  }

  void task_editor_form_widget::rebuild_history()
  {
      if(!m_history_panel) return;
      m_history_panel->clear();

      if(m_task_id == 0 && m_original.id == 0)
      {
          m_history_panel->addNew<Wt::WText>("No history yet.", Wt::TextFormat::Plain);
          return;
      }
      const long long hid = m_task_id != 0 ? m_task_id : m_original.id;
      const auto events = m_db.history_for_task(hid);
      if(events.empty())
      {
          m_history_panel->addNew<Wt::WText>("No history yet.", Wt::TextFormat::Plain);
          return;
      }

      static const std::map<std::string,std::string> k_field_labels = {
          {"status","Status"},{"title","Title"},{"description","Description"},
          {"assigned_to","Assigned to"},{"start_date","Start date"},
          {"end_date","End date"},{"type","Type"}};
      static const std::map<std::string,std::string> k_status_display = {
          {"todo","To Do"},{"in_progress","In Progress"},
          {"review","Review"},{"done","Done"}};

      auto display_val = [&](const std::string& field, const std::string& val) -> std::string {
          if(val.empty()) return "(unset)";
          if(field == "status"){ auto it = k_status_display.find(val);
              return it != k_status_display.end() ? it->second : val; }
          return val;
      };

      for(const auto& ev : events)
      {
          auto* entry = m_history_panel->addNew<Wt::WContainerWidget>();
          entry->setStyleClass("kb-history-entry");
          auto* hdr = entry->addNew<Wt::WText>(
              fmt_ts(ev.occurred_at) + "  \xe2\x80\x94  " + ev.actor,
              Wt::TextFormat::Plain);
          hdr->setStyleClass("kb-history-header");

          if(ev.event_type == "created")
              entry->addNew<Wt::WText>("[Task created]", Wt::TextFormat::Plain)
                   ->setStyleClass("kb-history-line");
          else if(ev.event_type == "archived")
              entry->addNew<Wt::WText>("[Task archived]", Wt::TextFormat::Plain)
                   ->setStyleClass("kb-history-line");
          else if(ev.event_type == "unarchived")
              entry->addNew<Wt::WText>("[Task unarchived]", Wt::TextFormat::Plain)
                   ->setStyleClass("kb-history-line");
          else
          {
              for(const auto& ch : ev.changes)
              {
                  const auto lbl = k_field_labels.count(ch.field_name)
                                 ? k_field_labels.at(ch.field_name) : ch.field_name;
                  entry->addNew<Wt::WText>(
                      lbl + ": " + display_val(ch.field_name, ch.old_value) +
                      " \xe2\x86\x92 " + display_val(ch.field_name, ch.new_value),
                      Wt::TextFormat::Plain)->setStyleClass("kb-history-line");
              }
          }
      }
  }

  void task_editor_form_widget::rebuild_comments()
  {
      if(!m_comment_list) return;
      m_comment_list->clear();
      if(m_comment_compose) m_comment_compose->clear();

      const long long cid = m_task_id != 0 ? m_task_id : m_original.id;
      if(cid == 0)
      {
          m_comment_list->addNew<Wt::WText>(
              "Save the task first to add comments.", Wt::TextFormat::Plain);
          return;
      }

      const auto comments = m_db.comments_for_task(cid);
      if(comments.empty())
      {
          m_comment_list->addNew<Wt::WText>("No comments yet.", Wt::TextFormat::Plain)
              ->setStyleClass("kb-comment-deleted");
      }
      else
      {
          for(const auto& c : comments)
          {
              auto* item = m_comment_list->addNew<Wt::WContainerWidget>();
              item->setStyleClass("kb-comment-item");
              if(c.is_deleted)
              {
                  item->setStyleClass("kb-comment-item kb-comment-deleted");
                  item->addNew<Wt::WText>(
                      "Comment deleted by " + c.deleted_by + " \xe2\x80\x94 " +
                      fmt_ts(c.deleted_at), Wt::TextFormat::Plain);
                  continue;
              }
              auto* hdr2 = item->addNew<Wt::WContainerWidget>();
              hdr2->setStyleClass("kb-comment-header");
              hdr2->addNew<Wt::WText>(c.author, Wt::TextFormat::Plain)
                   ->setStyleClass("kb-comment-author");
              hdr2->addNew<Wt::WText>(
                  " \xe2\x80\x94 " + fmt_ts(c.created_at), Wt::TextFormat::Plain);
              auto* bw = item->addNew<Wt::WContainerWidget>();
              bw->setStyleClass("kb-comment-body");
              char* raw2 = cmark_markdown_to_html(
                  c.body.c_str(), c.body.size(), CMARK_OPT_DEFAULT);
              bw->addNew<Wt::WText>(raw2 ? std::string(raw2) : "", Wt::TextFormat::UnsafeXHTML);
              if(raw2) free(raw2);
              if(!c.last_edited_at.empty())
                  item->addNew<Wt::WText>(
                      "Edited by " + c.last_edited_by + " at " + fmt_ts(c.last_edited_at),
                      Wt::TextFormat::Plain)->setStyleClass("kb-comment-meta");

              const bool can_act = m_caps.has_any(team_cap::comment) &&
                                   (c.author == m_username ||
                                    m_caps.has_any(team_cap::manage_team));
              if(can_act)
              {
                  auto* actions = item->addNew<Wt::WContainerWidget>();
                  actions->setStyleClass("kb-comment-actions");
                  auto* edit_btn = actions->addNew<Wt::WPushButton>("Edit");
                  auto* del_btn  = actions->addNew<Wt::WPushButton>("Delete");
                  del_btn->setStyleClass("kb-comment-del-btn");
                  const long long   ccid    = c.id;
                  const std::string cauthor = c.author;
                  const bool        is_own  = (c.author == m_username);
                  auto* ea = item->addNew<Wt::WContainerWidget>();
                  ea->setStyleClass("kb-comment-edit-area");
                  ea->hide();
                  auto* eta = ea->addNew<Wt::WTextArea>();
                  eta->setText(c.body);
                  eta->setStyleClass("editor-field");
                  auto* ebtns = ea->addNew<Wt::WContainerWidget>();
                  ebtns->setStyleClass("kb-comment-edit-btns");
                  auto* save_eb   = ebtns->addNew<Wt::WPushButton>("Save");
                  save_eb->setStyleClass("editor-btn");
                  auto* cancel_eb = ebtns->addNew<Wt::WPushButton>("Cancel");
                  cancel_eb->setStyleClass("editor-btn editor-btn-cancel");
                  edit_btn->clicked().connect([this, cauthor, is_own, bw, ea, actions]{
                      if(is_own){ bw->hide(); actions->hide(); ea->show(); }
                      else
                      {
                          auto* d = new Wt::WDialog("Edit Another User's Comment");
                          d->contents()->addNew<Wt::WText>(
                              "This comment was written by " + cauthor +
                              ". Are you sure you want to edit it?", Wt::TextFormat::Plain);
                          auto* yes = d->footer()->addNew<Wt::WPushButton>("Edit Anyway");
                          yes->setStyleClass("editor-btn");
                          auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
                          no->setStyleClass("editor-btn editor-btn-cancel");
                          yes->clicked().connect([d, bw, ea, actions]{
                              d->accept(); bw->hide(); actions->hide(); ea->show(); });
                          no->clicked().connect([d]{ d->reject(); });
                          d->finished().connect([d](Wt::DialogCode){ delete d; });
                          d->show();
                      }
                  });
                  cancel_eb->clicked().connect([bw, ea, actions]{
                      ea->hide(); bw->show(); actions->show(); });
                  save_eb->clicked().connect([this, ccid, eta, save_eb, alive = m_alive]{
                      if(!*alive) return;
                      const std::string nb = eta->text().toUTF8();
                      if(nb.empty()) return;
                      save_eb->setDisabled(true);
                      const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
                      m_db.edit_comment(ccid, m_username, nb);
                      live_hub::instance().broadcast("task:" + std::to_string(tid) + ":comments");
                      rebuild_comments();
                  });
                  del_btn->clicked().connect([this, ccid, cauthor, is_own]{
                      auto* d = new Wt::WDialog("Delete Comment");
                      const std::string msg = is_own
                          ? "Are you sure you want to delete this comment?"
                          : "This comment was written by " + cauthor +
                            ". Are you sure you want to delete it?";
                      d->contents()->addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
                      auto* yes = d->footer()->addNew<Wt::WPushButton>("Delete");
                      yes->setStyleClass("editor-btn editor-btn-danger");
                      auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
                      no->setStyleClass("editor-btn editor-btn-cancel");
                      yes->clicked().connect([this, d, ccid, alive = m_alive]{
                          d->accept();
                          if(!*alive) return;
                          const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
                          m_db.delete_comment(ccid, m_username);
                          live_hub::instance().broadcast(
                              "task:" + std::to_string(tid) + ":comments");
                          rebuild_comments();
                      });
                      no->clicked().connect([d]{ d->reject(); });
                      d->finished().connect([d](Wt::DialogCode){ delete d; });
                      d->show();
                  });
              }
          }
      }

      if(!m_comment_compose || m_original.is_archived) return;
      auto* ta = m_comment_compose->addNew<Wt::WTextArea>();
      ta->setPlaceholderText("Write a comment (Markdown supported)");
      ta->setStyleClass("editor-field");
      auto* post = m_comment_compose->addNew<Wt::WPushButton>("Post Comment");
      post->setStyleClass("editor-btn kb-comment-post-btn");
      post->setDisabled(true);
      ta->keyWentUp().connect([ta, post]{ post->setDisabled(ta->text().empty()); });
      post->clicked().connect([this, ta, post, alive = m_alive]{
          if(!*alive) return;
          const std::string body = ta->text().toUTF8();
          if(body.empty()) return;
          post->setDisabled(true);
          const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
          m_db.add_comment(tid, m_username, body);
          live_hub::instance().broadcast("task:" + std::to_string(tid) + ":comments");
          ta->setText(Wt::WString{});
          rebuild_comments();
      });
  }
  ```

  Note on `save()`: the `creating` flag is `m_original.id == 0` (no task was loaded), which is true for new tasks.

- [ ] **Step 2: Build**

  ```bash
  cmake --build build --parallel $(nproc)
  ```

  Expected: compiles without errors. The old popup and editor page still compile and work (unchanged so far).

- [ ] **Step 3: Commit**

  ```bash
  git add src/kanban/task_editor_form_widget.cpp
  git commit -m "$(cat <<'EOF'
  feat: implement task_editor_form_widget (shared editor form)

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 6: Rewrite `task_popup_widget` as thin dialog wrapper

**Files:**
- Rewrite: `src/kanban/task_popup_widget.hpp`
- Rewrite: `src/kanban/task_popup_widget.cpp`
- Modify: `src/pages/kanban_board_page.cpp` (remove `type_colors` arg)

- [ ] **Step 1: Replace `src/kanban/task_popup_widget.hpp`**

  ```cpp
  #pragma once

  #include "kanban_db.hpp"
  #include "team_cap.hpp"
  #include "auth/session_data.hpp"
  #include "org/org_db.hpp"

  #include <Wt/WDialog.h>
  #include <Wt/WLineEdit.h>

  #include <memory>
  #include <string>

  class task_editor_form_widget;

  class task_popup_widget : public Wt::WDialog
  {
  public:
      task_popup_widget(kanban_db&          db,
                        org_db&             odb,
                        long long           task_id,
                        const session_data& session,
                        team_cap::flags     caps,
                        long long           team_id);

      ~task_popup_widget() override;

  private:
      task_editor_form_widget* m_form{nullptr};
      Wt::WLineEdit*           m_close_cb{nullptr};

      void try_close();
  };
  ```

- [ ] **Step 2: Replace `src/kanban/task_popup_widget.cpp`**

  ```cpp
  #include "task_popup_widget.hpp"
  #include "task_editor_form_widget.hpp"

  #include <Wt/WAnchor.h>
  #include <Wt/WApplication.h>
  #include <Wt/WLink.h>
  #include <Wt/WPushButton.h>
  #include <Wt/WText.h>

  task_popup_widget::task_popup_widget(kanban_db& db, org_db& odb, long long task_id,
                                       const session_data& session, team_cap::flags caps,
                                       long long team_id)
    : Wt::WDialog{}
  {
      addStyleClass("kb-task-popup");

      const auto task_opt = db.find_task(task_id);
      if(!task_opt)
      {
          setWindowTitle("Task not found");
          contents()->addNew<Wt::WText>("Task not found.", Wt::TextFormat::Plain);
          auto* close_btn = footer()->addNew<Wt::WPushButton>("Close");
          close_btn->setStyleClass("editor-btn editor-btn-cancel");
          close_btn->clicked().connect([this]{ reject(); });
          finished().connect([this](Wt::DialogCode){ delete this; });
          show();
          return;
      }

      setWindowTitle(task_opt->title);

      const std::string edit_url = "/board/" + std::to_string(team_id) +
                                   "/task/" + std::to_string(task_id) + "/edit";
      auto* full_link = titleBar()->addNew<Wt::WAnchor>(
          Wt::WLink{Wt::LinkType::InternalPath, edit_url}, "Open full editor \xe2\x86\x97");
      full_link->setStyleClass("kb-popup-full-link");
      full_link->clicked().connect([this]{ reject(); });

      m_form = contents()->addNew<task_editor_form_widget>(
          db, odb, task_id, team_id, session, caps,
          [this]{ accept(); },
          [this]{ try_close(); });

      // Hidden input: JS → C++ close callback
      m_close_cb = addNew<Wt::WLineEdit>();
      m_close_cb->setStyleClass("kb-cb-hidden");
      const std::string cb_id = m_close_cb->id();
      m_close_cb->changed().connect([this]{ try_close(); });

      auto* close_btn = footer()->addNew<Wt::WPushButton>("Close");
      close_btn->setStyleClass("editor-btn editor-btn-cancel");
      close_btn->clicked().connect([this]{ try_close(); });

      finished().connect([this](Wt::DialogCode){ delete this; });
      show();

      // Wire Escape key and modal-cover click to try_close()
      doJavaScript(
          "(function(cbId){"
          "  function fireClose(){"
          "    var inp=document.getElementById(cbId);"
          "    if(!inp)return;"
          "    inp.value='CLOSE';"
          "    inp.dispatchEvent(new Event('change'));"
          "  }"
          "  document.addEventListener('keydown',function onEsc(e){"
          "    if(e.key==='Escape'){"
          "      document.removeEventListener('keydown',onEsc);"
          "      fireClose();"
          "    }"
          "  });"
          "  var cover=document.querySelector('.Wt-dialogcover');"
          "  if(cover)cover.addEventListener('click',fireClose,{once:true});"
          "})('" + cb_id + "');");
  }

  task_popup_widget::~task_popup_widget() = default;

  void task_popup_widget::try_close()
  {
      if(m_form && m_form->is_dirty())
      {
          auto* d = new Wt::WDialog("Unsaved Changes");
          d->contents()->addNew<Wt::WText>(
              "You have unsaved changes. Discard them?", Wt::TextFormat::Plain);
          auto* discard = d->footer()->addNew<Wt::WPushButton>("Discard Changes");
          discard->setStyleClass("editor-btn editor-btn-danger");
          auto* keep = d->footer()->addNew<Wt::WPushButton>("Keep Editing");
          keep->setStyleClass("editor-btn editor-btn-cancel");
          discard->clicked().connect([this, d]{ d->accept(); reject(); });
          keep->clicked().connect([d]{ d->reject(); });
          d->finished().connect([d](Wt::DialogCode){ delete d; });
          d->show();
      }
      else
      {
          reject();
      }
  }
  ```

- [ ] **Step 3: Update `src/pages/kanban_board_page.cpp` — remove `type_colors` from popup calls**

  Find both occurrences of `new task_popup_widget(m_db, m_odb, tid, m_session, m_caps, m_type_colors, m_team_id)` and change to:

  ```cpp
  new task_popup_widget(m_db, m_odb, tid, m_session, m_caps, m_team_id);
  ```

  There are two call sites (one in the gantt callback, one in the board callback).

- [ ] **Step 4: Build**

  ```bash
  cmake --build build --parallel $(nproc)
  ```

  Expected: compiles without errors.

- [ ] **Step 5: Commit**

  ```bash
  git add src/kanban/task_popup_widget.hpp src/kanban/task_popup_widget.cpp \
          src/pages/kanban_board_page.cpp
  git commit -m "$(cat <<'EOF'
  refactor: task_popup_widget becomes thin dialog wrapper over task_editor_form_widget

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 7: Rewrite `kanban_task_editor_page` as thin page wrapper

**Files:**
- Rewrite: `src/pages/kanban_task_editor_page.hpp`
- Rewrite: `src/pages/kanban_task_editor_page.cpp`

- [ ] **Step 1: Replace `src/pages/kanban_task_editor_page.hpp`**

  ```cpp
  #pragma once

  #include "auth/session_data.hpp"
  #include "kanban/kanban.hpp"
  #include "kanban/kanban_db.hpp"
  #include "kanban/team_cap.hpp"
  #include "org/org_db.hpp"

  #include <Wt/WContainerWidget.h>

  #include <functional>
  #include <string>
  #include <vector>

  class kanban_task_editor_page : public Wt::WContainerWidget
  {
  public:
      // members and types are accepted but unused (form fetches from DB directly).
      kanban_task_editor_page(kanban_db&                          db,
                              org_db&                             odb,
                              long long                           team_id,
                              const session_data&                 session,
                              team_cap::flags                     caps,
                              const kanban_task_entry*            existing,
                              const std::vector<std::string>&     members,
                              const std::vector<task_type_entry>& types,
                              std::function<void()>               on_save);
  };
  ```

- [ ] **Step 2: Replace `src/pages/kanban_task_editor_page.cpp`**

  ```cpp
  #include "kanban_task_editor_page.hpp"
  #include "kanban/task_editor_form_widget.hpp"

  #include <Wt/WApplication.h>
  #include <Wt/WText.h>

  kanban_task_editor_page::kanban_task_editor_page(
      kanban_db& db, org_db& odb, long long team_id,
      const session_data& session, team_cap::flags caps,
      const kanban_task_entry* existing,
      const std::vector<std::string>& /*members*/,
      const std::vector<task_type_entry>& /*types*/,
      std::function<void()> on_save)
  {
      setStyleClass("page kb-editor-page");

      const bool is_new = (existing == nullptr);
      addNew<Wt::WText>(
          is_new ? "<h1>New Task</h1>" : "<h1>Edit Task</h1>",
          Wt::TextFormat::UnsafeXHTML);

      const long long   task_id   = existing ? existing->id : 0;
      const std::string board_url = "/board/" + std::to_string(team_id);

      addNew<task_editor_form_widget>(
          db, odb, task_id, team_id, session, caps,
          on_save,  // on_saved
          [board_url]{
              Wt::WApplication::instance()->setInternalPath(board_url, true);
          });
  }
  ```

- [ ] **Step 3: Build**

  ```bash
  cmake --build build --parallel $(nproc)
  ```

  Expected: compiles without errors.

- [ ] **Step 4: Commit**

  ```bash
  git add src/pages/kanban_task_editor_page.hpp src/pages/kanban_task_editor_page.cpp
  git commit -m "$(cat <<'EOF'
  refactor: kanban_task_editor_page becomes thin wrapper over task_editor_form_widget

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 8: SCSS — popup scrolling, inline title, card cursor, description display

**Files:**
- Modify: `resources/scss/_kanban.scss`

- [ ] **Step 1: Apply all SCSS changes**

  **a. Inside `.kb-task-popup` (add new rule at the end of the task-popup section):**
  ```scss
  .kb-task-popup {
    .Wt-dialog-contents {
      max-height: calc(90vh - 120px);
      overflow-y: auto;
    }
  }
  ```

  **b. Add `.kb-popup-title-field` rule in the Task popup section:**
  ```scss
  .kb-popup-title-field {
    display:     flex;
    align-items: baseline;
    gap:         0.5rem;

    .kb-field-label   { flex-shrink: 0; }
    .kb-popup-display,
    .editor-field     { flex: 1; min-width: 0; }
  }
  ```

  **c. Add `.kb-desc-display` rule:**
  ```scss
  .kb-desc-display {
    font-size:   0.9rem;
    line-height: 1.55;
    min-height:  1.5em;

    p           { margin: 0 0 0.4rem; }
    p:last-child { margin-bottom: 0; }
    em          { color: var(--color-muted); font-style: italic; } // for "(none)"
    code        { font-family: var(--font-mono); font-size: 0.85em; }
  }
  ```

  **d. In `.kb-card`, change `cursor: default` to `cursor: pointer`** (already done in Task 2).

  **e. Confirm `.kb-card-edit` block has been removed** (already done in Task 2).

- [ ] **Step 2: Build**

  ```bash
  cmake --build build --parallel $(nproc)
  ```

  Expected: SCSS compiles, CSS output updated.

- [ ] **Step 3: Commit**

  ```bash
  git add resources/scss/_kanban.scss
  git commit -m "$(cat <<'EOF'
  style: popup scrolling, inline title field, desc display, card cursor

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 9: Build, run full test suite, fix failures

**Files:** Any that need fixes based on test output.

- [ ] **Step 1: Full build**

  ```bash
  cmake --build build --parallel $(nproc)
  ```

- [ ] **Step 2: Run JS unit tests**

  ```bash
  cd tests/js && node test_gantt.js
  ```

  Expected: all pass.

- [ ] **Step 3: Start the server** (in a separate terminal, keep running for E2E)

  ```bash
  ./build/altinf \
    --docroot ./build/resources \
    --http-address 0.0.0.0 --http-port 8080 \
    --https-address 0.0.0.0 --https-port 8443 \
    --ssl-certificate ./certs/cert.pem \
    --ssl-private-key ./certs/key.pem \
    --ssl-tmp-dh ./certs/dh.pem
  ```

- [ ] **Step 4: Run the full E2E suite**

  ```bash
  cd e2e && npx playwright test
  ```

  Expected: all tests pass, including the new tests added in Task 3. If any fail, fix the root cause, rebuild, and re-run. Do not skip or comment out tests.

- [ ] **Step 5: Commit any fixes**

  ```bash
  git add -p   # stage only the fix files
  git commit -m "$(cat <<'EOF'
  fix: address test failures from popup enhancements

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Self-Review Notes

- All 11 spec requirements are covered: scroll (Task 8), revert-on-blur (Task 5), markdown desc (Task 5), inline title (Tasks 5+8), save disabled (Task 5 `unmark_field_dirty`), confirm close (Task 6 `try_close`), Escape/click-outside (Task 6 JS), History tab + Archive (Task 5), shared widget (Tasks 6+7), kanban card click (Task 2), gantt label click (Task 1).
- No placeholder steps — all code is complete.
- Type consistency: `task_editor_form_widget` declared in Task 4 header, used in Tasks 5/6/7 with matching signatures.
- `is_dirty()` / `is_stale()` defined in header (Task 4) and used in `try_close()` (Task 6).
- `creating` flag in `save()` is `m_original.id == 0`, which is correct: for new tasks `m_original` is default-constructed with `id == 0`.
