import { test, expect, type Page } from '@playwright/test';

// All tests share the same SQLite database, so state accumulates across tests
// within a run.  Serial mode keeps them ordered and avoids parallel-write races.
test.describe.configure({ mode: 'serial' });

// ── helpers ──────────────────────────────────────────────────────────────────

async function login(page: Page) {
  await page.goto('/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function loginAndGoToBoard(page: Page) {
  await login(page);
  // Navigate explicitly via the Orgs admin page to avoid last_org interference
  // from concurrently-running notification tests that also log in as admin.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: 'BoardTestOrg' }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-page')).toBeVisible();
  // Wait for the JS board to initialise — initKanban() sets .kb-board on the mount div.
  await expect(page.locator('.kb-board')).toBeVisible();
}

// ── setup ─────────────────────────────────────────────────────────────────────

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await login(page);

  // Create the test org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill('BoardTestOrg');
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: 'BoardTestOrg' })).toBeVisible();

  // Navigate to the org manage page and create a team.
  await page.locator('.org-list-link', { hasText: 'BoardTestOrg' }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('input[placeholder="Team name"]').fill('Test Team');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  await page.close();
});

/** Create a task with only a title and wait until the board is back. */
async function createTask(page: Page, title: string) {
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill(title);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

/** Create a task with title, start date, and end date, then wait until the board is back. */
async function createTaskWithDates(page: Page, title: string, startDate: string, endDate: string) {
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill(title);
  const startInput = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
  const endInput   = page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
  await startInput.fill(startDate);
  await startInput.press('Tab');
  await endInput.fill(endDate);
  await endInput.press('Tab');
  // Wait for Wt's change-event AJAXes to complete before clicking Save.
  // Without this, the server's widget state may still be stale when save() runs
  // because Wt only re-encodes a field when its value changed since the last AJAX
  // (form-data caching), so the Save payload may not include the dates.
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

// ── access control ────────────────────────────────────────────────────────────

test('unauthenticated visit to /board redirects to login', async ({ page }) => {
  await page.goto('/board/1');
  await expect(page.locator('.login-form')).toBeVisible();
});

// ── board structure ───────────────────────────────────────────────────────────

test('board page is reachable via org nav link after login', async ({ page }) => {
  await login(page);
  // Boards are accessed through the Orgs page; there is no standalone 'Board' link.
  // Navigate by name to be robust against last_org changes from concurrent tests.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: 'BoardTestOrg' }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-page')).toBeVisible();
});

test('board shows exactly four columns', async ({ page }) => {
  await loginAndGoToBoard(page);
  await expect(page.locator('.kb-column')).toHaveCount(4);
});

test('board columns have correct labels in order', async ({ page }) => {
  await loginAndGoToBoard(page);
  const titles = page.locator('.kb-col-title');
  await expect(titles.nth(0)).toContainText('To Do');
  await expect(titles.nth(1)).toContainText('In Progress');
  await expect(titles.nth(2)).toContainText('Review');
  await expect(titles.nth(3)).toContainText('Done');
});

test('board shows Board and Gantt view tabs', async ({ page }) => {
  await loginAndGoToBoard(page);
  await expect(page.locator('.kb-tab', { hasText: 'Board' })).toBeVisible();
  await expect(page.locator('.kb-tab', { hasText: 'Gantt' })).toBeVisible();
});

test('Board tab has active style, Gantt tab does not', async ({ page }) => {
  await loginAndGoToBoard(page);
  await expect(page.locator('.kb-tab--active', { hasText: 'Board' })).toBeVisible();
  await expect(page.locator('.kb-tab--active', { hasText: 'Gantt' })).not.toBeVisible();
});

test('admin sees + New Task button', async ({ page }) => {
  await loginAndGoToBoard(page);
  await expect(page.locator('.kb-new-btn')).toBeVisible();
});

test('admin sees Manage Team link', async ({ page }) => {
  await loginAndGoToBoard(page);
  await expect(page.locator('.kb-manage-link', { hasText: 'Manage Team' })).toBeVisible();
});

test('board header shows team name', async ({ page }) => {
  await loginAndGoToBoard(page);
  // The h1 is inside .kb-page-hdr; just assert it is non-empty text.
  await expect(page.locator('.kb-page-hdr h1')).not.toBeEmpty();
});

// ── task editor form ──────────────────────────────────────────────────────────

test('+ New Task button opens task editor', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
});

test('task editor shows title and description fields', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('input[placeholder="Task title"]')).toBeVisible();
  await expect(page.locator('textarea[placeholder="Description (optional)"]')).toBeVisible();
});

test('task editor shows Status and Assigned to dropdowns', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  const statusSelect = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' }).locator('select');
  const assigneeSelect = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Assigned to' }).locator('select');
  await expect(statusSelect).toBeVisible();
  await expect(assigneeSelect).toBeVisible();
});

test('task editor shows Start date and End date fields', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first()).toBeVisible();
  await expect(page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first()).toBeVisible();
});

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

test('submitting task without a title shows validation error', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.editor-status')).toContainText('Title is required');
});

test('cancel in task editor returns to board', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('.editor-btn-row .editor-btn-cancel').click();
  await expect(page.locator('.kb-page')).toBeVisible();
});

test('new task editor does not show Delete button', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.editor-btn-danger')).not.toBeVisible();
});

// ── task creation ─────────────────────────────────────────────────────────────

test('creating a task returns to the board', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Alpha');
  await expect(page.locator('.kb-page')).toBeVisible();
  await expect(page.locator('.kb-board')).toBeVisible();
});

test('new task appears in the To Do column by default', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Beta');
  await expect(
    page.locator('.kb-column[data-status="todo"] .kb-card', { hasText: 'Board Test Task Beta' })
  ).toBeVisible();
});

test('card count badge in To Do column increments after creating a task', async ({ page }) => {
  await loginAndGoToBoard(page);
  const todoCount = page.locator('.kb-column[data-status="todo"] .kb-col-count');
  const before = parseInt(await todoCount.textContent() ?? '0', 10);
  await createTask(page, 'Board Test Task Gamma');
  await expect(todoCount).toHaveText(String(before + 1));
});

test('task created with In Progress status appears in that column', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-new-btn').click();
  await page.locator('input[placeholder="Task title"]').fill('Board Test Task Delta');
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(
    page.locator('.kb-column[data-status="in_progress"] .kb-card', { hasText: 'Board Test Task Delta' })
  ).toBeVisible();
  // Must not appear in To Do
  await expect(
    page.locator('.kb-column[data-status="todo"] .kb-card', { hasText: 'Board Test Task Delta' })
  ).not.toBeVisible();
});

test('created task card shows its title', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Epsilon');
  const card = page.locator('.kb-card', { hasText: 'Board Test Task Epsilon' });
  await expect(card.locator('.kb-card-title')).toContainText('Board Test Task Epsilon');
});

test('created task card shows Edit button', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Zeta');
  await expect(
    page.locator('.kb-card', { hasText: 'Board Test Task Zeta' }).locator('.kb-card-edit')
  ).toBeVisible();
});

// ── task editing ──────────────────────────────────────────────────────────────

test('Edit button opens editor with prefilled title', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Eta');
  await page.locator('.kb-card', { hasText: 'Board Test Task Eta' }).locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await expect(page.locator('input[placeholder="Task title"]')).toHaveValue('Board Test Task Eta');
});

test('edit editor shows Delete button for existing task', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Theta');
  await page.locator('.kb-card', { hasText: 'Board Test Task Theta' }).locator('.kb-card-edit').click();
  await expect(page.locator('.editor-btn-danger')).toBeVisible();
});

test('saving edited task updates its card on the board', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Iota');
  await page.locator('.kb-card', { hasText: 'Board Test Task Iota' }).locator('.kb-card-edit').click();
  await page.locator('input[placeholder="Task title"]').fill('Board Test Task Iota Renamed');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(page.locator('.kb-card-title', { hasText: /^Board Test Task Iota Renamed$/ })).toBeVisible();
  await expect(page.locator('.kb-card-title', { hasText: /^Board Test Task Iota$/ })).not.toBeVisible();
});

test('cancel in edit editor returns to board without changing the task', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Kappa');
  await page.locator('.kb-card', { hasText: 'Board Test Task Kappa' }).locator('.kb-card-edit').click();
  await page.locator('input[placeholder="Task title"]').fill('Should Not Appear');
  await page.locator('.editor-btn-row .editor-btn-cancel').click();
  await expect(page.locator('.kb-page')).toBeVisible();
  await expect(page.locator('.kb-card', { hasText: 'Should Not Appear' })).not.toBeVisible();
  await expect(page.locator('.kb-card', { hasText: 'Board Test Task Kappa' })).toBeVisible();
});

test('Delete button removes the task from the board', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTask(page, 'Board Test Task Lambda');
  await page.locator('.kb-card', { hasText: 'Board Test Task Lambda' }).locator('.kb-card-edit').click();
  await page.locator('.editor-btn-danger').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(page.locator('.kb-card', { hasText: 'Board Test Task Lambda' })).not.toBeVisible();
});

// ── date clearing ─────────────────────────────────────────────────────────────

test('Clear button on start date removes it after saving', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTaskWithDates(page, 'Board Test Task Xi', '2025-03-01', '2025-03-31');

  const card = page.locator('.kb-card', { hasText: 'Board Test Task Xi' });
  // Dates are saved — both should appear on the card.
  await expect(card.locator('.kb-card-dates')).toHaveText('2025-03-01 – 2025-03-31');

  await card.locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('.kb-date-clear').click();
  // Wait for the Wt AJAX round-trip: the start date input should be empty.
  const startInput = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
  await expect(startInput).toHaveValue('');

  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // End date is still set, so the date row shows "? – 2025-03-31".
  await expect(card.locator('.kb-card-dates')).toHaveText('? – 2025-03-31');
});

test('Clear button on end date removes it after saving', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTaskWithDates(page, 'Board Test Task Omicron', '2025-04-01', '2025-04-30');

  const card = page.locator('.kb-card', { hasText: 'Board Test Task Omicron' });
  await expect(card.locator('.kb-card-dates')).toHaveText('2025-04-01 – 2025-04-30');

  await card.locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('.kb-date-clear').click();
  const endInput = page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
  await expect(endInput).toHaveValue('');

  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Start date is still set, so the date row shows "2025-04-01 – ?".
  await expect(card.locator('.kb-card-dates')).toHaveText('2025-04-01 – ?');
});

test('clearing both dates removes the date row from the card', async ({ page }) => {
  await loginAndGoToBoard(page);
  await createTaskWithDates(page, 'Board Test Task Rho', '2025-05-01', '2025-05-31');

  const card = page.locator('.kb-card', { hasText: 'Board Test Task Rho' });
  await expect(card.locator('.kb-card-dates')).toHaveText('2025-05-01 – 2025-05-31');

  await card.locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('.kb-date-clear').click();
  const startInput = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
  await expect(startInput).toHaveValue('');
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('.kb-date-clear').click();
  const endInput = page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
  await expect(endInput).toHaveValue('');

  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // No dates remain — the date row should not be present on the card at all.
  await expect(card.locator('.kb-card-dates')).not.toBeVisible();
});

// ── drag-and-drop ─────────────────────────────────────────────────────────────

test('dragging a card to a different column moves it there', async ({ page }) => {
  await loginAndGoToBoard(page);

  // Use the first todo card — it is always at the top of the list and always in the viewport,
  // avoiding coordinate-mismatch issues when many accumulated serial-test tasks push later cards
  // below the fold.
  const card = page.locator('.kb-column[data-status="todo"] .kb-card').first();
  await expect(card).toBeVisible();
  const cardId = await card.getAttribute('data-id');

  await card.dragTo(page.locator('.kb-column[data-status="done"]'));

  // After the Wt AJAX round-trip and JS re-render the card should be in Done.
  await expect(
    page.locator(`.kb-column[data-status="done"] .kb-card[data-id="${cardId}"]`)
  ).toBeVisible({ timeout: 15_000 });
  await expect(
    page.locator(`.kb-column[data-status="todo"] .kb-card[data-id="${cardId}"]`)
  ).not.toBeVisible();
});

// ── Gantt view ────────────────────────────────────────────────────────────────

test('Gantt tab navigates to the Gantt view', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
});

test('Gantt tab has active style after navigation', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
  await expect(page.locator('.kb-tab--active', { hasText: 'Gantt' })).toBeVisible();
  await expect(page.locator('.kb-tab--active', { hasText: 'Board' })).not.toBeVisible();
});

test('Board tab returns from Gantt view to kanban board', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
  await page.locator('.kb-tab', { hasText: 'Board' }).click();
  await expect(page.locator('.kb-board')).toBeVisible();
});

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
  const startInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
  const endInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'End date' }).locator('input').first();
  await startInput.fill('2026-04-01');
  await startInput.press('Tab');
  await endInput.fill('2026-12-31');
  await endInput.press('Tab');
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'Done' });
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(
    page.locator('.kb-column[data-status="done"] .kb-card', { hasText: 'GanttDoneFilterTask' })
  ).toBeVisible();
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
  await expect(page.locator('.gv-scroll svg')).toBeVisible({ timeout: 15_000 });
  await expect(page.locator('.gv-scroll svg text', { hasText: 'GanttDoneFilterTask' })).not.toBeAttached();
});

test('Gantt view renders an SVG for tasks that have dates', async ({ page }) => {
  await loginAndGoToBoard(page);

  // Create a task with start and end dates.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('Board Test Task Nu');
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  const startInput = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
  const endInput   = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'End date' }).locator('input').first();
  await startInput.fill('2025-01-01');
  await startInput.press('Tab');
  await endInput.fill('2025-01-31');
  await endInput.press('Tab');
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Switch to Gantt — the SVG should render.
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
  await expect(page.locator('.gv-scroll svg')).toBeVisible({ timeout: 15_000 });
});

test('Gantt view shows no today line when today is outside all task date ranges', async ({ page }) => {
  await loginAndGoToBoard(page);
  // At this point only "Board Test Task Nu" (2025-01-01–2025-01-31) has both
  // dates set.  Today (2026-04-28) lies beyond that range, so no today line
  // should be drawn.
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-scroll svg')).toBeVisible({ timeout: 15_000 });
  await expect(page.locator('.gv-scroll svg line[style*="e05252"]')).not.toBeAttached();
});

test('Gantt view renders today line and label when a task spans today', async ({ page }) => {
  await loginAndGoToBoard(page);
  // Create a task whose date range brackets today, with In Progress
  // status so it is not filtered out of the Gantt.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('Board Test Task Today Span');
  const tsStart = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Start date' }).locator('input').first();
  const tsEnd = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'End date' }).locator('input').first();
  await tsStart.fill('2026-01-01');
  await tsStart.press('Tab');
  await tsEnd.fill('2026-12-31');
  await tsEnd.press('Tab');
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' })
    .locator('select').selectOption({ label: 'In Progress' });
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-scroll svg')).toBeVisible({ timeout: 15_000 });
  // Red today line.
  await expect(page.locator('.gv-scroll svg line[style*="e05252"]')).toBeAttached();
});

// ── team management ───────────────────────────────────────────────────────────

test('Manage Team link opens team management page', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
});

test('team page shows the current team name in the rename field', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();
  // The default name is "Team"; just assert the field is non-empty.
  const nameInput = page.locator('.kb-team-name-row input');
  await expect(nameInput).not.toHaveValue('');
});

test('team name can be renamed and the new name appears on the board', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();

  const nameInput = page.locator('.kb-team-name-row input');
  await nameInput.fill('Engineering');
  await page.locator('.kb-team-name-row .editor-btn').click();

  // Navigate back via the back-link and verify the board heading updated.
  await page.locator('.kb-back-link').click();
  await expect(page.locator('.kb-page-hdr h1')).toContainText('Engineering');
});

test('member can be added to the team', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Scope to the team block — the org-members section also has .kb-member-row for admin.
  const teamMemberRow = page.locator('.kb-team-block .kb-member-row', { hasText: 'admin' });

  // Remove admin from the team first so the "Add to team" button is guaranteed present.
  if (await teamMemberRow.isVisible()) {
    await teamMemberRow.locator('button', { hasText: 'Remove' }).click();
    await expect(teamMemberRow).not.toBeVisible();
  }

  // "Add to team" combo auto-selects admin (the only available org member).
  await page.locator('.kb-team-block .kb-member-add-row .editor-btn-cancel').click();
  await expect(teamMemberRow).toBeVisible();
});

test('added member appears in the Assigned to dropdown', async ({ page }) => {
  await loginAndGoToBoard(page);

  // Ensure admin is a team member.  Scope to .kb-team-block to avoid matching
  // the org-members section row which also contains 'admin'.
  await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  const teamMemberRow = page.locator('.kb-team-block .kb-member-row', { hasText: 'admin' });
  if (!(await teamMemberRow.isVisible())) {
    await page.locator('.kb-team-block .kb-member-add-row .editor-btn-cancel').click();
    await expect(teamMemberRow).toBeVisible();
  }
  await page.locator('.kb-back-link').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Open the task editor and check the assignee options.
  await page.locator('.kb-new-btn').click();
  const assigneeSelect = page.locator('.kb-editor-field-wrap')
    .filter({ hasText: 'Assigned to' }).locator('select');
  await expect(assigneeSelect.locator('option', { hasText: 'admin' })).toBeAttached();
});

test('member can be removed from the team', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Scope to .kb-team-block to avoid matching the org-members section row.
  const teamMemberRow = page.locator('.kb-team-block .kb-member-row', { hasText: 'admin' });

  // Ensure admin is on the team first so we have something to remove.
  if (!(await teamMemberRow.isVisible())) {
    await page.locator('.kb-team-block .kb-member-add-row .editor-btn-cancel').click();
    await expect(teamMemberRow).toBeVisible();
  }

  await teamMemberRow.locator('button', { hasText: 'Remove' }).click();
  await expect(teamMemberRow).not.toBeVisible();
});

test('team page back-to-board link returns to the board', async ({ page }) => {
  await loginAndGoToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('.kb-back-link').click();
  await expect(page.locator('.kb-page')).toBeVisible();
});
