import { test, expect, type Page, type Browser } from '@playwright/test';
import { loginAs } from './helpers';

test.describe.configure({ mode: 'serial' });

const ORG  = 'EditOrg';
const TEAM = 'EditTeam';

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

// Navigate to team board as admin (has the "Orgs" nav link).
async function gotoTeamBoardAdmin(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

// Navigate to team board as a member (uses the org-name link in the nav bar).
async function gotoTeamBoardMember(page: Page) {
  await page.locator('.nav-link.nav-org-link', { hasText: ORG }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  await createUser(page, 'edit_member', 'editpass');

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  await page.locator('.kb-member-input').fill('edit_member');
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to edit_member');

  await page.locator('input[placeholder="Team name"]').fill(TEAM);
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block:has(.kb-team-name-label:text-is("' + TEAM + '"))')).toBeVisible();

  await acceptInvite(browser, 'edit_member', 'editpass');

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  const teamBlock = page.locator('.kb-team-block:has(.kb-team-name-label:text-is("' + TEAM + '"))');
  await teamBlock.locator('.gv-range-select').selectOption('edit_member');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();
  await expect(teamBlock.locator('.kb-member-row', { hasText: 'edit_member' })).toBeVisible();

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('DetailTask');
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.close();
});

async function setEditDetails(page: Page, on: boolean) {
  await gotoTeamBoardAdmin(page);
  await page.locator('.kb-manage-link', { hasText: 'Settings' }).click();
  await expect(page.locator('.team-settings-page')).toBeVisible();
  const cb = page.getByRole('checkbox', { name: 'Members can edit task details' });
  if ((await cb.isChecked()) !== on) {
    await cb.click();
    await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  }
  await expect(cb).toBeChecked({ checked: on });
}

async function openDetailTaskEditor(page: Page) {
  await gotoTeamBoardMember(page);
  await page.locator('.kb-card', { hasText: 'DetailTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-popup-full-link').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

test('member cannot edit task title when the setting is off', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const admin    = await adminCtx.newPage();
  await loginAs(admin, 'admin', 'testpass');
  await setEditDetails(admin, false);
  await adminCtx.close();

  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'edit_member', 'editpass');
  await openDetailTaskEditor(page);

  await expect(page.locator('input[placeholder="Task title"]')).toHaveAttribute('readonly', 'readonly');
  await ctx.close();
});

test('member can edit task title when the setting is on', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const admin    = await adminCtx.newPage();
  await loginAs(admin, 'admin', 'testpass');
  await setEditDetails(admin, true);
  await adminCtx.close();

  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'edit_member', 'editpass');
  await openDetailTaskEditor(page);

  await expect(page.locator('input[placeholder="Task title"]')).not.toHaveAttribute('readonly', 'readonly');
  await ctx.close();
});
