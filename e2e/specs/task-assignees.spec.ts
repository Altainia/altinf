import { test, expect, type Page, type Browser } from '@playwright/test';
import { loginAs } from './helpers';

test.describe.configure({ mode: 'serial' });

const ORG    = 'AssigneeOrg';
const TEAM   = 'AssigneeTeam';
const USER_A = 'asgn_a';
const USER_B = 'asgn_b';
const PASS   = 'asgnpass';

let boardUrl = '';

// ── Helpers ───────────────────────────────────────────────────────────────────

async function ensureUser(page: Page, username: string) {
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
  const exists = await page.locator('.account-row', { hasText: username }).isVisible();
  if (exists) return;
  await page.locator('.account-new-btn').click();
  await expect(page.locator('.account-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Username (required)"]').fill(username);
  await page.locator('input[placeholder="Password (required)"]').fill(PASS);
  await page.locator('input[placeholder="Confirm password"]').fill(PASS);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
}

async function acceptPendingInvite(browser: Browser, username: string) {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, username, PASS);
  await page.locator('.nav-bell-link').click();
  await expect(page.locator('.notifications-page')).toBeVisible();
  const hasAccept = await page.getByRole('button', { name: 'Accept' }).isVisible();
  if (hasAccept) {
    await page.getByRole('button', { name: 'Accept' }).click();
    await expect(page.locator('.nav-bell-badge')).not.toBeVisible();
  }
  await ctx.close();
}

// ── beforeAll: idempotent setup ───────────────────────────────────────────────

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create test users.
  await ensureUser(page, USER_A);
  await ensureUser(page, USER_B);

  // Create or find the org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  const orgExists = await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).isVisible();
  if (!orgExists) {
    await page.locator('input[placeholder="Organization name"]').fill(ORG);
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  }

  // Navigate to org manage.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Invite USER_A if not already a member or pending.
  const userAMember  = await page.locator('.kb-members-container .kb-member-row', { hasText: USER_A }).isVisible();
  const userAPending = await page.locator('.kb-pending-row', { hasText: USER_A }).isVisible();
  if (!userAMember && !userAPending) {
    await page.locator('.kb-member-input').fill(USER_A);
    await page.getByRole('button', { name: 'Send invite' }).click();
    await expect(page.locator('.editor-status')).toContainText(`Invite sent to ${USER_A}`);
  }

  // Invite USER_B if not already a member or pending.
  const userBMember  = await page.locator('.kb-members-container .kb-member-row', { hasText: USER_B }).isVisible();
  const userBPending = await page.locator('.kb-pending-row', { hasText: USER_B }).isVisible();
  if (!userBMember && !userBPending) {
    await page.locator('.kb-member-input').fill(USER_B);
    await page.getByRole('button', { name: 'Send invite' }).click();
    await expect(page.locator('.editor-status')).toContainText(`Invite sent to ${USER_B}`);
  }

  // Users accept invites.
  await acceptPendingInvite(browser, USER_A);
  await acceptPendingInvite(browser, USER_B);

  // Navigate back to manage and refresh the page state.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Create team if not present.
  const teamExists = await page.locator(`.kb-team-block:has(.kb-team-name-label:text-is("${TEAM}"))`).isVisible();
  if (!teamExists) {
    await page.locator('input[placeholder="Team name"]').fill(TEAM);
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator(`.kb-team-block:has(.kb-team-name-label:text-is("${TEAM}"))`)).toBeVisible();
  }

  const teamBlock = page.locator(`.kb-team-block:has(.kb-team-name-label:text-is("${TEAM}"))`);

  // Add USER_A to team if not already present.
  const userAInTeam = await teamBlock.locator('.kb-member-row', { hasText: USER_A }).isVisible();
  if (!userAInTeam) {
    await teamBlock.locator('.gv-range-select').selectOption(USER_A);
    await teamBlock.getByRole('button', { name: 'Add to team' }).click();
    await expect(teamBlock.locator('.kb-member-row', { hasText: USER_A })).toBeVisible();
  }

  // Add USER_B to team if not already present.
  const userBInTeam = await teamBlock.locator('.kb-member-row', { hasText: USER_B }).isVisible();
  if (!userBInTeam) {
    await teamBlock.locator('.gv-range-select').selectOption(USER_B);
    await teamBlock.getByRole('button', { name: 'Add to team' }).click();
    await expect(teamBlock.locator('.kb-member-row', { hasText: USER_B })).toBeVisible();
  }

  // Navigate to the team board and capture the URL.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();
  boardUrl = page.url();

  await page.close();
});

// ── Bug 4: new task creation saves and appears on the board ───────────────────

test('bug4: creating a new task saves it and shows it on the board', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  await page.locator('input[placeholder="Task title"]').fill('Bug4Task');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();

  // Should navigate to the board (not stay on new task page).
  await expect(page.locator('.kb-board')).toBeVisible({ timeout: 5000 });
  await expect(page.locator('.kb-editor-page')).not.toBeVisible();

  // The new task must appear on the board.
  await expect(page.locator('.kb-card', { hasText: 'Bug4Task' })).toBeVisible();
});

// ── Bug 2: new task form has assignee combo + Add button ──────────────────────

test('bug2: new task form shows assignee combo and Add button', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  // The Assignees section must contain a select + Add button, not just the label.
  const assigneeSection = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Assignees' });
  await expect(assigneeSection).toBeVisible();
  await expect(assigneeSection.locator('.kb-assignee-add-row select')).toBeVisible();
  await expect(assigneeSection.locator('.kb-assignee-add-row button', { hasText: 'Add' })).toBeVisible();
});

// ── Bug 1a: adding a second assignee (popup) keeps the popup open ─────────────

test('bug1a: adding a second assignee in the popup keeps the popup open', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();

  // Create a fresh task for this test.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('Bug1aTask');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Open the popup.
  await page.locator('.kb-card', { hasText: 'Bug1aTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();

  // Add USER_A as the first assignee.
  const addRow = page.locator('.Wt-dialog.kb-task-popup .kb-assignee-add-row');
  await addRow.locator('select').selectOption(USER_A);
  await addRow.getByRole('button', { name: 'Add' }).click();
  // Wait for the chip to appear.
  await expect(page.locator('.Wt-dialog.kb-task-popup .kb-assignee-chip', { hasText: USER_A })).toBeVisible();
  // Popup must still be open.
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();

  // Now add USER_B as the second assignee (this is what triggers Bug 1).
  await addRow.locator('select').selectOption(USER_B);
  await addRow.getByRole('button', { name: 'Add' }).click();
  // Wait for the chip to appear.
  await expect(page.locator('.Wt-dialog.kb-task-popup .kb-assignee-chip', { hasText: USER_B })).toBeVisible();
  // Popup must still be open after the second assignment.
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
});

// ── Bug 1b: removing an assignee (popup) keeps the popup open ────────────────

test('bug1b: removing an assignee in the popup keeps the popup open', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();

  // Create a fresh task.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('Bug1bTask');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Open the popup and assign USER_A.
  await page.locator('.kb-card', { hasText: 'Bug1bTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  const addRow = page.locator('.Wt-dialog.kb-task-popup .kb-assignee-add-row');
  await addRow.locator('select').selectOption(USER_A);
  await addRow.getByRole('button', { name: 'Add' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup .kb-assignee-chip', { hasText: USER_A })).toBeVisible();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();

  // Remove USER_A (this is what triggers Bug 1b).
  const chip = page.locator('.Wt-dialog.kb-task-popup .kb-assignee-chip', { hasText: USER_A });
  await chip.locator('.kb-assignee-rm').click();
  // USER_A chip must be gone.
  await expect(page.locator('.Wt-dialog.kb-task-popup .kb-assignee-chip', { hasText: USER_A })).not.toBeVisible();
  // Popup must still be open after removing the assignee.
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
});

// ── Bug 3: add-assignee combo excludes already-assigned members ───────────────

test('bug3: add-assignee combo does not list already-assigned members', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible();

  // Create a fresh task.
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('Bug3Task');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Open the popup and assign USER_A.
  await page.locator('.kb-card', { hasText: 'Bug3Task' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  const addRow = page.locator('.Wt-dialog.kb-task-popup .kb-assignee-add-row');
  await addRow.locator('select').selectOption(USER_A);
  await addRow.getByRole('button', { name: 'Add' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup .kb-assignee-chip', { hasText: USER_A })).toBeVisible();

  // After assigning USER_A, the combo must not offer USER_A as an option.
  const options = await addRow.locator('select option').allTextContents();
  expect(options).not.toContain(USER_A);
  // USER_B must still be available.
  expect(options).toContain(USER_B);
});
