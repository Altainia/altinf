import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG  = 'HistoryOrg';
const TEAM = 'HistoryTeam';

async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/login');
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}

async function goToBoard(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

async function goToManage(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();
}

/** Create a task with only a title and wait until the board is back. */
async function createTask(page: Page, title: string) {
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill(title);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

/** Open the edit editor for a task card. */
async function openTaskEditor(page: Page, title: string) {
  await page.locator('.kb-card', { hasText: title }).first().locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Create org — skip if already present (serial-mode retries can re-run beforeAll).
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  const alreadyExists = await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).isVisible();
  if (!alreadyExists) {
    await page.locator('input[placeholder="Organization name"]').fill(ORG);
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  }

  // Navigate to org manage page — use .first() in case of duplicates from retries.
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Create team — skip if already present.
  const teamExists = await page.locator('.kb-team-block:has(input[value="' + TEAM + '"])').isVisible();
  if (!teamExists) {
    await page.locator('input[placeholder="Team name"]').fill(TEAM);
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block:has(input[value="' + TEAM + '"])')).toBeVisible();
  }

  await page.close();
});

// ── Test 1: Create task, edit a field, verify history tab ───────────────────

test('history tab: title change appears in history with correct label and values', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToBoard(page);

  // Create the task.
  await createTask(page, 'HistoryTask1');

  // Open the editor and rename the task.
  await openTaskEditor(page, 'HistoryTask1');
  await page.locator('input[placeholder="Task title"]').fill('HistoryTask1 Renamed');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Open the editor for the renamed task.
  await openTaskEditor(page, 'HistoryTask1 Renamed');

  // Click the History tab — Wt renders it as an anchor inside a list item.
  await page.locator('.kb-editor-tabs').getByRole('link', { name: 'History' }).click();

  // The history panel should show at least one entry.
  await expect(page.locator('.kb-history-entry').first()).toBeVisible();
  // Find the entry that contains a title change line.
  const historyLine = page.locator('.kb-history-line', { hasText: 'Title:' });
  await expect(historyLine).toBeVisible();
  await expect(historyLine).toContainText('HistoryTask1');
  await expect(historyLine).toContainText('HistoryTask1 Renamed');

  await ctx.close();
});

// ── Test 2: Archive a task — disappears from board, appears on archive page ─

test('archive task: disappears from board and appears on archive page', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToBoard(page);

  await createTask(page, 'ArchiveMe');

  // Open the editor and click Archive.
  // (On retry there may be more than one ArchiveMe card; archive them all.)
  let cardCount = await page.locator('.kb-card', { hasText: 'ArchiveMe' }).count();
  while (cardCount > 0) {
    await openTaskEditor(page, 'ArchiveMe');
    await page.locator('.editor-btn-row .editor-btn-danger', { hasText: 'Archive' }).click();
    await expect(page.locator('.kb-board')).toBeVisible();
    cardCount = await page.locator('.kb-card', { hasText: 'ArchiveMe' }).count();
  }

  // Board must show no ArchiveMe cards.
  await expect(page.locator('.kb-card', { hasText: 'ArchiveMe' })).not.toBeVisible();

  // Navigate to the archive page via the "Archived" link in the board header.
  await page.locator('.kb-manage-link', { hasText: 'Archived' }).click();
  await expect(page.locator('.kb-archive-page')).toBeVisible();

  // The task must appear in the archive list.
  await expect(page.locator('.kb-archive-row .kb-archive-title', { hasText: 'ArchiveMe' }).first()).toBeVisible();

  await ctx.close();
});

// ── Test 3: Archive page tasks are read-only (Save absent, fields disabled) ─

test('archive page: archived task editor is read-only', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Navigate to the archive page.
  await goToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Archived' }).click();
  await expect(page.locator('.kb-archive-page')).toBeVisible();

  // Click on the archived task title link to open the editor (use .first() — retries may archive it twice).
  await page.locator('.kb-archive-title', { hasText: 'ArchiveMe' }).first().click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  // Save Changes button must be hidden for archived tasks.
  await expect(page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)', { hasText: 'Save Changes' })).not.toBeVisible();

  // Title input must be read-only (Wt renders readonly="readonly").
  await expect(page.locator('input[placeholder="Task title"]')).toHaveAttribute('readonly', 'readonly');

  // Status select must be disabled.
  const statusSelect = page.locator('.kb-editor-field-wrap').filter({ hasText: 'Status' }).locator('select');
  await expect(statusSelect).toBeDisabled();

  // The Unarchive button should be present (it replaces Archive for archived tasks).
  await expect(page.locator('.editor-btn', { hasText: 'Unarchive' })).toBeVisible();

  await ctx.close();
});

// ── Test 4: Unarchive — task returns to board and becomes editable ──────────

test('unarchive task: returns to board and is editable', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Navigate to archive and open the archived task.
  await goToBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Archived' }).click();
  await expect(page.locator('.kb-archive-page')).toBeVisible();
  await page.locator('.kb-archive-title', { hasText: 'ArchiveMe' }).first().click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  // Click Unarchive.
  await page.locator('.editor-btn', { hasText: 'Unarchive' }).click();

  // Should navigate back (to board or archive page).
  await expect(page.locator('.kb-board, .kb-archive-page')).toBeVisible();

  // Navigate to the board and verify the task is back.
  await goToBoard(page);
  await expect(page.locator('.kb-card', { hasText: 'ArchiveMe' })).toBeVisible();

  // Open the editor — Save Changes must be visible (not read-only).
  await openTaskEditor(page, 'ArchiveMe');
  await expect(
    page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')
  ).toBeVisible();
  // Title input must not be read-only.
  await expect(page.locator('input[placeholder="Task title"]')).not.toHaveAttribute('readonly', 'readonly');

  await ctx.close();
});

// ── Test 5: Archive a team — its tasks appear on the archive page ───────────

test('archive team: tasks from archived team appear on archive page', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Check current state of teams (retry-safe: team may already be archived).
  await goToManage(page);

  const teamIsActive = await page.locator('.kb-team-block:has(input[value="TeamToArchive"])').isVisible();
  const teamIsAlreadyArchived = await page.locator('.kb-archived-teams .kb-member-name', { hasText: 'TeamToArchive' }).isVisible();

  if (!teamIsActive && !teamIsAlreadyArchived) {
    // Team does not exist at all — create it, add a task, then archive it.
    await page.locator('input[placeholder="Team name"]').fill('TeamToArchive');
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block:has(input[value="TeamToArchive"])')).toBeVisible();
  }

  if (!teamIsAlreadyArchived) {
    // Navigate to the team's board and create a task there.
    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
    await page.locator('.org-team-link', { hasText: 'TeamToArchive' }).click();
    await expect(page.locator('.kb-board')).toBeVisible();
    await createTask(page, 'TeamArchiveTask');

    // Navigate to the manage page and archive the team.
    await page.locator('.kb-manage-link', { hasText: 'Manage Team' }).click();
    await expect(page.locator('.kb-team-page')).toBeVisible();
    await page.locator('.kb-team-block:has(input[value="TeamToArchive"])')
      .getByRole('button', { name: 'Archive' }).click();

    // The active team block must disappear.
    await expect(page.locator('.kb-team-block:has(input[value="TeamToArchive"])')).not.toBeVisible();
  }

  // The archived teams section must now list TeamToArchive.
  await goToManage(page);
  await expect(page.locator('.kb-archived-teams')).toBeVisible();
  await expect(page.locator('.kb-archived-teams .kb-member-name', { hasText: 'TeamToArchive' })).toBeVisible();

  await ctx.close();
});

// ── Test 6: Archived Teams section on team management page ──────────────────

test('archive team: Archived Teams section appears on management page', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Navigate to the manage page — the archived team from the previous test should be listed.
  await goToManage(page);
  await expect(page.locator('.kb-archived-teams')).toBeVisible();
  await expect(page.locator('.kb-archived-teams h2', { hasText: 'Archived Teams' })).toBeVisible();
  await expect(page.locator('.kb-archived-teams .kb-member-name', { hasText: 'TeamToArchive' })).toBeVisible();
  // Unarchive button must be present for each archived team.
  await expect(
    page.locator('.kb-archived-teams .kb-team-row', { hasText: 'TeamToArchive' })
      .getByRole('button', { name: 'Unarchive' })
  ).toBeVisible();

  await ctx.close();
});
