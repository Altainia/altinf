import { test, expect, type Page } from '@playwright/test';

test.describe.configure({ mode: 'serial' });

const ORG  = 'CommentOrg';
const TEAM = 'CommentTeam';

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

async function createTask(page: Page, title: string) {
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill(title);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

async function openTaskEditor(page: Page, title: string) {
  await page.locator('.kb-card', { hasText: title }).first().locator('.kb-card-edit').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

async function postComment(page: Page, body: string) {
  await page.locator('.kb-comment-compose textarea').pressSequentially(body);
  const postBtn = page.locator('.kb-comment-post-btn');
  await expect(postBtn).toBeEnabled();
  await postBtn.click();
  await expect(page.locator('.kb-comment-item').last()).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();

  const alreadyExists = await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).isVisible();
  if (!alreadyExists) {
    await page.locator('input[placeholder="Organization name"]').fill(ORG);
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  }

  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  const teamExists = await page.locator('.kb-team-block:has(input[value="' + TEAM + '"])').isVisible();
  if (!teamExists) {
    await page.locator('input[placeholder="Team name"]').fill(TEAM);
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block:has(input[value="' + TEAM + '"])')).toBeVisible();
  }

  await page.close();
});

// ── Test 1: Post a comment ────────────────────────────────────────────────────

test('comments: posting a comment renders it in the list', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToBoard(page);
  await createTask(page, 'CommentTask1');
  await openTaskEditor(page, 'CommentTask1');

  // Post button is disabled while textarea is empty
  await expect(page.locator('.kb-comment-post-btn')).toBeDisabled();

  // Type a comment and post
  await postComment(page, 'Hello **world**');

  // Comment appears in the list
  await expect(page.locator('.kb-comment-item')).toBeVisible();
  // Author label present
  await expect(page.locator('.kb-comment-author', { hasText: 'admin' })).toBeVisible();
  // Markdown rendered: **world** → <strong> (check the newly posted comment's body)
  const bodyHtml = await page.locator('.kb-comment-item').last().locator('.kb-comment-body').innerHTML();
  expect(bodyHtml).toContain('<strong>');

  // Textarea is cleared after posting
  await expect(page.locator('.kb-comment-compose textarea')).toHaveValue('');

  await ctx.close();
});

// ── Test 2: Edit own comment ──────────────────────────────────────────────────

test('comments: editing own comment updates body and shows "Edited by" label', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToBoard(page);
  await openTaskEditor(page, 'CommentTask1');

  // Click Edit on the first comment (no confirmation dialog for own comment)
  await page.locator('.kb-comment-item').first().locator('button', { hasText: 'Edit' }).click();

  // Inline textarea appears
  await expect(page.locator('.kb-comment-edit-area textarea')).toBeVisible();
  // .fill() is safe here — the Save button has no enabled/disabled state tied to keyWentUp
  await page.locator('.kb-comment-edit-area textarea').fill('Revised body');
  await page.locator('.kb-comment-edit-area button', { hasText: 'Save' }).click();

  // Updated body visible
  await expect(page.locator('.kb-comment-body').first()).toContainText('Revised body');
  // "Edited by" metadata
  await expect(page.locator('.kb-comment-meta').first()).toContainText('Edited by admin');

  await ctx.close();
});

// ── Test 3: Delete own comment ────────────────────────────────────────────────

test('comments: deleting own comment shows placeholder', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToBoard(page);
  await createTask(page, 'CommentTask2');
  await openTaskEditor(page, 'CommentTask2');
  await postComment(page, 'Delete me');

  await page.locator('.kb-comment-item').first().locator('button', { hasText: 'Delete' }).click();

  // Confirmation dialog — standard message (own comment, no author callout)
  await expect(page.locator('.Wt-dialog')).toBeVisible();
  await expect(page.locator('.Wt-dialog')).toContainText('Are you sure you want to delete this comment?');
  await expect(page.locator('.Wt-dialog')).not.toContainText('written by');
  await page.locator('.Wt-dialog .footer button', { hasText: 'Delete' }).click();

  // Placeholder appears; no non-deleted comment-item remains
  await expect(page.locator('.kb-comment-deleted').first()).toBeVisible();
  await expect(page.locator('.kb-comment-deleted').first()).toContainText('Comment deleted by admin');
  // Only deleted items should remain (no active .kb-comment-item without .kb-comment-deleted)
  await expect(page.locator('.kb-comment-item:not(.kb-comment-deleted)')).not.toBeVisible();

  await ctx.close();
});

// ── Test 4: Archived task — compose hidden, comments visible ─────────────────

test('comments: archived task hides compose area; existing comments remain visible', async ({ browser }) => {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'admin', 'testpass');
  await goToBoard(page);
  await createTask(page, 'CommentTaskArchive');
  await openTaskEditor(page, 'CommentTaskArchive');
  await postComment(page, 'Pre-archive comment');

  // Archive the task
  await page.locator('.editor-btn-danger', { hasText: 'Archive' }).click();
  await expect(page.locator('.kb-board')).toBeVisible();

  // Navigate to archive page and open the archived task
  await page.locator('.kb-manage-link', { hasText: 'Archived' }).click();
  await expect(page.locator('.kb-archive-page')).toBeVisible();
  await page.locator('.kb-archive-title', { hasText: 'CommentTaskArchive' }).first().click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();

  // Compose area hidden
  await expect(page.locator('.kb-comment-compose')).not.toBeVisible();
  // Comment still visible
  await expect(page.locator('.kb-comment-item')).toBeVisible();
  await expect(page.locator('.kb-comment-body').first()).toContainText('Pre-archive comment');

  await ctx.close();
});

// ── Test 5: Live update — add ─────────────────────────────────────────────────

test('comments: comment posted in session B appears in session A without reload', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  // Both open the same task editor
  await goToBoard(pageA);
  await createTask(pageA, 'LiveCommentTask');
  await openTaskEditor(pageA, 'LiveCommentTask');
  await goToBoard(pageB);
  await openTaskEditor(pageB, 'LiveCommentTask');

  // B posts a comment
  await postComment(pageB, 'From session B');

  // A sees the comment without reloading
  await expect(pageA.locator('.kb-comment-item', { hasText: 'From session B' })).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});

// ── Test 6: Live update — edit ────────────────────────────────────────────────

test('comments: comment edited in session B updates in session A without reload', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  // A posts a comment, then both open the same editor
  await goToBoard(pageA);
  await openTaskEditor(pageA, 'LiveCommentTask');
  await postComment(pageA, 'Original from A');

  await goToBoard(pageB);
  await openTaskEditor(pageB, 'LiveCommentTask');

  // B edits the comment — use .last() so a retry (with a leftover comment) picks the newest one
  const editItem = pageB.locator('.kb-comment-item', { hasText: 'Original from A' }).last();
  await editItem.locator('button', { hasText: 'Edit' }).click();
  // .fill() is safe here — the Save button has no enabled/disabled state tied to keyWentUp
  await editItem.locator('.kb-comment-edit-area textarea').fill('Edited by B');
  await editItem.locator('.kb-comment-edit-area button', { hasText: 'Save' }).click();

  // A sees the updated body and "Edited by" label without reloading
  await expect(pageA.locator('.kb-comment-body').last()).toContainText('Edited by B');
  await expect(pageA.locator('.kb-comment-meta').last()).toContainText('Edited by admin');

  await ctxA.close();
  await ctxB.close();
});

// ── Test 7: Live update — delete ─────────────────────────────────────────────

test('comments: comment deleted in session B shows placeholder in session A without reload', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await goToBoard(pageA);
  await openTaskEditor(pageA, 'LiveCommentTask');
  await goToBoard(pageB);
  await openTaskEditor(pageB, 'LiveCommentTask');

  // B deletes the comment that was edited by B in the previous test
  await pageB.locator('.kb-comment-item', { hasText: 'Edited by B' })
    .locator('button', { hasText: 'Delete' }).click();
  await pageB.locator('.Wt-dialog .footer button', { hasText: 'Delete' }).click();

  // A sees the placeholder without reloading
  await expect(pageA.locator('.kb-comment-deleted')).toBeVisible();

  await ctxA.close();
  await ctxB.close();
});
