import { test, expect, type Page } from '@playwright/test';
import { loginAs } from './helpers';

test.describe.configure({ mode: 'serial' });

const ORG = 'LiveLandingOrg';

async function goToLanding(page: Page) {
  // Admin can use the Orgs admin link; non-admin members use the nav org link.
  const orgsLink = page.locator('.nav-link', { hasText: 'Orgs' });
  const hasOrgsLink = await orgsLink.isVisible().catch(() => false);
  if (hasOrgsLink) {
    await orgsLink.click();
    await expect(page.locator('.org-admin-page')).toBeVisible();
    await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  } else {
    // Regular member: use the org name link in the nav bar.
    await page.locator('.nav-org-link', { hasText: ORG }).click();
  }
  await expect(page.locator('.org-landing-page')).toBeVisible();
}

async function goToManage(page: Page) {
  await goToLanding(page);
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
}

// Create a task on the currently-open board with an optional end date, assigned to
// `assignee`, then wait for the board to come back.
async function createTaskAssigned(page: Page, title: string, endDate: string, assignee: string) {
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill(title);
  if (endDate) {
    const endInput = page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
    await endInput.fill(endDate);
    await endInput.press('Tab');
  } else {
    // New-task editor only pre-fills the start date; leave End date empty.
    const endInput = page.locator('.kb-editor-field-wrap').filter({ hasText: 'End date' }).locator('input').first();
    await endInput.fill('');
    await endInput.press('Tab');
  }
  const assigneeSection = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Assignees' });
  await assigneeSection.locator('.kb-assignee-add-row select').selectOption(assignee);
  await assigneeSection.locator('.kb-assignee-add-row button', { hasText: 'Add' }).click();
  await expect(assigneeSection.locator('.kb-assignee-chip', { hasText: assignee })).toBeVisible();
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create user 'frank' — skip if already exists (idempotent for re-runs).
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
  const frankExists = await page.locator('.account-row', { hasText: 'frank' }).isVisible();
  if (!frankExists) {
    await page.locator('.account-new-btn').click();
    await page.locator('input[placeholder="Username (required)"]').fill('frank');
    await page.locator('input[placeholder="Password (required)"]').fill('frankpass');
    await page.locator('input[placeholder="Confirm password"]').fill('frankpass');
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
    await expect(page.locator('.account-manager-page')).toBeVisible();
  }

  // Create the org — skip if already present.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  const orgExists = await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).isVisible();
  if (!orgExists) {
    await page.locator('input[placeholder="Organization name"]').fill(ORG);
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  }

  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Invite frank — skip if already a member or pending.
  const frankIsMember = await page.locator('.kb-members-container .kb-member-row', { hasText: 'frank' }).isVisible();
  const frankHasPending = await page.locator('.kb-pending-row', { hasText: 'frank' }).isVisible();
  if (!frankIsMember && !frankHasPending) {
    await page.locator('.kb-member-input').fill('frank');
    await page.getByRole('button', { name: 'Send invite' }).click();
    await expect(page.locator('.editor-status')).toContainText('Invite sent to frank');

    // Frank accepts the invite.
    const frankCtx = await browser.newContext();
    const frankPage = await frankCtx.newPage();
    await loginAs(frankPage, 'frank', 'frankpass');
    await frankPage.locator('.nav-bell-link').click();
    await expect(frankPage.locator('.notifications-page')).toBeVisible();
    await frankPage.getByRole('button', { name: 'Accept' }).click();
    await expect(frankPage.locator('.nav-bell-badge')).not.toBeVisible();
    await frankCtx.close();
  }

  // Create a team — skip if already present.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  const teamExists = await page.locator('.kb-team-block:has(.kb-team-name-label:text-is("LandingTeam"))').isVisible();
  if (!teamExists) {
    await page.locator('input[placeholder="Team name"]').fill('LandingTeam');
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block:has(.kb-team-name-label:text-is("LandingTeam"))')).toBeVisible();
  }

  await page.close();
});

test('org landing: team created by lead appears on member\'s landing page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  await adminPage.locator('input[placeholder="Team name"]').fill('NewLandingTeam');
  await adminPage.getByRole('button', { name: 'Create' }).click();
  await expect(adminPage.locator('.kb-team-block:has(.kb-team-name-label:text-is("NewLandingTeam"))')).toBeVisible();

  // Frank's landing page gains the team in "Other teams" (he's not a member yet).
  await expect(frankPage.locator('.org-team-row--other', { hasText: 'NewLandingTeam' })).toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: team renamed by lead updates member\'s landing page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  // Rename LandingTeam to RenamedLandingTeam via team settings page.
  await adminPage.locator('.kb-team-block:has(.kb-team-name-label:text-is("LandingTeam"))')
    .locator('.kb-team-settings-link').click();
  await expect(adminPage.locator('.team-settings-page')).toBeVisible();
  await adminPage.locator('input.editor-field').fill('RenamedLandingTeam');
  await adminPage.getByRole('button', { name: 'Save' }).click();
  await goToManage(adminPage);

  await expect(frankPage.locator('.org-team-row', { hasText: 'RenamedLandingTeam' })).toBeVisible();
  await expect(frankPage.locator('.org-team-row').filter({ hasText: /^LandingTeam$/ })).not.toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: team archived by lead disappears from member\'s landing page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  await adminPage.locator('.kb-team-block:has(.kb-team-name-label:text-is("NewLandingTeam"))')
    .getByRole('button', { name: 'Archive' }).click();

  await expect(frankPage.locator('.org-team-row', { hasText: 'NewLandingTeam' })).not.toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: added to team moves row from Other to Your teams on member\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  // Frank is not in RenamedLandingTeam — it should be in "Other teams".
  await expect(frankPage.locator('.org-team-row--other', { hasText: 'RenamedLandingTeam' })).toBeVisible();

  const teamBlock = adminPage.locator('.kb-team-block:has(.kb-team-name-label:text-is("RenamedLandingTeam"))');
  await teamBlock.locator('.gv-range-select').selectOption('frank');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();

  // Frank's row moves to "Your teams" (no --other class).
  await expect(frankPage.locator('.org-team-row:not(.org-team-row--other)', { hasText: 'RenamedLandingTeam' })).toBeVisible();
  await expect(frankPage.locator('.org-team-row--other', { hasText: 'RenamedLandingTeam' })).not.toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: removed from team moves row back to Other teams', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  await adminPage.locator('.kb-team-block:has(.kb-team-name-label:text-is("RenamedLandingTeam"))')
    .locator('.kb-member-row', { hasText: 'frank' })
    .getByRole('button', { name: 'Remove' }).click();

  await expect(frankPage.locator('.org-team-row--other', { hasText: 'RenamedLandingTeam' })).toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: "Your tasks" shows an empty note when the user has no assignments', async ({ browser }) => {
  const frankCtx = await browser.newContext();
  const frankPage = await frankCtx.newPage();

  await loginAs(frankPage, 'frank', 'frankpass');
  await goToLanding(frankPage);

  await expect(frankPage.getByRole('heading', { name: 'Your tasks' })).toBeVisible();
  await expect(frankPage.locator('.org-empty-note', { hasText: 'no assigned tasks' })).toBeVisible();

  await frankCtx.close();
});

test('org landing: assigned tasks appear grouped by team, sorted by end date (no end date last)', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');

  // Frank must be a team member to be assignable — add him to RenamedLandingTeam.
  await goToManage(adminPage);
  const teamBlock = adminPage.locator('.kb-team-block:has(.kb-team-name-label:text-is("RenamedLandingTeam"))');
  const frankInTeam = await teamBlock.locator('.kb-member-row', { hasText: 'frank' }).isVisible();
  if (!frankInTeam) {
    await teamBlock.locator('.gv-range-select').selectOption('frank');
    await teamBlock.getByRole('button', { name: 'Add to team' }).click();
    await expect(teamBlock.locator('.kb-member-row', { hasText: 'frank' })).toBeVisible();
  }

  // Open the team board and create three tasks assigned to frank, out of date order
  // and including one with no end date.
  await goToLanding(adminPage);
  await adminPage.locator('.org-team-link', { hasText: 'RenamedLandingTeam' }).click();
  await expect(adminPage.locator('.kb-board')).toBeVisible();
  await createTaskAssigned(adminPage, 'LateTask', '2030-03-01', 'frank');
  await createTaskAssigned(adminPage, 'NoEndTask', '', 'frank');
  await createTaskAssigned(adminPage, 'EarlyTask', '2030-01-15', 'frank');

  // Frank's landing page lists them under the team group, earliest end date first,
  // no end date last.
  await goToLanding(frankPage);
  await expect(frankPage.locator('.org-tasks-team', { hasText: 'RenamedLandingTeam' })).toBeVisible();
  const rows = frankPage.locator('.org-task-row');
  await expect(rows).toHaveCount(3);
  await expect(rows.nth(0)).toContainText('EarlyTask');
  await expect(rows.nth(1)).toContainText('LateTask');
  await expect(rows.nth(2)).toContainText('NoEndTask');
  await expect(rows.nth(2).locator('.org-task-due')).toContainText('No end date');
  await expect(rows.nth(0)).toHaveAttribute('href', /\/task\/edit\/\d+/);

  await adminCtx.close();
  await frankCtx.close();
});
