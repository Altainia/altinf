import { test, expect } from '@playwright/test';

test('links page shows heading', async ({ page }) => {
  await page.goto('/links');
  await expect(page.locator('h1')).toContainText('Links');
});

test('links page shows empty message when no links exist', async ({ page }) => {
  await page.goto('/links');
  await expect(page.locator('.links-empty')).toBeVisible();
});

test('logged-out user sees no Add Link button', async ({ page }) => {
  await page.goto('/links');
  await expect(page.locator('.link-add-btn')).not.toBeVisible();
});

test('logged-in admin sees Add Link button', async ({ page }) => {
  await page.goto('/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();

  // Navigate within the same Wt session via the nav link (page.goto resets the session)
  await page.locator('.nav-link', { hasText: 'Links' }).click();
  await expect(page.locator('.link-add-btn')).toBeVisible();
});

test('Add Link button navigates to link editor', async ({ page }) => {
  await page.goto('/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();

  await page.locator('.nav-link', { hasText: 'Links' }).click();
  await page.locator('.link-add-btn').click();
  // Link editor page should appear with a URL field
  await expect(page.locator('.link-editor-page')).toBeVisible();
});

test('notifications page redirects to login when not logged in', async ({ page }) => {
  await page.goto('/notifications');
  await expect(page.locator('.login-form')).toBeVisible();
});
