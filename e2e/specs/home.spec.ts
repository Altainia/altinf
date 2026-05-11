import { test, expect } from '@playwright/test';

test('home page loads with correct title', async ({ page }) => {
  await page.goto('/');
  await expect(page).toHaveTitle(/AltInf/);
});

test('home page shows nav brand and links', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('.nav-brand')).toBeVisible();
  await expect(page.locator('.nav-link', { hasText: 'Home' })).toBeVisible();
  await expect(page.locator('.nav-link', { hasText: 'Blog' })).toBeVisible();
  await expect(page.locator('.nav-link', { hasText: 'Links' })).toBeVisible();
});

test('home page shows welcome text', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('h1')).toContainText("Hi, I'm Ben.");
  await expect(page.locator('text=software developer')).toBeVisible();
});

test('nav brand links back to home', async ({ page }) => {
  await page.goto('/blog');
  await page.locator('.nav-brand').click();
  await expect(page.locator('h1')).toContainText("Hi, I'm Ben.");
});

test('clicking Blog nav link produces a clean URL', async ({ page }) => {
  await page.goto('/');
  await page.locator('.nav-link', { hasText: 'Blog' }).click();
  await expect(page).toHaveURL('/blog');
});
