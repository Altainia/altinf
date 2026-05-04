import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG = 'LiveNavOrg';

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

  // Create user 'grace'.
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await page.locator('.account-new-btn').click();
  await page.locator('input[placeholder="Username (required)"]').fill('grace');
  await page.locator('input[placeholder="Password (required)"]').fill('gracepass');
  await page.locator('input[placeholder="Confirm password"]').fill('gracepass');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();

  // Create org.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();

  await page.close();
});

test('nav bar: accepting invite adds org to selector without navigating', async ({ browser }) => {
  // Admin invites grace.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');
  await goToManage(adminPage);
  await adminPage.locator('.kb-member-input').fill('grace');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to grace');
  await adminCtx.close();

  // Grace is on the home page. Her org area should not yet include LiveNavOrg.
  const graceCtx = await browser.newContext();
  const gracePage = await graceCtx.newPage();
  await loginAs(gracePage, 'grace', 'gracepass');
  await expect(gracePage.locator('.nav-org-area')).not.toBeVisible();

  // Grace accepts the invite from the notifications page.
  await gracePage.locator('.nav-bell-link').click();
  await expect(gracePage.locator('.notifications-page')).toBeVisible();
  await gracePage.getByRole('button', { name: 'Accept' }).click();

  // The org area should now include LiveNavOrg (server push via user channel).
  await expect(gracePage.locator('.nav-org-area')).toContainText(ORG);

  await graceCtx.close();
});

test('nav bar: removed from org removes org from selector without navigating', async ({ browser }) => {
  // Grace is a member of LiveNavOrg. Admin removes her.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');

  // Grace is on the home page with LiveNavOrg visible in her org area.
  const graceCtx = await browser.newContext();
  const gracePage = await graceCtx.newPage();
  await loginAs(gracePage, 'grace', 'gracepass');
  await expect(gracePage.locator('.nav-org-area')).toContainText(ORG);

  await goToManage(adminPage);
  await adminPage.locator('.kb-members-container .kb-member-row', { hasText: 'grace' })
    .getByRole('button', { name: 'Remove' }).click();

  // Grace's nav bar no longer shows LiveNavOrg.
  await expect(gracePage.locator('.nav-org-area')).not.toBeVisible();

  await adminCtx.close();
  await graceCtx.close();
});

test('account manager: new user created by A appears on B\'s accounts page', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  // Both navigate to accounts.
  await pageA.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();
  await pageB.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageB.locator('.account-manager-page')).toBeVisible();

  // A creates a new user.
  await pageA.locator('.account-new-btn').click();
  await expect(pageA.locator('.account-editor-page')).toBeVisible();
  await pageA.locator('input[placeholder="Username (required)"]').fill('newliveuser');
  await pageA.locator('input[placeholder="Password (required)"]').fill('pass');
  await pageA.locator('input[placeholder="Confirm password"]').fill('pass');
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();

  // B's accounts page shows the new user without reload.
  await expect(pageB.locator('.account-table td', { hasText: 'newliveuser' })).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('account manager: user deleted by A disappears from B\'s accounts page', async ({ browser }) => {
  // State: 'newliveuser' exists from previous test.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await pageA.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();
  await pageB.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageB.locator('.account-manager-page')).toBeVisible();

  // A deletes 'newliveuser'.
  const row = pageA.locator('.account-table tr', { hasText: 'newliveuser' });
  await row.getByRole('button', { name: 'Delete' }).click();
  // Wait for the confirmation dialog to render, then confirm.
  await expect(pageA.locator('h4', { hasText: 'Confirm Delete' })).toBeVisible();
  await pageA.locator('.Wt-dialog .footer .editor-btn:not(.editor-btn-cancel)').click();
  // Wait for A's page to reflect the deletion before checking B.
  await expect(pageA.locator('.account-table td', { hasText: 'newliveuser' })).not.toBeVisible({ timeout: 15000 });

  // B's accounts page no longer shows 'newliveuser'.
  await expect(pageB.locator('.account-table td', { hasText: 'newliveuser' })).not.toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

test('account manager: user permissions edited by A update on B\'s accounts page', async ({ browser }) => {
  // State: 'grace' exists. Admin edits her permissions; B should see the new label.
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await pageA.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();
  await pageB.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(pageB.locator('.account-manager-page')).toBeVisible();

  // A navigates to grace's edit page and adds the "Create Orgs" permission.
  const graceRow = pageA.locator('.account-table tr', { hasText: 'grace' });
  await graceRow.locator('.link-action-link', { hasText: 'Edit' }).click();
  await expect(pageA.locator('.account-editor-page')).toBeVisible();
  await pageA.locator('label', { hasText: 'Create Orgs' }).click();
  await pageA.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(pageA.locator('.account-manager-page')).toBeVisible();

  // B sees the updated permissions label for grace.
  await expect(
    pageB.locator('.account-table tr', { hasText: 'grace' }).locator('td').nth(2)
  ).toContainText('Create Orgs');

  await ctxA.close();
  await ctxB.close();
});

test('bell regression: invite sent to grace increments bell badge on her active session', async ({ browser }) => {
  // Admin creates a fresh org and invites grace. Grace's bell should increment.
  const adminCtx = await browser.newContext();
  const graceCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  const gracePage = await graceCtx.newPage();

  await loginAs(adminPage, 'admin', 'testpass');
  await loginAs(gracePage, 'grace', 'gracepass');

  // Grace may have accumulated notifications from earlier tests (e.g. org_removed).
  // Clear them to reach a known zero-badge state before testing the increment.
  await gracePage.locator('.nav-bell-link').click();
  await expect(gracePage.locator('.notifications-page')).toBeVisible();
  for (let i = await gracePage.getByRole('button', { name: 'Dismiss' }).count(); i > 0; i--) {
    await gracePage.getByRole('button', { name: 'Dismiss' }).first().click();
  }
  await gracePage.locator('.nav-link', { hasText: 'Home' }).click();
  await expect(gracePage.locator('.nav-bell-badge')).not.toBeVisible();

  // Admin creates a new org and invites grace.
  await adminPage.locator('.nav-link', { hasText: 'Orgs' }).click();
  await adminPage.locator('input[placeholder="Organization name"]').fill('BellRegressionOrg');
  await adminPage.locator('.org-create-form .editor-btn').click();
  await expect(adminPage.locator('.org-list-link', { hasText: /^BellRegressionOrg$/ })).toBeVisible();
  await adminPage.locator('.org-list-link', { hasText: /^BellRegressionOrg$/ }).click();
  await adminPage.getByRole('link', { name: 'Manage organization' }).click();
  await expect(adminPage.locator('.kb-team-page')).toBeVisible();
  await adminPage.locator('.kb-member-input').fill('grace');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to grace');

  // Grace's bell badge appears.
  await expect(gracePage.locator('.nav-bell-badge')).toBeVisible();

  await adminCtx.close();
  await graceCtx.close();
});
