import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG = 'LiveLandingOrg';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function goToLanding(page: Page) {
  // Admin can use the Orgs admin link; non-admin members use the nav org link.
  const orgsLink = page.locator('.nav-link', { hasText: 'Orgs' });
  const hasOrgsLink = await orgsLink.isVisible().catch(() => false);
  if (hasOrgsLink) {
    await orgsLink.click();
    await expect(page.locator('.org-admin-page')).toBeVisible();
    await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
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

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create a fresh user 'frank' for landing page tests.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await page.locator('.account-new-btn').click();
  await page.locator('input[placeholder="Username (required)"]').fill('frank');
  await page.locator('input[placeholder="Password (required)"]').fill('frankpass');
  await page.locator('input[placeholder="Confirm password"]').fill('frankpass');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  // Create the org and invite frank.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('.kb-member-input').fill('frank');
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to frank');

  // Create a team.
  await page.locator('input[placeholder="Team name"]').fill('LandingTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block:has(input[value="LandingTeam"])')).toBeVisible();

  // Frank accepts the invite.
  const frankCtx = await browser.newContext();
  const frankPage = await frankCtx.newPage();
  await loginAs(frankPage, 'frank', 'frankpass');
  await frankPage.locator('.nav-bell-link').click();
  await expect(frankPage.locator('.notifications-page')).toBeVisible();
  await frankPage.getByRole('button', { name: 'Accept' }).click();
  await expect(frankPage.locator('.nav-bell-badge')).not.toBeVisible();
  await frankCtx.close();

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
  await expect(adminPage.locator('.kb-team-block:has(input[value="NewLandingTeam"])')).toBeVisible();

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

  // Rename LandingTeam to RenamedLandingTeam.
  const teamBlock = adminPage.locator('.kb-team-block:has(input[value="LandingTeam"])');
  await teamBlock.locator('input.editor-field').fill('RenamedLandingTeam');
  await teamBlock.getByRole('button', { name: 'Rename' }).click();

  await expect(frankPage.locator('.org-team-row', { hasText: 'RenamedLandingTeam' })).toBeVisible();
  await expect(frankPage.locator('.org-team-row').filter({ hasText: /^LandingTeam$/ })).not.toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});

test('org landing: team deleted by lead disappears from member\'s landing page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const frankCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const frankPage = await frankCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(frankPage, 'frank', 'frankpass');
  await goToManage(adminPage);
  await goToLanding(frankPage);

  await adminPage.locator('.kb-team-block:has(input[value="NewLandingTeam"])')
    .getByRole('button', { name: 'Delete team' }).click();

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

  const teamBlock = adminPage.locator('.kb-team-block:has(input[value="RenamedLandingTeam"])');
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

  await adminPage.locator('.kb-team-block:has(input[value="RenamedLandingTeam"])')
    .locator('.kb-member-row', { hasText: 'frank' })
    .getByRole('button', { name: 'Remove' }).click();

  await expect(frankPage.locator('.org-team-row--other', { hasText: 'RenamedLandingTeam' })).toBeVisible();

  await adminCtx.close();
  await frankCtx.close();
});
