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
