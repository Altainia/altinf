import { test, expect, type Page } from '@playwright/test';

// These tests exercise real-time server push: one browser session triggers an
// action that should update a second logged-in session without a page reload.
//
// All tests share one SQLite database and run serially so state (alice's
// account, orgs, notifications) accumulates predictably.
//
// IMPORTANT: navigation inside an active Wt session must use click links, not
// page.goto().  A full-page reload starts a new Wt session and loses the
// logged-in state.
test.describe.configure({ mode: 'serial' });

// ── helpers ───────────────────────────────────────────────────────────────────

async function loginAs(page: Page, username: string, password: string) {
  // page.goto() is fine here — it creates a fresh session that we log into.
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

// Navigate to the accounts list then create a new user.
// Must be called on a logged-in admin page (uses click-based navigation).
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

// Navigate to the org admin page and create an org with the given name.
async function createOrg(page: Page, orgName: string) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organisation name"]').fill(orgName);
  await page.locator('.org-create-form .editor-btn').click();
  // Use exact-match regex to avoid PushTestOrg matching PushTestOrg2.
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${orgName}$`) })).toBeVisible();
}

// From any logged-in page, navigate to the org manage page for orgName.
async function navigateToOrgManage(page: Page, orgName: string) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  // Exact-match regex prevents 'PushTestOrg' from also matching 'PushTestOrg2'.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${orgName}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.getByRole('link', { name: 'Manage organisation' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
}

// ── setup ─────────────────────────────────────────────────────────────────────

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');
  await createUser(page, 'alice', 'alicepass');
  await createOrg(page, 'PushTestOrg');
  await createOrg(page, 'PushTestOrg2');
  await page.close();
});

// ── tests ─────────────────────────────────────────────────────────────────────

test("bell badge appears on alice's active session without reload when admin sends an org invite", async ({ browser }) => {
  // Alice's session — no notifications yet, badge should be hidden.
  const aliceCtx = await browser.newContext();
  const alicePage = await aliceCtx.newPage();
  await loginAs(alicePage, 'alice', 'alicepass');
  await expect(alicePage.locator('.nav-bell-badge')).not.toBeVisible();

  // Admin's session — navigate to the org manage page and send an invite.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');
  await navigateToOrgManage(adminPage, 'PushTestOrg');
  await adminPage.locator('.kb-member-input').fill('alice');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to alice');

  // Server push should make alice's badge visible without any reload.
  await expect(alicePage.locator('.nav-bell-badge')).toBeVisible();

  await aliceCtx.close();
  await adminCtx.close();
});

test("notification row appears live in alice's open notifications page when admin sends another invite", async ({ browser }) => {
  // Alice has 1 unread notification from the previous test.
  const aliceCtx = await browser.newContext();
  const alicePage = await aliceCtx.newPage();
  await loginAs(alicePage, 'alice', 'alicepass');
  // Navigate to notifications via the bell link.
  await alicePage.locator('.nav-bell-link').click();
  await expect(alicePage.locator('.notifications-page')).toBeVisible();
  const rowsBefore = await alicePage.locator('.notif-row').count();

  // Admin invites alice to a second org from a separate session.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');
  await navigateToOrgManage(adminPage, 'PushTestOrg2');
  await adminPage.locator('.kb-member-input').fill('alice');
  await adminPage.getByRole('button', { name: 'Send invite' }).click();
  await expect(adminPage.locator('.editor-status')).toContainText('Invite sent to alice');

  // Alice's open notifications page should gain a new row via server push.
  await expect(alicePage.locator('.notif-row')).toHaveCount(rowsBefore + 1);

  await aliceCtx.close();
  await adminCtx.close();
});

test("rescinded invite updates existing notification text live on alice's open notifications page", async ({ browser }) => {
  // Alice has 2 unread notifications; the PushTestOrg2 invite is still pending.
  const aliceCtx = await browser.newContext();
  const alicePage = await aliceCtx.newPage();
  await loginAs(alicePage, 'alice', 'alicepass');
  await alicePage.locator('.nav-bell-link').click();
  await expect(alicePage.locator('.notifications-page')).toBeVisible();

  // Confirm the active invite text is present before the rescind.
  await expect(
    alicePage.locator('.notif-row').filter({ hasText: 'PushTestOrg2' })
  ).toContainText('invited to join');

  // Admin withdraws the PushTestOrg2 invite from a separate session.
  const adminCtx = await browser.newContext();
  const adminPage = await adminCtx.newPage();
  await loginAs(adminPage, 'admin', 'testpass');
  await navigateToOrgManage(adminPage, 'PushTestOrg2');
  // alice should appear in the Pending invites section.
  await adminPage
    .locator('.kb-member-row', { hasText: 'alice' })
    .getByRole('button', { name: 'Withdraw' })
    .click();

  // Server push should refresh alice's list; the row should show the rescind message.
  await expect(
    alicePage.locator('.notif-row').filter({ hasText: 'PushTestOrg2' })
  ).toContainText('has been rescinded');

  await aliceCtx.close();
  await adminCtx.close();
});

test("bell badge disappears after alice dismisses all unread notifications", async ({ browser }) => {
  // Alice has 2 unread notifications: active invite (PushTestOrg) + rescinded (PushTestOrg2).
  const aliceCtx = await browser.newContext();
  const alicePage = await aliceCtx.newPage();
  await loginAs(alicePage, 'alice', 'alicepass');

  // Badge should be visible on login (2 unread from previous tests).
  await expect(alicePage.locator('.nav-bell-badge')).toBeVisible();

  await alicePage.locator('.nav-bell-link').click();
  await expect(alicePage.locator('.notifications-page')).toBeVisible();

  // Acknowledge the rescinded invite (rescinded invites show an "Acknowledge" button).
  await alicePage.getByRole('button', { name: 'Acknowledge' }).click();
  // Wait for the full server response: badge drops to 1 AND the list is rebuilt.
  // Clicking Decline before the DOM refresh completes would hit a stale widget.
  await expect(alicePage.locator('.nav-bell-badge')).toHaveText('1');

  // Decline the still-active invite (has Accept + Decline).
  await alicePage.getByRole('button', { name: 'Decline' }).click();

  // Both notifications are now read — badge should hide.
  await expect(alicePage.locator('.nav-bell-badge')).not.toBeVisible();

  await aliceCtx.close();
});
