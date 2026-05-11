import { test, expect, type Page } from '@playwright/test';
import { loginAs } from './helpers';

test.describe.configure({ mode: 'serial' });

let teamUrl = '';

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill('PopupOrg');
  await page.locator('.org-create-form .editor-btn').click();
  await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await page.locator('input[placeholder="Team name"]').fill('PopupTeam');
  await page.getByRole('button', { name: 'Create' }).click();

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).click();
  await page.locator('.org-team-link').first().click();
  teamUrl = page.url();

  await page.locator('.kb-new-btn').click();
  await page.locator('input[placeholder="Task title"]').fill('PopupTask');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.close();
});

test('clicking a kanban card opens the task popup', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).locator('.kb-card-edit').click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toContainText('PopupTask');
});

test('clicking a field in popup activates in-place edit mode', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).locator('.kb-card-edit').click();
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
  await page.locator('.kb-card', { hasText: 'PopupTask' }).locator('.kb-card-edit').click();
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
  await page.locator('.kb-card', { hasText: 'PopupTask' }).locator('.kb-card-edit').click();
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
  await pageA.locator('.kb-card', { hasText: 'PopupTaskSaved' }).locator('.kb-card-edit').click();
  await expect(pageA.locator('.Wt-dialog.kb-task-popup')).toBeVisible();

  await pageB.goto(teamUrl);
  await expect(pageB.locator('.kb-board')).toBeVisible();
  await pageB.locator('.kb-card', { hasText: 'PopupTaskSaved' }).locator('.kb-card-edit').click();
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
  await page.locator('.kb-card').first().locator('.kb-card-edit').click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-popup-full-link').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
});
