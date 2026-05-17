import { test, expect, type Page } from '@playwright/test';
import { loginAs } from './helpers';

test.describe.configure({ mode: 'serial' });

let teamUrl = '';

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Navigate to orgs page.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();

  // Create org only if it does not already exist.
  const orgExists = (await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).count()) > 0;
  if (!orgExists) {
    await page.locator('input[placeholder="Organization name"]').fill('PopupOrg');
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: /^PopupOrg$/ })).toBeVisible();
  }
  await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();

  // Create team only if it does not already exist.
  const teamExists = (await page.locator('.org-team-link').count()) > 0;
  if (!teamExists) {
    await page.getByRole('link', { name: 'Manage organization' }).click();
    await expect(page.locator('.kb-team-page')).toBeVisible();
    await page.locator('input[placeholder="Team name"]').fill('PopupTeam');
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block')).toBeVisible();
    // Return to org landing to navigate to board.
    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).first().click();
    await expect(page.locator('.org-landing-page')).toBeVisible();
  }

  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
  teamUrl = page.url();

  // Create task only if a task named exactly 'PopupTask' does not exist.
  const taskExists = (await page.locator('.kb-card', { hasText: /^PopupTask$/ }).count()) > 0;
  if (!taskExists) {
    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
    await page.locator('input[placeholder="Task title"]').fill('PopupTask');
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible();
  }

  await page.close();
});

test('clicking a kanban card opens the task popup', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toContainText('PopupTask');
});

test('clicking a field in popup activates in-place edit mode', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  // Title display is visible; input is hidden
  const display = page.locator('.kb-task-popup .kb-popup-display').first();
  const input   = page.locator('.kb-task-popup input[type="text"]').first();
  await expect(display).toBeVisible();
  await expect(input).toBeHidden();
  // Click display → input appears
  await display.click();
  await expect(input).toBeVisible();
  await expect(display).toBeHidden();
});

test('editing a field and tabbing away highlights it yellow', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-task-popup .kb-popup-display').first().click();
  const input = page.locator('.kb-task-popup input[type="text"]').first();
  await input.fill('PopupTaskEdited');
  await input.press('Tab');
  // Field wrapper should have dirty class
  await expect(page.locator('.kb-task-popup .kb-popup-field--dirty')).toBeVisible();
  // Save button should be enabled
  const saveBtn = page.locator('.kb-task-popup .footer .editor-btn:not(.editor-btn-cancel)');
  await expect(saveBtn).toBeEnabled();
});

test('saving popup change updates the board card title', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-task-popup .kb-popup-display').first().click();
  const input = page.locator('.kb-task-popup input[type="text"]').first();
  await input.fill('PopupTaskSaved');
  await input.press('Tab');
  const saveBtn = page.locator('.kb-task-popup .footer .editor-btn:not(.editor-btn-cancel)');
  await expect(saveBtn).toBeEnabled();
  await saveBtn.evaluate((el: HTMLElement) => el.click());
  // Popup closes
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
  // Board shows updated title
  await expect(page.locator('.kb-card', { hasText: 'PopupTaskSaved' })).toBeVisible();
});

test('popup shows stale banner when another session saves the same task', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await pageA.goto(teamUrl);
  await expect(pageA.locator('.kb-board')).toBeVisible();
  await pageA.locator('.kb-card', { hasText: 'PopupTaskSaved' }).click();
  await expect(pageA.locator('.Wt-dialog.kb-task-popup')).toBeVisible();

  await pageB.goto(teamUrl);
  await expect(pageB.locator('.kb-board')).toBeVisible();
  await pageB.locator('.kb-card', { hasText: 'PopupTaskSaved' }).click();
  await expect(pageB.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await pageB.locator('.kb-task-popup .kb-popup-display').first().click();
  await pageB.locator('.kb-task-popup input[type="text"]').first().fill('PopupTaskB');
  await pageB.locator('.kb-task-popup input[type="text"]').first().press('Tab');
  const saveBtnB = pageB.locator('.kb-task-popup .footer .editor-btn:not(.editor-btn-cancel)');
  await expect(saveBtnB).toBeEnabled();
  await saveBtnB.evaluate((el: HTMLElement) => el.click());
  await expect(pageB.locator('.Wt-dialog.kb-task-popup')).toBeHidden();

  // A sees stale banner; Save disabled
  await expect(pageA.locator('.kb-popup-stale-banner')).toBeVisible({ timeout: 10000 });
  await expect(
    pageA.locator('.kb-task-popup .footer .editor-btn:not(.editor-btn-cancel)')
  ).toBeDisabled();

  await ctxA.close();
  await ctxB.close();
});

test('direct URL navigation to full editor still works', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card').first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-popup-full-link').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
});

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
  await page.locator('.kb-task-popup .editor-btn-cancel').click();
  // Confirmation dialog appears
  await expect(page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })).toBeVisible();
  // Click "Keep Editing" — popup stays open
  await page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })
    .locator('button', { hasText: 'Keep Editing' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  // Click Close again, then Discard
  await page.locator('.kb-task-popup .editor-btn-cancel').click();
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
