import { expect, type Page } from '@playwright/test';

// Wt's progressive bootstrap sends an initial POST (~66ms after page load)
// whose response can replace DOM elements including clearing the password field.
// Waiting for that POST to complete ensures our fills land on the stable,
// post-bootstrap DOM and the subsequent click POST will carry the password.
export async function loginAs(page: Page, username: string, password: string) {
  const bootstrapDone = page.waitForResponse(
    res => res.request().method() === 'POST' && res.status() === 200
  );
  await page.goto('/login');
  await bootstrapDone;
  await page.locator('input[placeholder="Username"]').fill(username);
  await page.locator('input[placeholder="Password"]').fill(password);
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
}
