import { test, expect } from '@playwright/test';

test('blog page shows heading', async ({ page }) => {
  await page.goto('/blog');
  await expect(page.locator('h1')).toContainText('Blog');
});

test('blog page lists posts from posts directory', async ({ page }) => {
  await page.goto('/blog');
  await expect(page.locator('.post-list')).toBeVisible();
  const count = await page.locator('.post-item').count();
  expect(count).toBeGreaterThanOrEqual(1);
});

test('post items have title links and dates', async ({ page }) => {
  await page.goto('/blog');
  const firstItem = page.locator('.post-item').first();
  await expect(firstItem.locator('.post-title')).toBeVisible();
  await expect(firstItem.locator('.post-date')).toBeVisible();
});

test('clicking a post title navigates to post page', async ({ page }) => {
  await page.goto('/blog');
  const firstTitle = page.locator('.post-title').first();
  const titleText = await firstTitle.textContent();
  await firstTitle.click();
  // Post page should show the title text somewhere
  await expect(page.locator('body')).toContainText(titleText ?? '');
});

test('tag chips are shown when post has tags', async ({ page }) => {
  await page.goto('/blog');
  // The fixture posts have tags, so at least one chip should appear
  await expect(page.locator('.tag-chip').first()).toBeVisible();
});

test('clicking a tag chip filters the list', async ({ page }) => {
  await page.goto('/blog');
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
  await page.goto('/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();

  // Navigate within the same Wt session via the nav link, then open a post
  await page.locator('.nav-link', { hasText: 'Blog' }).click();
  await page.locator('.post-title').first().click();
  await expect(page.locator('.post-edit-link')).toBeVisible();
});
