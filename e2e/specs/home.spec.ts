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
  await expect(page.locator('.nav-link', { hasText: 'Gantt' })).toBeVisible();
});

test('home page shows welcome text', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('h1')).toContainText('Hello');
  await expect(page.locator('text=Welcome to AltInf')).toBeVisible();
});

test('nav brand links back to home', async ({ page }) => {
  await page.goto('/?_=/blog');
  await page.locator('.nav-brand').click();
  await expect(page.locator('h1')).toContainText('Hello');
});
