import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function navigateToBoard(page: Page, orgName: string) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${orgName}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

async function navigateToGantt(page: Page, orgName: string) {
  await navigateToBoard(page, orgName);
  await page.locator('.kb-tab', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gv-wrap')).toBeVisible();
}

async function openTaskEditor(page: Page, title: string) {
  await page.locator('.kb-card', { hasText: title }).locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

async function saveTaskEditor(page: Page) {
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill('LiveBoardOrg');
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: /^LiveBoardOrg$/ })).toBeVisible();
  await page.locator('.org-list-link', { hasText: /^LiveBoardOrg$/ }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('input[placeholder="Team name"]').fill('LiveBoardTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  await page.close();
});

test('board: task created by A appears on B without reload', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToBoard(pageB, 'LiveBoardOrg');

  await pageA.locator('.kb-new-btn').click();
  await expect(pageA.locator('.kb-editor-page')).toBeVisible();
  await pageA.locator('input[placeholder="Task title"]').fill('LiveNewTask');
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  await expect(pageB.locator('.kb-card', { hasText: 'LiveNewTask' })).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('board: task title edited by A updates on B', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToBoard(pageB, 'LiveBoardOrg');

  await openTaskEditor(pageA, 'LiveNewTask');
  await pageA.locator('input[placeholder="Task title"]').fill('LiveRenamedTask');
  await saveTaskEditor(pageA);

  await expect(pageB.locator('.kb-card', { hasText: 'LiveRenamedTask' })).toBeVisible();
  await expect(pageB.locator('.kb-card', { hasText: 'LiveNewTask' })).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('board: task deleted by A disappears from B', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToBoard(pageB, 'LiveBoardOrg');

  await openTaskEditor(pageA, 'LiveRenamedTask');
  await pageA.locator('.editor-btn-danger').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  await expect(pageB.locator('.kb-card', { hasText: 'LiveRenamedTask' })).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('board: drag-drop by A moves card on B', async ({ browser }) => {
  const setup = await browser.newContext();
  const setupPage = await setup.newPage();
  await loginAs(setupPage, 'admin', 'testpass');
  await navigateToBoard(setupPage, 'LiveBoardOrg');
  await setupPage.locator('.kb-new-btn').click();
  await expect(setupPage.locator('.kb-editor-page')).toBeVisible();
  await setupPage.locator('input[placeholder="Task title"]').fill('DragDropTask');
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
  await navigateToBoard(pageB, 'LiveBoardOrg');

  const card   = pageA.locator('.kb-column[data-status="todo"] .kb-card', { hasText: 'DragDropTask' });
  const inProg = pageA.locator('.kb-column[data-status="in_progress"]');
  await card.dragTo(inProg);

  await expect(
    pageB.locator('.kb-column[data-status="in_progress"] .kb-card', { hasText: 'DragDropTask' })
  ).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('gantt: task created with dates by A appears on B gantt view', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToGantt(pageB, 'LiveBoardOrg');

  await pageA.locator('.kb-new-btn').click();
  await expect(pageA.locator('.kb-editor-page')).toBeVisible();
  await pageA.locator('input[placeholder="Task title"]').fill('GanttTask');
  const startInput = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
  const endInput   = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
  await startInput.fill('2026-06-01');
  await startInput.press('Tab');
  await endInput.fill('2026-06-15');
  await endInput.press('Tab');
  await pageA.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  await expect(pageB.locator('.gv-scroll svg text', { hasText: 'GanttTask' })).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('gantt: task deleted by A disappears from B gantt view', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');
  await navigateToGantt(pageB, 'LiveBoardOrg');

  await openTaskEditor(pageA, 'GanttTask');
  await pageA.locator('.editor-btn-danger').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  await expect(pageB.locator('.gv-scroll svg text', { hasText: 'GanttTask' })).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('gantt: date change by A updates bar on B gantt view', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');
  await navigateToBoard(pageA, 'LiveBoardOrg');

  // A creates DateTask with a short initial date range
  await pageA.locator('.kb-new-btn').click();
  await expect(pageA.locator('.kb-editor-page')).toBeVisible();
  await pageA.locator('input[placeholder="Task title"]').fill('DateTask');
  const startInput = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'Start date' }).locator('input').first();
  const endInput   = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
  await startInput.fill('2025-03-01');
  await startInput.press('Tab');
  await endInput.fill('2025-03-02');
  await endInput.press('Tab');
  await pageA.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(pageA.locator('.kb-board')).toBeVisible();

  // B navigates to the Gantt and sees DateTask
  await navigateToGantt(pageB, 'LiveBoardOrg');
  await expect(pageB.locator('.gv-scroll svg text', { hasText: 'DateTask' })).toBeVisible();

  // Snapshot B's current SVG markup before A's edit — we'll verify it changes.
  const svgBefore = await pageB.locator('.gv-scroll svg').evaluate(el => el.outerHTML);

  // A edits DateTask and extends the end date to widen the bar
  await openTaskEditor(pageA, 'DateTask');
  const endInputEdit = pageA.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
  await endInputEdit.fill('2025-03-31');
  await endInputEdit.press('Tab');
  await pageA.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await saveTaskEditor(pageA);

  // B's Gantt must re-render via live push: wait until the SVG markup changes.
  await pageB.waitForFunction(
    (expected) => document.querySelector('.gv-scroll svg')?.outerHTML !== expected,
    svgBefore,
    { timeout: 15000 },
  );
  // DateTask is still present in the re-rendered Gantt (bar widened to 30 days).
  await expect(pageB.locator('.gv-scroll svg text', { hasText: 'DateTask' })).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});
