import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG = 'LiveManageOrg';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function goToManage(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create a second user 'bob' for the org tests.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
  await page.locator('.account-new-btn').click();
  await expect(page.locator('.account-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Username (required)"]').fill('bob');
  await page.locator('input[placeholder="Password (required)"]').fill('bobpass');
  await page.locator('input[placeholder="Confirm password"]').fill('bobpass');
  // Give bob org_create so he can be an org lead.
  await page.locator('label', { hasText: 'Create Orgs' }).click();
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  // Create the org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();

  // Invite bob as a lead so he can also navigate to the manage page.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
  await page.locator('.kb-member-input').fill('bob');
  await page.locator('.kb-lead-check').click();
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to bob');

  // Accept the invite as bob.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await bobPage.locator('.nav-bell-link').click();
  await expect(bobPage.locator('.notifications-page')).toBeVisible();
  await bobPage.getByRole('button', { name: 'Accept' }).click();
  await expect(bobPage.locator('.nav-bell-badge')).not.toBeVisible();
  await bobCtx.close();

  // Create a team for the org.
  await goToManage(page);
  await page.locator('input[placeholder="Team name"]').fill('ManageTeam');
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block')).toBeVisible();

  await page.close();
});

test('org manage: accept invite adds member on second lead\'s page', async ({ browser }) => {
  // Create a third user 'carol' and invite her.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');
  await adminPage.locator('.nav-link', { hasText: 'Accounts' }).click();
  await adminPage.locator('.account-new-btn').click();
  await adminPage.locator('input[placeholder="Username (required)"]').fill('carol');
  await adminPage.locator('input[placeholder="Password (required)"]').fill('carolpass');
  await adminPage.locator('input[placeholder="Confirm password"]').fill('carolpass');
  await adminPage.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(adminPage.locator('.account-manager-page')).toBeVisible();
  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('carol');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to carol');

  // Bob is also watching the manage page.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await goToManage(bobPage);
  // Carol accepts the invite.
  const carolCtx = await browser.newContext();
  const carolPage = await carolCtx.newPage();
  await loginAs(carolPage, 'carol', 'carolpass');
  await carolPage.locator('.nav-bell-link').click();
  await expect(carolPage.locator('.notifications-page')).toBeVisible();
  await carolPage.getByRole('button', { name: 'Accept' }).click();
  // Wait for accept to complete (bell badge disappears means server acknowledged).
  await expect(carolPage.locator('.nav-bell-badge')).not.toBeVisible();

  // Bob's manage page shows carol in Members (members section) without reload.
  await expect(
    bobPage.locator('.kb-members-container').first().locator('.kb-member-row', { hasText: 'carol' })
  ).toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
  await carolCtx.close();
});

test('org manage: decline invite removes pending row on second lead\'s page', async ({ browser }) => {
  // Invite a new user 'dave' and have bob watch the pending section.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');

  // Create dave.
  await adminPage.locator('.nav-link', { hasText: 'Accounts' }).click();
  await adminPage.locator('.account-new-btn').click();
  await adminPage.locator('input[placeholder="Username (required)"]').fill('dave');
  await adminPage.locator('input[placeholder="Password (required)"]').fill('davepass');
  await adminPage.locator('input[placeholder="Confirm password"]').fill('davepass');
  await adminPage.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(adminPage.locator('.account-manager-page')).toBeVisible();

  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('dave');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to dave');

  // Bob watches the manage page.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await goToManage(bobPage);
  await expect(bobPage.locator('.kb-members-container ~ .kb-members-container .kb-member-row', { hasText: 'dave' })).toBeVisible();

  // Dave declines.
  const daveCtx = await browser.newContext();
  const davePage = await daveCtx.newPage();
  await loginAs(davePage, 'dave', 'davepass');
  await davePage.locator('.nav-bell-link').click();
  await davePage.getByRole('button', { name: 'Decline' }).click();

  // Bob's pending section no longer shows dave.
  await expect(bobPage.locator('.kb-members-container ~ .kb-members-container .kb-member-row', { hasText: 'dave' })).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
  await daveCtx.close();
});

test('org manage: withdraw invite updates second lead\'s pending section', async ({ browser }) => {
  // Create a new user 'eve' and invite her.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');

  await adminPage.locator('.nav-link', { hasText: 'Accounts' }).click();
  await adminPage.locator('.account-new-btn').click();
  await adminPage.locator('input[placeholder="Username (required)"]').fill('eve');
  await adminPage.locator('input[placeholder="Password (required)"]').fill('evepass');
  await adminPage.locator('input[placeholder="Confirm password"]').fill('evepass');
  await adminPage.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(adminPage.locator('.account-manager-page')).toBeVisible();

  // Bob watches the manage page.
  const bobCtx = await browser.newContext();
  const bobPage = await bobCtx.newPage();
  await loginAs(bobPage, 'bob', 'bobpass');
  await goToManage(bobPage);

  // Admin invites eve from the manage page.
  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('eve');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to eve');
  // Bob sees the pending invite.
  await expect(bobPage.locator('.kb-members-container ~ .kb-members-container .kb-member-row', { hasText: 'eve' })).toBeVisible();

  // Admin withdraws the invite.
  await adminPage.locator('.kb-members-container ~ .kb-members-container .kb-member-row', { hasText: 'eve' })
    .getByRole('button', { name: 'Withdraw' }).click();

  // Bob's pending section no longer shows eve.
  await expect(bobPage.locator('.kb-members-container ~ .kb-members-container .kb-member-row', { hasText: 'eve' })).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: promote to org lead updates second lead\'s members section', async ({ browser }) => {
  // carol is a member. Admin promotes her; bob should see "(lead)" appear.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
    .getByRole('button', { name: 'Make lead' }).click();

  await expect(
    bobPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
  ).toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: demote from org lead updates second lead\'s members section', async ({ browser }) => {
  // carol is now a lead. Admin demotes; bob should see "(lead)" disappear.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
    .getByRole('button', { name: 'Demote' }).click();

  await expect(
    bobPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
  ).not.toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: remove member updates second lead\'s members section', async ({ browser }) => {
  // carol is a plain member. Admin removes her; bob should not see carol anymore.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
    .getByRole('button', { name: 'Remove' }).click();

  await expect(
    bobPage.locator('.kb-members-container .kb-member-row', { hasText: 'carol' })
  ).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: create team appears on second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('input[placeholder="Team name"]').fill('NewLiveTeam');
  await adminPage.getByRole('button', { name: 'Create' }).click();
  await expect(adminPage.locator('.kb-team-block input[value="NewLiveTeam"]')).toBeVisible();

  await expect(bobPage.locator('.kb-team-block input[value="NewLiveTeam"]')).toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: rename team updates second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  const teamBlock = adminPage.locator('.kb-team-block:has(input[value="NewLiveTeam"])');
  await teamBlock.locator('input.editor-field').fill('RenamedLiveTeam');
  await teamBlock.getByRole('button', { name: 'Rename' }).click();

  await expect(bobPage.locator('.kb-team-block:has(input[value="RenamedLiveTeam"])')).toBeVisible();
  await expect(bobPage.locator('.kb-team-block:has(input[value="NewLiveTeam"])')).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: archive team disappears from second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-team-block:has(input[value="RenamedLiveTeam"])')
    .getByRole('button', { name: 'Archive team' }).click();

  await expect(bobPage.locator('.kb-team-block:has(input[value="RenamedLiveTeam"])')).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: add member to team updates second lead\'s page', async ({ browser }) => {
  // bob is already a member of the org. Add bob to ManageTeam.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  const teamBlock = adminPage.locator('.kb-team-block:has(input[value="ManageTeam"])');
  await teamBlock.locator('.gv-range-select').selectOption('bob');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();

  await expect(
    bobPage.locator('.kb-team-block:has(input[value="ManageTeam"])').locator('.kb-member-row', { hasText: 'bob' })
  ).toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: remove member from team updates second lead\'s page', async ({ browser }) => {
  // bob is on ManageTeam from previous test. Admin removes him.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-team-block:has(input[value="ManageTeam"])')
    .locator('.kb-member-row', { hasText: 'bob' })
    .getByRole('button', { name: 'Remove' }).click();

  await expect(
    bobPage.locator('.kb-team-block:has(input[value="ManageTeam"])').locator('.kb-member-row', { hasText: 'bob' })
  ).not.toBeVisible();

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: promote team lead updates second lead\'s page', async ({ browser }) => {
  // Add bob back to ManageTeam first, then promote.
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  const teamBlock = adminPage.locator('.kb-team-block:has(input[value="ManageTeam"])');
  await teamBlock.locator('.gv-range-select').selectOption('bob');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();
  await expect(adminPage.locator('.kb-team-block:has(input[value="ManageTeam"])').locator('.kb-member-row', { hasText: 'bob' })).toBeVisible();

  await teamBlock.locator('.kb-member-row', { hasText: 'bob' })
    .getByRole('button', { name: 'Make lead' }).click();

  await expect(
    bobPage.locator('.kb-team-block:has(input[value="ManageTeam"])').locator('.kb-member-row', { hasText: 'bob' })
  ).toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: demote team lead updates second lead\'s page', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const bobCtx   = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const bobPage   = await bobCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(bobPage,   'bob',   'bobpass');
  await goToManage(adminPage);
  await goToManage(bobPage);

  await adminPage.locator('.kb-team-block:has(input[value="ManageTeam"])')
    .locator('.kb-member-row', { hasText: 'bob' })
    .getByRole('button', { name: 'Remove lead' }).click();

  await expect(
    bobPage.locator('.kb-team-block:has(input[value="ManageTeam"])').locator('.kb-member-row', { hasText: 'bob' })
  ).not.toContainText('(lead)');

  await adminCtx.close();
  await bobCtx.close();
});

test('org manage: pressing Enter in invite input sends invite', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create a fresh user 'hank' for this test.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await page.locator('.account-new-btn').click();
  await page.locator('input[placeholder="Username (required)"]').fill('hank');
  await page.locator('input[placeholder="Password (required)"]').fill('hankpass');
  await page.locator('input[placeholder="Confirm password"]').fill('hankpass');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  await goToManage(page);
  await page.locator('.kb-member-input').fill('hank');
  await page.locator('.kb-member-input').press('Enter');
  await expect(page.locator('.editor-status')).toContainText('Invite sent to hank');

  await ctx.close();
});

test('org manage: pressing Enter in team name input creates team', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToManage(page);

  await page.locator('input[placeholder="Team name"]').fill('EnterKeyTeam');
  await page.locator('input[placeholder="Team name"]').press('Enter');
  await expect(page.locator('.kb-team-block input[value="EnterKeyTeam"]')).toBeVisible();

  await ctx.close();
});
