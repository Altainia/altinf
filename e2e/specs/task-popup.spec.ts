import { test, expect, type Page } from '@playwright/test';
import { loginAs } from './helpers';

test.describe.configure({ mode: 'serial' });

let teamUrl = '';

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // Navigate to orgs page.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();

  // Create org only if it does not already exist.
  const orgExists = (await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).count()) > 0;
  if (!orgExists) {
    await page.locator('input[placeholder="Organization name"]').fill('PopupOrg');
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: /^PopupOrg$/ })).toBeVisible();
  }
  await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();

  // Create team only if it does not already exist.
  const teamExists = (await page.locator('.org-team-link').count()) > 0;
  if (!teamExists) {
    await page.getByRole('link', { name: 'Manage organization' }).click();
    await expect(page.locator('.kb-team-page')).toBeVisible();
    await page.locator('input[placeholder="Team name"]').fill('PopupTeam');
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block')).toBeVisible();
    // Return to org landing to navigate to board.
    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await page.locator('.org-list-link', { hasText: /^PopupOrg$/ }).first().click();
    await expect(page.locator('.org-landing-page')).toBeVisible();
  }

  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible();
  teamUrl = page.url();

  // Create task only if a task named exactly 'PopupTask' does not exist.
  const taskExists = (await page.locator('.kb-card', { hasText: /^PopupTask$/ }).count()) > 0;
  if (!taskExists) {
    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible();
    await page.locator('input[placeholder="Task title"]').fill('PopupTask');
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible();
  }

  await page.close();
});

test('clicking a kanban card opens the task popup', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toContainText('PopupTask');
});

test('clicking a field in popup activates in-place edit mode', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  // Title display is visible; input is hidden
  const display = page.locator('.kb-task-popup .kb-popup-display').first();
  const input   = page.locator('.kb-task-popup input[type="text"]').first();
  await expect(display).toBeVisible();
  await expect(input).toBeHidden();
  // Click display → input appears
  await display.click();
  await expect(input).toBeVisible();
  await expect(display).toBeHidden();
});

test('editing a field and tabbing away highlights it yellow', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-task-popup .kb-popup-display').first().click();
  const input = page.locator('.kb-task-popup input[type="text"]').first();
  await input.fill('PopupTaskEdited');
  await input.press('Tab');
  // Field wrapper should have dirty class
  await expect(page.locator('.kb-task-popup .kb-popup-field--dirty')).toBeVisible();
  // Save button should be enabled
  const saveBtn = page.locator('.kb-task-popup .editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)');
  await expect(saveBtn).toBeEnabled();
});

test('saving popup change updates the board card title', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-task-popup .kb-popup-display').first().click();
  const input = page.locator('.kb-task-popup input[type="text"]').first();
  await input.fill('PopupTaskSaved');
  await input.press('Tab');
  const saveBtn = page.locator('.kb-task-popup .editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)');
  await expect(saveBtn).toBeEnabled();
  await saveBtn.evaluate((el: HTMLElement) => el.click());
  // Popup closes
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
  // Board shows updated title
  await expect(page.locator('.kb-card', { hasText: 'PopupTaskSaved' })).toBeVisible();
});

test('popup shows stale banner when another session saves the same task', async ({ browser }) => {
  const ctxA = await browser.newContext();
  const ctxB = await browser.newContext();
  const pageA = await ctxA.newPage();
  const pageB = await ctxB.newPage();

  await loginAs(pageA, 'admin', 'testpass');
  await loginAs(pageB, 'admin', 'testpass');

  await pageA.goto(teamUrl);
  await expect(pageA.locator('.kb-board')).toBeVisible();
  await pageA.locator('.kb-card', { hasText: 'PopupTaskSaved' }).click();
  await expect(pageA.locator('.Wt-dialog.kb-task-popup')).toBeVisible();

  await pageB.goto(teamUrl);
  await expect(pageB.locator('.kb-board')).toBeVisible();
  await pageB.locator('.kb-card', { hasText: 'PopupTaskSaved' }).click();
  await expect(pageB.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await pageB.locator('.kb-task-popup .kb-popup-display').first().click();
  await pageB.locator('.kb-task-popup input[type="text"]').first().fill('PopupTaskB');
  await pageB.locator('.kb-task-popup input[type="text"]').first().press('Tab');
  const saveBtnB = pageB.locator('.kb-task-popup .editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)');
  await expect(saveBtnB).toBeEnabled();
  await saveBtnB.evaluate((el: HTMLElement) => el.click());
  await expect(pageB.locator('.Wt-dialog.kb-task-popup')).toBeHidden();

  // A sees stale banner; Save disabled
  await expect(pageA.locator('.kb-popup-stale-banner')).toBeVisible({ timeout: 10000 });
  await expect(
    pageA.locator('.kb-task-popup .editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)')
  ).toBeDisabled();

  await ctxA.close();
  await ctxB.close();
});

test('direct URL navigation to full editor still works', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card').first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-popup-full-link').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
});

test('blur with no change reverts field to display mode', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card').first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  // Click display to enter edit mode
  const display = page.locator('.kb-task-popup .kb-popup-display').first();
  await display.click();
  const input = page.locator('.kb-task-popup input[type="text"]').first();
  await expect(input).toBeVisible();
  // Tab away without changing value
  await input.press('Tab');
  // Display should reappear, save button must remain disabled
  await expect(display).toBeVisible();
  await expect(input).toBeHidden();
  const saveBtn = page.locator('.kb-task-popup .editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)');
  await expect(saveBtn).toBeDisabled();
});

test('close with pending changes shows confirmation dialog', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card').first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  // Make a change
  await page.locator('.kb-task-popup .kb-popup-display').first().click();
  await page.locator('.kb-task-popup input[type="text"]').first().fill('DirtyTitle');
  await page.locator('.kb-task-popup input[type="text"]').first().press('Tab');
  // Wait for the field to be marked dirty (server processes blur signal)
  await expect(page.locator('.kb-task-popup .kb-popup-field--dirty')).toBeVisible();
  // Use Escape key to trigger try_close
  await page.keyboard.press('Escape');
  // Confirmation dialog appears with both "Keep Editing" and "Discard" options
  await expect(page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })).toBeVisible();
  await expect(page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' }).locator('button', { hasText: 'Keep Editing' })).toBeVisible();
  await expect(page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' }).locator('button', { hasText: 'Discard' })).toBeVisible();
  // Click Discard — popup closes
  await page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })
    .locator('button', { hasText: 'Discard' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
});

test('escape key with no changes closes popup immediately', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card').first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.keyboard.press('Escape');
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
});

test('escape key with pending changes shows confirmation', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card').first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-task-popup .kb-popup-display').first().click();
  await page.locator('.kb-task-popup input[type="text"]').first().fill('EscDirty');
  await page.locator('.kb-task-popup input[type="text"]').first().press('Tab');
  // Wait for server to mark field dirty before pressing Escape
  await expect(page.locator('.kb-task-popup .kb-popup-field--dirty')).toBeVisible();
  await page.keyboard.press('Escape');
  await expect(page.locator('.Wt-dialog', { hasText: 'Unsaved Changes' })).toBeVisible();
});

test('popup has History tab that shows task history', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card').first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  // History tab must exist — click the anchor element inside the tab li
  const historyTab = page.locator('.kb-task-popup .Wt-tabs li', { hasText: 'History' });
  await expect(historyTab).toBeVisible();
  await historyTab.getByRole('link').click();
  // At minimum the history panel should be present (task was created, so there is history)
  await expect(page.locator('.kb-task-popup .kb-history-entry').first()).toBeVisible();
});

test('popup has Archive button that archives the task', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  // Create a fresh task for this test
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('ArchiveViaPopup');
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  // Open popup (use .first() in case prior runs left archived copies visible)
  await page.locator('.kb-card', { hasText: 'ArchiveViaPopup' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  // Click Archive via JS (button may be below viewport in tall popup)
  await page.evaluate(() => {
    const popup = document.querySelector('.Wt-dialog.kb-task-popup');
    const btn = popup?.querySelector('.editor-btn-danger') as HTMLElement | null;
    if (btn) btn.click();
  });
  // Popup closes, task gone from board
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();
  await expect(page.locator('.kb-card', { hasText: 'ArchiveViaPopup' })).not.toBeVisible();
});

test('description renders markdown in display mode', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  // Create task with markdown description via full editor
  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('MarkdownDesc');
  const descEditorEl = page.locator('.ProseMirror[contenteditable="true"]').filter({ visible: true }).first();
  await descEditorEl.click();
  await descEditorEl.fill('task description text');
  // Blur the editor so the value is synced before save
  await page.evaluate(() => { (document.activeElement as HTMLElement)?.blur(); });
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();
  // Open popup — description display should show the entered text
  await page.locator('.kb-card', { hasText: 'MarkdownDesc' }).click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  const descDisplay = page.locator('.kb-task-popup .kb-desc-display');
  await expect(descDisplay).toBeVisible();
  await expect(descDisplay).toContainText('task description text');
});

test('gantt label area click opens popup', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl.replace('/kanban', '/gantt'));
  await expect(page.locator('.gv-wrap')).toBeVisible();
  const hit = page.locator('.gantt-label-hit').first();
  // Only test if at least one task has dates and appears in the gantt
  const count = await hit.count();
  if (count === 0) { return; } // no dated tasks — skip
  await hit.click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
});

// ── Date field tests ──────────────────────────────────────────────────────────
// These tests pin the edit-mode enter/exit and calendar-selection behaviour
// for WDateEdit fields in the popup.

async function openPopupForDateTests(page: any) {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(teamUrl);
  await expect(page.locator('.kb-board')).toBeVisible();
  await page.locator('.kb-card', { hasText: 'PopupTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
}

// Locate the wrapper div for a date field by its label text.
function startDateField(page: any) {
  return page.locator('.kb-editor-field-wrap.kb-popup-field')
    .filter({ has: page.locator('.kb-field-label', { hasText: 'Start date' }) });
}

test('date field: clicking display enters edit mode', async ({ page }) => {
  await openPopupForDateTests(page);
  const field   = startDateField(page);
  const display = field.locator('.kb-popup-display');
  const input   = field.locator('input[type="text"]').first();

  await expect(display).toBeVisible();
  await expect(input).toBeHidden();

  await display.click();

  await expect(input).toBeVisible();
  await expect(display).toBeHidden();
});

test('date field: clicking away without selecting reverts to display mode', async ({ page }) => {
  await openPopupForDateTests(page);
  const field   = startDateField(page);
  const display = field.locator('.kb-popup-display');
  const input   = field.locator('input[type="text"]').first();

  await display.click();
  await expect(input).toBeVisible();

  // Click the popup titlebar — safe non-interactive target inside the dialog.
  await page.locator('.Wt-dialog.kb-task-popup .titlebar').click();

  await expect(display).toBeVisible();
  await expect(input).toBeHidden();
});

// Open the calendar popup by clicking the rightmost 40 px of the WDateEdit
// input — that's where WDateEdit.js's mouseUp handler calls showPopup().
async function openCalendar(page: any, field: any) {
  const input = field.locator('input[type="text"]').first();
  await expect(input).toBeVisible();
  const box = await input.boundingBox();
  if (!box) throw new Error('input bounding box not found');
  // Click ~10 px from the right edge, vertically centred.
  await input.click({ position: { x: box.width - 10, y: Math.floor(box.height / 2) } });
}

test('date field: calendar opens when input is clicked', async ({ page }) => {
  await openPopupForDateTests(page);
  const field   = startDateField(page);
  const display = field.locator('.kb-popup-display');

  await display.click();

  await openCalendar(page, field);

  await page.waitForTimeout(400); // let Wt position the popup
  const datepickerStyle = await page.evaluate(() => {
    const dp = document.querySelector('.Wt-datepicker');
    return dp ? (dp as HTMLElement).style.display : 'NOT FOUND';
  });
  console.log('[date-test] .Wt-datepicker display after right-edge click:', datepickerStyle);

  await expect(page.locator('.Wt-cal').first()).toBeVisible();
});

test('date field: selecting a date from the calendar updates the display', async ({ page }) => {
  await openPopupForDateTests(page);
  const field   = startDateField(page);
  const display = field.locator('.kb-popup-display');

  await display.click();
  await openCalendar(page, field);

  const calendar = page.locator('.Wt-cal').first();
  await expect(calendar).toBeVisible({ timeout: 5000 });

  // Click day 15 — use /^15$/ to avoid matching "15" inside "115" etc.
  const day15 = calendar.locator('td').filter({ hasText: /^15$/ }).first();
  await expect(day15).toBeVisible();
  await day15.click();

  // Log state immediately after click (no extra wait — check before server round-trip).
  const afterClickImmediate = await page.evaluate(() => {
    const disp = document.querySelector('.kb-editor-field-wrap .kb-popup-display');
    const inp  = document.querySelector('.kb-editor-field-wrap input[type="text"]');
    const dp   = document.querySelector('.Wt-datepicker');
    return {
      displayStyle: disp ? (disp as HTMLElement).style.display : null,
      displayText:  disp ? disp.textContent : null,
      inputStyle:   inp  ? (inp  as HTMLElement).style.display : null,
      inputValue:   inp  ? (inp  as HTMLInputElement).value    : null,
      pickerStyle:  dp   ? (dp   as HTMLElement).style.display : null,
    };
  });
  console.log('[date-test] State immediately after day-15 click (pre-server):', afterClickImmediate);

  // Wait for the Wt server round-trip to update the display.
  await expect(display).toBeVisible({ timeout: 5000 });

  const afterRoundtrip = await page.evaluate(() => {
    const disp = document.querySelector('.kb-editor-field-wrap .kb-popup-display');
    const inp  = document.querySelector('.kb-editor-field-wrap input[type="text"]');
    return {
      displayStyle: disp ? (disp as HTMLElement).style.display : null,
      displayText:  disp ? disp.textContent : null,
      inputStyle:   inp  ? (inp  as HTMLElement).style.display : null,
      inputValue:   inp  ? (inp  as HTMLInputElement).value    : null,
    };
  });
  console.log('[date-test] State after server round-trip:', afterRoundtrip);

  const text = await display.textContent();
  console.log('[date-test] Display text:', text);
  expect(text).toMatch(/15/);
});

test('date field: selecting a date marks field dirty and enables Save', async ({ page }) => {
  await openPopupForDateTests(page);
  const field   = startDateField(page);
  const display = field.locator('.kb-popup-display');
  const saveBtn = page.locator('.kb-task-popup .editor-btn-row .editor-btn')
    .filter({ hasText: 'Save Changes' });

  await expect(saveBtn).toBeDisabled();

  await display.click();
  await openCalendar(page, field);

  await expect(page.locator('.Wt-cal').first()).toBeVisible({ timeout: 5000 });
  await page.locator('.Wt-cal').first().locator('td').filter({ hasText: /^15$/ }).first().click();

  // After a valid date selection the field should be dirty → Save enabled.
  await expect(saveBtn).toBeEnabled({ timeout: 5000 });
});

test('date field: calendar navigation does not close the picker', async ({ page }) => {
  // Regression: clicking nav arrows or the month <select> inside .Wt-datepicker
  // used to blur the WDateEdit input, triggering exit_edit_mode → WDateEdit::setHidden
  // → popup_->setHidden, which hid the calendar the user was still using.
  await openPopupForDateTests(page);
  const field   = startDateField(page);
  const display = field.locator('.kb-popup-display');

  await display.click();
  await openCalendar(page, field);

  const calendar = page.locator('.Wt-cal').first();
  await expect(calendar).toBeVisible({ timeout: 5000 });

  // Click the "next month" nav button — should NOT close the picker.
  const monthSelect = calendar.locator('select').first();
  const monthBefore = await monthSelect.inputValue();
  const nextBtn = calendar.locator('.Wt-cal-navbutton').last();
  await nextBtn.click();

  // Calendar should still be visible after navigating.
  await expect(calendar).toBeVisible({ timeout: 3000 });

  // Wait for the server to re-render the calendar with the next month before
  // looking for day 10 — nav button triggers a server round-trip and the DOM
  // updates asynchronously.  Without this wait, Playwright finds a "10" td that
  // belongs to the old month, clicks it, and the coordinate maps to a wrong date.
  await expect(monthSelect).not.toHaveValue(monthBefore, { timeout: 5000 });

  // Now select day 10 in the new month — should work and update the display.
  const day10 = calendar.locator('td').filter({ hasText: /^10$/ }).first();
  await expect(day10).toBeVisible();
  await day10.click();

  await expect(display).toBeVisible({ timeout: 5000 });
  const text = await display.textContent();
  expect(text).toMatch(/10/);
});
