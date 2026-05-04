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

async function openTaskEditorViaPath(page: Page, orgName: string, title: string) {
  await navigateToBoard(page, orgName);
  await page.locator('.kb-card', { hasText: title }).locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create org, team, and a task for conflict tests.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill('LiveEditorOrg');
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: /^LiveEditorOrg$/ })).toBeVisible();
  await page.locator('.org-list-link', { hasText: /^LiveEditorOrg$/ }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('input[placeholder="Team name"]').fill('EditorTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  // Create the conflict task.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('.org-list-link', { hasText: /^LiveEditorOrg$/ }).click();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('ConflictTask');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.close();
});

test('task editor: B saves a change → A sees conflict message and Save is disabled', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  // Both open the same task editor.
  await openTaskEditorViaPath(pageA, 'LiveEditorOrg', 'ConflictTask');
  await openTaskEditorViaPath(pageB, 'LiveEditorOrg', 'ConflictTask');

  // B saves a change.
  await pageB.locator('input[placeholder="Task title"]').fill('ConflictTaskEdited');
  await pageB.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(pageB.locator('.kb-board')).toBeVisible();

  // A should see the conflict message and Save should be disabled — without reloading.
  await expect(pageA.locator('.editor-status')).toContainText('modified by another user');
  await expect(
    pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')
  ).toBeDisabled();

  await ctxA.close();
  await ctxB.close();
});

test('task editor: B deletes the task → A sees conflict message and Save is disabled', async ({ browser }) => {
  // State: task is now titled 'ConflictTaskEdited' from previous test.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await openTaskEditorViaPath(pageA, 'LiveEditorOrg', 'ConflictTaskEdited');
  await openTaskEditorViaPath(pageB, 'LiveEditorOrg', 'ConflictTaskEdited');

  // B deletes the task.
  await pageB.locator('.editor-btn-danger').click();
  await expect(pageB.locator('.kb-board')).toBeVisible();

  // A's editor shows the conflict message and Save is disabled.
  await expect(pageA.locator('.editor-status')).toContainText('modified by another user');
  await expect(
    pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')
  ).toBeDisabled();

  await ctxA.close();
  await ctxB.close();
});
