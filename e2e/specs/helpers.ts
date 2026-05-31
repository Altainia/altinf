import { expect, type Page } from '@playwright/test';

// Wt bootstraps the login page progressively: an initial POST (~tens of ms after
// load) can replace DOM nodes and clear the input fields. In Firefox this timing is
// looser, so a naive fill+submit sometimes submits empty credentials and login
// silently fails (the form re-renders and `.nav-logout` never appears).
//
// To be robust across browsers we fill, verify the values actually stuck on the
// post-bootstrap DOM, then submit — retrying the whole sequence until the
// authenticated nav appears.
export async function loginAs(page: Page, username: string, password: string) {
  await page.goto('/login');

  const userField = page.locator('input[placeholder="Username"]');
  const passField = page.locator('input[placeholder="Password"]');
  const nav = page.locator('.nav-logout');

  for (let attempt = 0; attempt < 4; attempt++) {
    // Already authenticated (e.g. nav arrived slowly after a prior click)? Done.
    if (await nav.isVisible().catch(() => false)) return;

    // Wait for the login form; it may still be bootstrapping.
    try {
      await userField.waitFor({ state: 'visible', timeout: 10_000 });
    } catch {
      if (await nav.isVisible().catch(() => false)) return;
      continue;
    }

    await userField.fill(username);
    await passField.fill(password);

    // A late bootstrap response can clear the fields; confirm before submitting.
    const uv = await userField.inputValue().catch(() => '');
    const pv = await passField.inputValue().catch(() => '');
    if (uv !== username || pv !== password) continue; // refill next iteration

    await page.locator('.login-btn').click();

    try {
      await expect(nav).toBeVisible({ timeout: 8_000 });
      return;
    } catch {
      // Submit raced the bootstrap or the fields were cleared; loop and retry.
    }
  }

  // Final assertion gives a clear failure message if every attempt failed.
  await expect(nav).toBeVisible();
}
