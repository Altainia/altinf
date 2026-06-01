import { test, expect } from '@playwright/test';
import { loginAs } from './helpers';

// Run serially: the post-creation regression test below adds a post, which would
// otherwise race the "tag chip filters the list" test's before/after post counts
// when workers run in parallel.
test.describe.configure({ mode: 'serial' });

test('blog page shows heading', async ({ page }) => {
  await page.goto('/blog/list');
  await expect(page.locator('h1')).toContainText('Blog');
});

test('blog page lists posts from posts directory', async ({ page }) => {
  await page.goto('/blog/list');
  await expect(page.locator('.post-list')).toBeVisible();
  const count = await page.locator('.post-item').count();
  expect(count).toBeGreaterThanOrEqual(1);
});

test('post items have title links and dates', async ({ page }) => {
  await page.goto('/blog/list');
  const firstItem = page.locator('.post-item').first();
  await expect(firstItem.locator('.post-title')).toBeVisible();
  await expect(firstItem.locator('.post-date')).toBeVisible();
});

test('clicking a post title navigates to post page', async ({ page }) => {
  await page.goto('/blog/list');
  const firstTitle = page.locator('.post-title').first();
  const titleText = await firstTitle.textContent();
  await firstTitle.click();
  // Post page should show the title text somewhere
  await expect(page.locator('body')).toContainText(titleText ?? '');
});

test('tag chips are shown when post has tags', async ({ page }) => {
  await page.goto('/blog/list');
  // The fixture posts have tags, so at least one chip should appear
  await expect(page.locator('.tag-chip').first()).toBeVisible();
});

test('clicking a tag chip filters the list', async ({ page }) => {
  await page.goto('/blog/list');
  const totalBefore = await page.locator('.post-item').count();
  await page.locator('.tag-chip').first().click();
  // Filter bar should appear
  await expect(page.locator('.filter-bar')).toBeVisible();
  // Clear button restores full list
  await page.locator('.tag-chip.clear-chip').click();
  const totalAfter = await page.locator('.post-item').count();
  expect(totalAfter).toBe(totalBefore);
});

test('logged-in admin sees edit post link on post page', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');

  // Navigate within the same Wt session via the nav link, then open a post
  await page.locator('.nav-link', { hasText: 'Blog' }).click();
  await page.locator('.post-title').first().click();
  await expect(page.locator('.post-edit-link')).toBeVisible();
});

// Regression: the markdown editor's hidden value carrier used to be an <input>,
// which strips CR/LF via the HTML value-sanitization algorithm. Multi-paragraph
// posts collapsed into a single paragraph. The carrier is now a <textarea>, which
// preserves newlines — so two markdown paragraphs must render as two <p> elements.
test('creating a post preserves newlines between paragraphs', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');

  await page.goto('/blog/new');
  await expect(page.locator('.post-editor-page')).toBeVisible({ timeout: 20_000 });

  const stamp = 'nl-' + Date.now();
  await page.locator('input[placeholder="Title"]').fill('Newline Test ' + stamp);

  const pm = page.locator('.post-body-editor .ProseMirror[contenteditable="true"]')
    .filter({ visible: true }).first();
  await pm.click();
  await pm.type('First paragraph ' + stamp);
  await page.keyboard.press('Enter');
  await page.keyboard.press('Enter'); // blank line -> separate markdown paragraph
  await pm.type('Second paragraph ' + stamp);

  // Blur the editor so its focusout handler syncs the markdown back to Wt.
  await page.locator('input[placeholder="Title"]').click();
  await page.waitForTimeout(400);

  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();

  // Saved -> redirected to the rendered post page.
  await expect(page.locator('.blog-post-page')).toBeVisible({ timeout: 20_000 });
  const paragraphs = page.locator('.post-content p');
  // Two distinct paragraphs prove the newline survived (was 1 before the fix).
  expect(await paragraphs.count()).toBeGreaterThanOrEqual(2);
  await expect(page.locator('.post-content')).toContainText('First paragraph ' + stamp);
  await expect(page.locator('.post-content')).toContainText('Second paragraph ' + stamp);
});
