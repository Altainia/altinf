import { test, expect, type Page } from '@playwright/test';

async function loginAndGoToGantt(page: Page) {
  await page.goto('/?_=/login');
  await page.locator('input[placeholder="Username"]').fill('admin');
  await page.locator('input[placeholder="Password"]').fill('testpass');
  await page.locator('.login-btn').click();
  await expect(page.locator('.nav-logout')).toBeVisible();
  await page.locator('.nav-link', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gantt-list-page')).toBeVisible();
}

async function createChart(page: Page, title: string) {
  await page.locator('.gantt-new-btn').click();
  await expect(page.locator('.gantt-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Chart title"]').fill(title);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.gantt-view-page')).toBeVisible();
}

// Run list-page tests before any test creates a project so the empty state is valid

test('gantt list page shows heading', async ({ page }) => {
  await loginAndGoToGantt(page);
  await expect(page.locator('h1')).toContainText('Gantt Charts');
});

test('gantt list page shows empty state when no charts exist', async ({ page }) => {
  await loginAndGoToGantt(page);
  await expect(page.locator('.gantt-empty')).toBeVisible();
});

test('admin sees New Chart button', async ({ page }) => {
  await loginAndGoToGantt(page);
  await expect(page.locator('.gantt-new-btn')).toBeVisible();
});

test('New Chart button opens the editor', async ({ page }) => {
  await loginAndGoToGantt(page);
  await page.locator('.gantt-new-btn').click();
  await expect(page.locator('.gantt-editor-page')).toBeVisible();
});

test('editor shows title and description fields', async ({ page }) => {
  await loginAndGoToGantt(page);
  await page.locator('.gantt-new-btn').click();
  await expect(page.locator('input[placeholder="Chart title"]')).toBeVisible();
  await expect(page.locator('textarea[placeholder="Description (optional)"]')).toBeVisible();
});

test('saving without a title shows a validation error', async ({ page }) => {
  await loginAndGoToGantt(page);
  await page.locator('.gantt-new-btn').click();
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.editor-status')).toContainText('Title is required');
});

test('Add Task button appends a task row', async ({ page }) => {
  await loginAndGoToGantt(page);
  await page.locator('.gantt-new-btn').click();
  const before = await page.locator('.gantt-task-row').count();
  await page.locator('.gantt-add-btn').click();
  await expect(page.locator('.gantt-task-row')).toHaveCount(before + 1);
});

test('delete button removes a task row', async ({ page }) => {
  await loginAndGoToGantt(page);
  await page.locator('.gantt-new-btn').click();
  await page.locator('.gantt-add-btn').click();
  await expect(page.locator('.gantt-task-row')).toHaveCount(1);
  await page.locator('.gantt-del-btn').first().click();
  await expect(page.locator('.gantt-task-row')).toHaveCount(0);
});

test('creating a chart navigates to view page', async ({ page }) => {
  await loginAndGoToGantt(page);
  await createChart(page, 'E2E Chart Alpha');
  await expect(page.locator('.gantt-view-title')).toContainText('E2E Chart Alpha');
});

test('created chart appears in project list', async ({ page }) => {
  await loginAndGoToGantt(page);
  await createChart(page, 'E2E Chart Beta');
  await page.locator('.nav-link', { hasText: 'Gantt' }).click();
  await expect(page.locator('.gantt-project-list')).toBeVisible();
  await expect(page.locator('.gantt-project-title', { hasText: 'E2E Chart Beta' })).toBeVisible();
});

test('admin sees Edit Chart button on view page', async ({ page }) => {
  await loginAndGoToGantt(page);
  await createChart(page, 'E2E Chart Gamma');
  await expect(page.locator('.gantt-edit-btn')).toBeVisible();
});

test('Edit Chart button opens editor with prefilled title', async ({ page }) => {
  await loginAndGoToGantt(page);
  await createChart(page, 'E2E Chart Delta');
  await page.locator('.gantt-edit-btn').click();
  await expect(page.locator('.gantt-editor-page')).toBeVisible();
  await expect(page.locator('input[placeholder="Chart title"]')).toHaveValue('E2E Chart Delta');
});

test('cancel link in editor returns to project view', async ({ page }) => {
  await loginAndGoToGantt(page);
  await createChart(page, 'E2E Chart Epsilon');
  await page.locator('.gantt-edit-btn').click();
  await page.locator('.editor-btn-row .editor-btn-cancel').click();
  await expect(page.locator('.gantt-view-page')).toBeVisible();
});
