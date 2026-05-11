import { test, expect, type Page, type Browser } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG  = 'PermOrg';
const TEAM = 'PermTeam';

let boardUrl        = '';
let taskUrl         = '';
let archivedTaskUrl = '';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function createUser(page: Page, username: string, password: string) {
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
  await page.locator('.account-new-btn').click();
  await expect(page.locator('.account-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Username (required)"]').fill(username);
  await page.locator('input[placeholder="Password (required)"]').fill(password);
  await page.locator('input[placeholder="Confirm password"]').fill(password);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
}

async function acceptInvite(browser: Browser, username: string, password: string) {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, username, password);
  await page.locator('.nav-bell-link').click();
  await expect(page.locator('.notifications-page')).toBeVisible();
  await page.getByRole('button', { name: 'Accept' }).click();
  await expect(page.locator('.nav-bell-badge')).not.toBeVisible();
  await ctx.close();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create three users.
  await createUser(page, 'perm_member',  'permpass');
  await createUser(page, 'perm_orgonly', 'permpass');
  await createUser(page, 'perm_outsider', 'permpass');

  // Create PermOrg.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();

  // Navigate to org manage page.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Invite perm_member to org.
  await page.locator('.kb-member-input').fill('perm_member');
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to perm_member');

  // Invite perm_orgonly to org.
  await page.locator('.kb-member-input').fill('perm_orgonly');
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to perm_orgonly');

  // Create PermTeam.
  await page.locator('input[placeholder="Team name"]').fill(TEAM);
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block:has(input[value="' + TEAM + '"])')).toBeVisible();

  // Both users accept their org invites.
  await acceptInvite(browser, 'perm_member',  'permpass');
  await acceptInvite(browser, 'perm_orgonly', 'permpass');

  // Add perm_member to PermTeam as a non-lead member.
  // The manage page is already open; refresh it so the new member appears in the dropdown.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  const teamBlock = page.locator('.kb-team-block:has(input[value="' + TEAM + '"])');
  await teamBlock.locator('.gv-range-select').selectOption('perm_member');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();
  await expect(teamBlock.locator('.kb-member-row', { hasText: 'perm_member' })).toBeVisible();

  // Navigate to the team board to create tasks.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Capture boardUrl.
  boardUrl = page.url();

  // Create PermTask.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('PermTask');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Open PermTask editor and capture taskUrl.
  await page.locator('.kb-card', { hasText: 'PermTask' }).first().locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  taskUrl = page.url();
  // Return to board.
  await page.locator('.editor-btn-row .editor-btn-cancel').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Create PermArchived and archive it.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('PermArchived');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.locator('.kb-card', { hasText: 'PermArchived' }).first().locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  // Capture archivedTaskUrl before archiving.
  archivedTaskUrl = page.url();
  await page.locator('.editor-btn-row .editor-btn-danger', { hasText: 'Archive' }).click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.close();
});

// ── perm_member tests ─────────────────────────────────────────────────────────

test('perm_member: sees the board', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await ctx.close();
});

test('perm_member: no New Task button on board', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(page.locator('.kb-new-btn')).not.toBeVisible();
  await ctx.close();
});

test('perm_member: task fields are editable, save visible, no Archive button', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(taskUrl);
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await expect(page.locator('input[placeholder="Task title"]')).not.toHaveAttribute('readonly', 'readonly');
  await expect(page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')).toBeVisible();
  await expect(page.locator('.editor-btn-danger', { hasText: 'Archive' })).not.toBeVisible();
  await ctx.close();
});

test('perm_member: can save a title change and see updated card', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(taskUrl);
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('PermTask-Edited');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(page.locator('.kb-card', { hasText: 'PermTask-Edited' })).toBeVisible();
  await ctx.close();
});

test('perm_member: archived task URL returns not-found', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_member', 'permpass');
  await page.goto(archivedTaskUrl);
  await expect(page.locator('.kb-editor-page')).not.toBeVisible();
  await ctx.close();
});

// ── perm_orgonly tests ────────────────────────────────────────────────────────

test('perm_orgonly: sees the board', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await ctx.close();
});

test('perm_orgonly: no New Task or Manage Team buttons', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await expect(page.locator('.kb-new-btn')).not.toBeVisible();
  await expect(page.locator('.kb-manage-link', { hasText: 'Manage Team' })).not.toBeVisible();
  await ctx.close();
});

test('perm_orgonly: task fields read-only, no save button, no comment compose', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await page.goto(taskUrl);
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await expect(page.locator('input[placeholder="Task title"]')).toHaveAttribute('readonly', 'readonly');
  await expect(page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')).not.toBeVisible();
  await expect(page.locator('.kb-comment-compose')).not.toBeVisible();
  await ctx.close();
});

test('perm_orgonly: archived task URL returns not-found', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_orgonly', 'permpass');
  await page.goto(archivedTaskUrl);
  await expect(page.locator('.kb-editor-page')).not.toBeVisible();
  await ctx.close();
});

// ── perm_outsider tests ───────────────────────────────────────────────────────

test('perm_outsider: board URL is forbidden', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_outsider', 'permpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).not.toBeVisible();
  await ctx.close();
});

test('perm_outsider: task URL is forbidden', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'perm_outsider', 'permpass');
  await page.goto(taskUrl);
  await expect(page.locator('.kb-editor-page')).not.toBeVisible();
  await ctx.close();
});
