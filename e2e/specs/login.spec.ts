import { test, expect } from '@playwright/test';

test('login page renders form', async ({ page }) => {
  await page.goto('/?_=/login');
  await expect(page.locator('.login-form')).toBeVisible();
  await expect(page.locator('input[placeholder="Username"]')).toBeVisible();
  await expect(page.locator('input[placeholder="Password"]')).toBeVisible();
  await expect(page.locator('.login-btn')).toBeVisible();
});

test('wrong credentials shows error message', async ({ page }) => {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('wrongpassword');
  await page.locator('.login-btn').click();
  await expect(page.locator('.login-error')).toContainText('Invalid credentials');
});

test('unknown user shows error message', async ({ page }) => {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill('nobody');
  await page.locator('input[placeholder="Password"]').fill('whatever');
  await page.locator('.login-btn').click();
  await expect(page.locator('.login-error')).toContainText('Invalid credentials');
});

test('correct credentials logs in and shows logout link', async ({ page }) => {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
});

test('logged-in admin sees admin nav links', async ({ page }) => {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-link', { hasText: 'New Post' })).toBeVisible();
  await expect(page.locator('.nav-link', { hasText: 'Accounts' })).toBeVisible();
});

test('logout clears session', async ({ page }) => {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
  await page.locator('.nav-logout').click();
  await expect(page.locator('.nav-logout')).not.toBeVisible();
});

test('logged-out user sees login link in navbar', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('.nav-login')).toBeVisible();
  await expect(page.locator('.nav-login')).toHaveText('Login');
});

test('navbar login link navigates to login page', async ({ page }) => {
  await page.goto('/');
  await page.locator('.nav-login').click();
  await expect(page.locator('.login-form')).toBeVisible();
});

test('login page auto-focuses username field', async ({ page }) => {
  await page.goto('/?_=/login');
  await expect(page.locator('input[placeholder="Username"]')).toBeFocused();
});
