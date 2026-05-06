import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG  = 'LiveTypesOrg';
const TEAM = 'TypesTeam';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function goToOrgLanding(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
}

async function goToTypeManager(page: Page) {
  await goToOrgLanding(page);
  await page.getByRole('link', { name: 'Manage types' }).click();
  await expect(page.locator('.org-type-manager-page')).toBeVisible();
}

async function goToBoard(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create the org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();

  // Navigate to manage page and create a team.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('input[placeholder="Team name"]').fill(TEAM);
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  await page.close();
});

test('type manager: org lead can navigate to the type manager page', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  await goToOrgLanding(page);
  await page.getByRole('link', { name: 'Manage types' }).click();

  await expect(page.locator('.org-type-manager-page')).toBeVisible();
  await expect(page.locator('h1')).toContainText('Task Types');

  await ctx.close();
});

test('type manager: create a type and see it in the list', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  await goToTypeManager(page);
  await page.locator('input[placeholder="Type name"]').fill('BugType');
  await page.getByRole('button', { name: 'Add Type' }).click();

  await expect(page.locator('.org-type-name', { hasText: 'BugType' })).toBeVisible();

  await ctx.close();
});

test('type manager: edit a type name and see the change', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  await goToTypeManager(page);
  // Click Edit on the BugType row.
  await page.locator('.org-type-row', { hasText: 'BugType' })
    .getByRole('button', { name: 'Edit' }).click();

  // The row should now have an input field — clear it and type the new name.
  const editInput = page.locator('.org-type-row input.editor-field');
  await editInput.clear();
  await editInput.fill('FeatureType');
  await page.locator('.org-type-row').getByRole('button', { name: 'Save' }).click();

  await expect(page.locator('.org-type-name', { hasText: 'FeatureType' })).toBeVisible();
  await expect(page.locator('.org-type-name', { hasText: 'BugType' })).not.toBeVisible();

  await ctx.close();
});

test('task editor: chip selector is visible when creating a new task', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  await goToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  await expect(page.locator('.kb-type-chips')).toBeVisible();

  await ctx.close();
});

test('task editor: assigning a type makes the board card non-gray', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  await goToBoard(page);
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  await page.locator('input[placeholder="Task title"]').fill('TypedTask');

  // Select the first available type chip (not the "None" chip).
  await page.locator('.kb-type-chip', { hasText: 'FeatureType' }).click();
  await expect(page.locator('.kb-type-chip.selected', { hasText: 'FeatureType' })).toBeVisible();

  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // The card's border-left-color should not be the gray fallback (#cccccc).
  await expect(page.locator('.kb-card', { hasText: 'TypedTask' })).toBeVisible();
  const borderColor = await page.locator('.kb-card', { hasText: 'TypedTask' })
    .evaluate((el) => (el as HTMLElement).style.borderLeftColor);
  expect(borderColor).not.toBe('rgb(204, 204, 204)');
  expect(borderColor).not.toBe('');

  await ctx.close();
});

test('type manager: delete type → card border-left-color becomes gray', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Delete the FeatureType.
  await goToTypeManager(page);
  await page.locator('.org-type-row', { hasText: 'FeatureType' })
    .getByRole('button', { name: 'Delete' }).click();
  await expect(page.locator('.org-type-name', { hasText: 'FeatureType' })).not.toBeVisible();

  // Navigate to the board — the card should fall back to gray.
  await goToBoard(page);

  await expect(page.locator('.kb-card', { hasText: 'TypedTask' })).toBeVisible();
  const borderColor = await page.locator('.kb-card', { hasText: 'TypedTask' })
    .evaluate((el) => (el as HTMLElement).style.borderLeftColor);
  expect(borderColor).toBe('rgb(204, 204, 204)');

  await ctx.close();
});
