# Gantt Status Filter and Default Start Date Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hide "To Do" and "Done" tasks from the Gantt view (live), and pre-fill today's date as the default start date for new tasks.

**Architecture:** The status filter lives in `gantt_view_widget::serialize_tasks()` — the single function through which all tasks pass before being sent to the browser, covering both initial render and every live `refresh()` call. The default start date is a one-line addition in the new-task branch of `kanban_task_editor_page`. Existing E2E tests that create Gantt tasks with default ("To Do") status must be updated to use "In Progress" so they remain visible after the filter is applied.

**Tech Stack:** C++ / Wt, Playwright E2E (TypeScript)

---

## Files

| File | Change |
|------|--------|
| `e2e/specs/board.spec.ts` | Add 3 new Gantt filter tests; update 2 existing Gantt tests to use in_progress status; add 1 start date default test |
| `e2e/specs/live-board.spec.ts` | Update 3 existing Gantt tests to use in_progress status; add 1 live Gantt filter test |
| `src/kanban/gantt_view_widget.cpp` | Skip `"todo"` and `"done"` tasks in `serialize_tasks()` |
| `src/pages/kanban_task_editor_page.cpp` | Default start date to today for new tasks |

---

## Task 1: Write failing Gantt filter tests

**Files:**
- Modify: `e2e/specs/board.spec.ts`

Add two new tests at the end of the `// ── Gantt view ──` section (after the `'Board tab returns from Gantt view...'` test at line 418, but before the team management section). These assert that To Do and Done tasks are absent from the Gantt SVG — they will fail until the C++ filter is in place.

- [ ] **Step 1: Add two failing tests to board.spec.ts**

Insert after the `'Board tab returns from Gantt view to kanban board'` test (after line 420) and before the `'Gantt view renders an SVG for tasks that have dates'` test:

```typescript
test('Gantt view hides tasks with To Do status', async ({ page }) => {
  await loginAndGoToBoard(page);
  // Create a task in To Do (default) status with dates spanning today.
  await createTaskWithDates(page, 'GanttTodoFilterTask', '2026-04-01', '2026-12-31');
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
  await expect(page.locator('.gv-scroll svg')).toBeVisible({ timeout: 15_000 });
  await expect(page.locator('.gv-scroll svg text', { hasText: 'GanttTodoFilterTask' })).not.toBeAttached();
});

test('Gantt view hides tasks with Done status', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('GanttDoneFilterTask');
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'Done' });
  const startInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
  const endInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'End date' }).locator('input').first();
  await startInput.fill('2026-04-01');
  await startInput.press('Tab');
  await endInput.fill('2026-12-31');
  await endInput.press('Tab');
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
  await expect(page.locator('.gv-scroll svg')).toBeVisible({ timeout: 15_000 });
  await expect(page.locator('.gv-scroll svg text', { hasText: 'GanttDoneFilterTask' })).not.toBeAttached();
});
```

Note: `.gv-scroll svg` will be visible because `GanttTodoFilterTask` and `GanttDoneFilterTask` appear in the Gantt before the filter is implemented (all tasks currently pass through). After the filter lands, they will be absent from the SVG.

- [ ] **Step 2: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: build succeeds.

- [ ] **Step 3: Run the two new tests to confirm they fail**

```bash
cd e2e && npx playwright test specs/board.spec.ts --grep "hides tasks"
```

Expected: both tests fail — the tasks are currently visible in the Gantt SVG so `.not.toBeAttached()` does not hold.

---

## Task 2: Implement Gantt status filter + fix breaking existing tests

**Files:**
- Modify: `src/kanban/gantt_view_widget.cpp:63`
- Modify: `e2e/specs/board.spec.ts` (2 existing tests)
- Modify: `e2e/specs/live-board.spec.ts` (3 existing tests + 1 new test)

### Step 1: Add filter to serialize_tasks()

- [ ] **Step 1: Edit serialize_tasks() to skip todo and done tasks**

In `src/kanban/gantt_view_widget.cpp`, change the loop body starting at line 63:

```cpp
// BEFORE
	for(const auto& t: tasks)
	{
		if(!first)
		{
			ss << ',';
		}
		first = false;
```

```cpp
// AFTER
	for(const auto& t: tasks)
	{
		if(t.status == "todo" || t.status == "done")
			continue;
		if(!first)
		{
			ss << ',';
		}
		first = false;
```

### Step 2: Fix board.spec.ts existing Gantt tests

Two existing tests create tasks with default To Do status for Gantt verification. They must use In Progress so the tasks remain visible after the filter.

- [ ] **Step 2: Update 'Gantt view renders an SVG for tasks that have dates' to set In Progress status**

In `e2e/specs/board.spec.ts`, find the test at line ~422. Add the status selection after the title fill and before the date fills:

```typescript
// BEFORE
  await page.locator('input[placeholder="Task title"]').fill('Board Test Task Nu');
  const startInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
```

```typescript
// AFTER
  await page.locator('input[placeholder="Task title"]').fill('Board Test Task Nu');
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  const startInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
```

- [ ] **Step 3: Update 'Gantt view renders today line and label when a task spans today' to set In Progress status**

In `e2e/specs/board.spec.ts`, find the test at line ~456 which calls `createTaskWithDates(page, 'Board Test Task Today Span', '2026-01-01', '2026-12-31')`. Replace that single call with inline steps that also set the status:

```typescript
// BEFORE
  await createTaskWithDates(page, 'Board Test Task Today Span', '2026-01-01', '2026-12-31');
```

```typescript
// AFTER
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('Board Test Task Today Span');
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  const tsStart = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
  const tsEnd = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'End date' }).locator('input').first();
  await tsStart.fill('2026-01-01');
  await tsStart.press('Tab');
  await tsEnd.fill('2026-12-31');
  await tsEnd.press('Tab');
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
```

### Step 3: Fix live-board.spec.ts existing Gantt tests + add live filter test

- [ ] **Step 4: Update 'gantt: task created with dates by A appears on B gantt view' to set In Progress**

In `e2e/specs/live-board.spec.ts` at line ~168. Add status selection after the title fill:

```typescript
// BEFORE
  await pageA.locator('input[placeholder="Task title"]').fill('GanttTask');
  const startInput = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
```

```typescript
// AFTER
  await pageA.locator('input[placeholder="Task title"]').fill('GanttTask');
  await pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  const startInput = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
```

- [ ] **Step 5: Update 'gantt: date change by A updates bar on B gantt view' to set In Progress for DateTask**

In `e2e/specs/live-board.spec.ts` at line ~220. Add status selection after the title fill:

```typescript
// BEFORE
  await pageA.locator('input[placeholder="Task title"]').fill('DateTask');
  const startInput = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
```

```typescript
// AFTER
  await pageA.locator('input[placeholder="Task title"]').fill('DateTask');
  await pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  const startInput = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
```

- [ ] **Step 6: Add live Gantt filter test to live-board.spec.ts**

Append after the `'gantt: date change by A updates bar on B gantt view'` test (after line 258):

```typescript
test('gantt: dragging task to Done on board removes it from live Gantt', async ({ browser }) => {
  // Setup: create GanttStatusTask with In Progress status and dates spanning today.
  const setup = await browser.newContext();
  const setupPage = await setup.newPage();
  await loginAs(setupPage, 'admin', 'testpass');
  await navigateToBoard(setupPage, 'LiveBoardOrg');
  await setupPage.locator('.kb-new-btn').click();
  await expect(setupPage.locator('.kb-editor-page')).toBeVisible();
  await setupPage.locator('input[placeholder="Task title"]').fill('GanttStatusTask');
  await setupPage.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  const ssStart = setupPage.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
  const ssEnd = setupPage.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'End date' }).locator('input').first();
  await ssStart.fill('2026-04-01');
  await ssStart.press('Tab');
  await ssEnd.fill('2026-12-31');
  await ssEnd.press('Tab');
  await setupPage.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
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
  await navigateToGantt(pageB, 'LiveBoardOrg');

  // GanttStatusTask is in_progress with dates spanning today — visible in B's Gantt.
  await expect(pageB.locator('.gv-scroll svg text', { hasText: 'GanttStatusTask' })).toBeVisible({ timeout: 15_000 });

  // A drags GanttStatusTask to Done.
  const card = pageA.locator('.kb-column[data-status="in_progress"] .kb-card', { hasText: 'GanttStatusTask' });
  await card.dragTo(pageA.locator('.kb-column[data-status="done"]'));

  // B's Gantt must no longer show GanttStatusTask — done tasks are filtered.
  await expect(pageB.locator('.gv-scroll svg text', { hasText: 'GanttStatusTask' })).not.toBeVisible({ timeout: 15_000 });

  await ctxA.close();
  await ctxB.close();
});
```

---

## Task 3: Build and run all tests for Gantt filter

**Files:** none

- [ ] **Step 1: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: build succeeds.

- [ ] **Step 2: Run E2E tests**

```bash
cd e2e && npx playwright test
```

Expected: all tests pass, including the two new filter tests and the new live Gantt filter test.

- [ ] **Step 3: Run JS unit tests**

```bash
cd tests/js && npm test
```

Expected: all tests pass (no changes to gantt.js).

- [ ] **Step 4: Run Catch2 tests**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass.

---

## Task 4: Commit Gantt filter work

- [ ] **Step 1: Commit**

```bash
git add src/kanban/gantt_view_widget.cpp \
        e2e/specs/board.spec.ts \
        e2e/specs/live-board.spec.ts
git commit -m "feat: filter To Do and Done tasks from Gantt view

Tasks with status 'todo' or 'done' are now excluded from the Gantt
view. The filter is applied in serialize_tasks() so it covers both
the initial render and every live refresh push."
```

---

## Task 5: Write failing default start date test

**Files:**
- Modify: `e2e/specs/board.spec.ts`

- [ ] **Step 1: Add failing start date test to board.spec.ts**

Insert a new test in the `// ── task editor form ──` section (after `'task editor shows Start date and End date fields'` at line ~177):

```typescript
test('new task editor pre-fills start date with today', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  const today = new Date();
  const pad = (n: number) => String(n).padStart(2, '0');
  const todayStr = `${today.getFullYear()}-${pad(today.getMonth() + 1)}-${pad(today.getDate())}`;
  const startInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
  await expect(startInput).toHaveValue(todayStr);
});
```

- [ ] **Step 2: Build and run the new test to confirm it fails**

```bash
cmake --build build --parallel $(nproc) && \
  cd e2e && npx playwright test specs/board.spec.ts --grep "pre-fills start date"
```

Expected: test fails — the start date input is currently empty for new tasks.

---

## Task 6: Implement default start date

**Files:**
- Modify: `src/pages/kanban_task_editor_page.cpp:170`

- [ ] **Step 1: Add else branch to pre-fill start date with today**

In `src/pages/kanban_task_editor_page.cpp`, change lines ~170-173:

```cpp
// BEFORE
	if(existing && existing->start_date.isValid())
	{
		m_start_date->setDate(existing->start_date);
	}
```

```cpp
// AFTER
	if(existing && existing->start_date.isValid())
	{
		m_start_date->setDate(existing->start_date);
	}
	else if(!existing)
	{
		m_start_date->setDate(Wt::WDate::currentDate());
	}
```

---

## Task 7: Build and run all tests for start date default

- [ ] **Step 1: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: build succeeds.

- [ ] **Step 2: Run E2E tests**

```bash
cd e2e && npx playwright test
```

Expected: all tests pass including `'new task editor pre-fills start date with today'`.

- [ ] **Step 3: Run JS unit tests**

```bash
cd tests/js && npm test
```

Expected: all tests pass.

- [ ] **Step 4: Run Catch2 tests**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass.

---

## Task 8: Commit start date default

- [ ] **Step 1: Commit**

```bash
git add src/pages/kanban_task_editor_page.cpp \
        e2e/specs/board.spec.ts
git commit -m "feat: default new task start date to today"
```
