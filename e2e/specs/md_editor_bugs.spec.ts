import { test, expect } from '@playwright/test';
import { loginAs } from './helpers';

// Regression tests for two markdown-editor bugs in the task popup:
//   Bug 1: clicking a TUI toolbar button (Bold, More, …) while editing the
//          description collapsed the editor back to its viewer.
//   Bug 2: the "More" overflow dropdown in the comment compose editor opened
//          then immediately closed (Wt dispatches a synthetic document click
//          that TUI's handleClickDocument treats as a click-away).

test.describe.configure({ mode: 'serial' });

// Wt renders single-page-app navigations over WebSocket; the first few on a cold
// server (notably in Firefox) can exceed the default 10s expect timeout. Give the
// cross-page transition waits extra headroom so the suite is stable without retries.
const NAV = { timeout: 20_000 };

let boardUrl = '';

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible(NAV);

  const orgExists = (await page.locator('.org-list-link', { hasText: /^BugTestOrg$/ }).count()) > 0;
  if (!orgExists) {
    await page.locator('input[placeholder="Organization name"]').fill('BugTestOrg');
    await page.locator('.org-create-form .editor-btn').click();
    await expect(page.locator('.org-list-link', { hasText: /^BugTestOrg$/ })).toBeVisible(NAV);
  }
  await page.locator('.org-list-link', { hasText: /^BugTestOrg$/ }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible(NAV);

  const teamExists = (await page.locator('.org-team-link').count()) > 0;
  if (!teamExists) {
    await page.getByRole('link', { name: 'Manage organization' }).click();
    await page.locator('input[placeholder="Team name"]').fill('BugTestTeam');
    await page.getByRole('button', { name: 'Create' }).click();
    await expect(page.locator('.kb-team-block')).toBeVisible(NAV);
    await page.locator('.nav-link', { hasText: 'Orgs' }).click();
    await page.locator('.org-list-link', { hasText: /^BugTestOrg$/ }).first().click();
  }
  await page.locator('.org-team-link').first().click();
  await expect(page.locator('.kb-board')).toBeVisible(NAV);
  boardUrl = page.url();

  const taskExists = (await page.locator('.kb-card', { hasText: /^BugTestTask$/ }).count()) > 0;
  if (!taskExists) {
    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible(NAV);
    await page.locator('input[placeholder="Task title"]').fill('BugTestTask');
    const descPM = page.locator('.ProseMirror[contenteditable="true"]').filter({ visible: true }).first();
    await descPM.click();
    await descPM.fill('Test description text');
    await page.evaluate(() => { (document.activeElement as HTMLElement)?.blur(); });
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible(NAV);
  }

  // A task with NO description — clicking its (empty) description area must still
  // enter edit mode (regression: empty viewer collapses, click landed on the dead
  // root area instead of the editor mount).
  const emptyExists = (await page.locator('.kb-card', { hasText: /^EmptyDescTask$/ }).count()) > 0;
  if (!emptyExists) {
    await page.locator('.kb-new-btn').click();
    await expect(page.locator('.kb-editor-page')).toBeVisible(NAV);
    await page.locator('input[placeholder="Task title"]').fill('EmptyDescTask');
    // Leave the description untouched (empty).
    await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
    await expect(page.locator('.kb-board')).toBeVisible(NAV);
  }

  // Add a comment to BugTestTask so there is an existing comment-edit editor in the
  // popup — this creates a second TUI dropdown-toolbar, the condition under which
  // the More-dropdown regression appears.
  await page.locator('.kb-card', { hasText: 'BugTestTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible(NAV);
  await page.waitForTimeout(500);
  const hasComment = (await page.locator('.kb-task-popup .kb-comment-item').count()) > 0;
  if (!hasComment) {
    await page.locator('.kb-task-popup .body').evaluate(el => el.scrollTo(0, el.scrollHeight));
    await page.waitForTimeout(300);
    const cpm = page.locator('.kb-task-popup .kb-comment-compose .ProseMirror[contenteditable="true"]').filter({ visible: true }).first();
    await cpm.click();
    await cpm.type('Existing comment body');
    await page.waitForTimeout(300);
    await page.locator('.kb-task-popup .titlebar').click(); // blur to fire changed()
    await page.waitForTimeout(400);
    await page.locator('.kb-task-popup .kb-comment-post-btn').click();
    await page.waitForTimeout(700);
  }
  await page.close();
});

async function openTaskPopup(page: any) {
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible(NAV);
  await page.locator('.kb-card', { hasText: 'BugTestTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible(NAV);
  await page.waitForTimeout(600);
}

// `_mdInEdit` is set on the TUI mount div (first child of .kb-desc-field).
function descState(field: any) {
  return field.evaluate((el: any) => {
    const mount = el.children[0];
    return {
      hasViewer: !!el.querySelector('.toastui-editor-contents'),
      hasEditorArea: !!el.querySelector('.ProseMirror[contenteditable="true"]'),
      mdInEdit: mount?._mdInEdit,
    };
  });
}

test('Bug 1: toolbar click does not collapse description editor', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await openTaskPopup(page);

  const descField = page.locator('.kb-task-popup .kb-desc-field');
  await expect(descField).toBeVisible();

  // Single widget starts in viewer mode.
  const init = await descState(descField);
  expect(init.hasViewer, 'starts as viewer').toBe(true);
  expect(init.hasEditorArea, 'no editor initially').toBe(false);
  expect(init.mdInEdit, '_mdInEdit false initially').toBe(false);

  // Clicking the viewer switches to the editor.
  await descField.click();
  await page.waitForTimeout(700);

  const editState = await descState(descField);
  expect(editState.hasEditorArea, 'editor appears after click').toBe(true);
  expect(editState.mdInEdit, '_mdInEdit true in edit mode').toBe(true);

  // Clicking a toolbar button must NOT collapse the editor.
  const tbIcon = descField.locator('.toastui-editor-toolbar-icons').filter({ visible: true }).first();
  await expect(tbIcon).toBeVisible();
  await tbIcon.click({ force: true });
  await page.waitForTimeout(700);

  const afterTb = await descState(descField);
  expect(afterTb.hasEditorArea, 'editor stays after toolbar click').toBe(true);
  expect(afterTb.mdInEdit, '_mdInEdit still true after toolbar click').toBe(true);
});

test('Bug 1b: clicking outside the description editor returns to viewer', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await openTaskPopup(page);

  const descField = page.locator('.kb-task-popup .kb-desc-field');
  await descField.click();
  await page.waitForTimeout(500);

  const inEdit = await descField.evaluate((el: any) => !!el.querySelector('.ProseMirror[contenteditable="true"]'));
  expect(inEdit, 'in edit mode after click').toBe(true);

  // Click outside the editor (titlebar) → focusout → viewer returns.
  await page.locator('.kb-task-popup .titlebar').click();
  await page.waitForTimeout(700);

  const exited = await descState(descField);
  expect(exited.hasViewer, 'viewer returns after blur').toBe(true);
  expect(exited.hasEditorArea, 'editor area gone after blur').toBe(false);
  expect(exited.mdInEdit, '_mdInEdit false after exit').toBe(false);
});

test('Bug 2: More dropdown opens in the comment compose editor', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await openTaskPopup(page);

  // Scroll to the comment compose editor at the bottom of the popup.
  await page.locator('.kb-task-popup .body').evaluate(el => el.scrollTo(0, el.scrollHeight));
  await page.waitForTimeout(400);

  const compose = page.locator('.kb-task-popup .kb-comment-compose');
  await expect(compose).toBeVisible();
  const composePM = compose.locator('.ProseMirror[contenteditable="true"]').filter({ visible: true }).first();
  await expect(composePM).toBeVisible();

  // Narrow the dialog so the toolbar overflows and the "More" button appears.
  await page.evaluate(() => {
    const dlg = document.querySelector('.Wt-dialog.kb-task-popup') as HTMLElement | null;
    if (dlg) {
      dlg.style.setProperty('min-width', '490px', 'important');
      dlg.style.setProperty('max-width', '490px', 'important');
    }
  });
  await page.waitForTimeout(700);

  const moreBtn = compose.locator('.toastui-editor-toolbar-icons.more').filter({ visible: true });
  await expect(moreBtn, 'More button should be present at 490px').toHaveCount(1);

  // Focus the editor, then click More — the dropdown must stay open.
  await composePM.click();
  await page.waitForTimeout(200);
  await moreBtn.click();
  await page.waitForTimeout(600);

  // The dropdown should be visible (display flex) with a real bounding box,
  // not immediately closed by Wt's synthetic document click.
  const ddState = await compose.evaluate((el: any) => {
    const dd = el.querySelector('.toastui-editor-dropdown-toolbar');
    if (!dd) return { inDOM: false };
    const r = dd.getBoundingClientRect();
    return {
      inDOM: true,
      display: window.getComputedStyle(dd).display,
      height: Math.round(r.height),
      width: Math.round(r.width),
    };
  });
  expect(ddState.inDOM, 'dropdown in DOM').toBe(true);
  expect(ddState.display, 'dropdown is displayed').not.toBe('none');
  expect(ddState.height, 'dropdown has nonzero height').toBeGreaterThan(0);
});

test('Bug 1c: clicking an empty description still enters edit mode', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible(NAV);
  await page.locator('.kb-card', { hasText: 'EmptyDescTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible(NAV);
  await page.waitForTimeout(600);

  const descField = page.locator('.kb-task-popup .kb-desc-field');
  await expect(descField).toBeVisible();

  // The empty viewer collapses the inner mount to height 0; the click must still
  // be caught (listener on the widget root, not the collapsed mount).
  await descField.click();
  await page.waitForTimeout(700);

  const editState = await descState(descField);
  expect(editState.hasEditorArea, 'editor appears even for empty description').toBe(true);
  expect(editState.mdInEdit, '_mdInEdit true after click on empty description').toBe(true);
});

test('Bug 1d: clicking an empty description on the full editor page enters edit mode', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible(NAV);
  // Reach the dedicated full editor via the popup's "Open full editor" link.
  await page.locator('.kb-card', { hasText: 'EmptyDescTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible(NAV);
  await page.waitForTimeout(500);
  await page.locator('.kb-popup-full-link').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible(NAV);
  await page.waitForTimeout(700);

  const descField = page.locator('.kb-editor-page .kb-desc-field');
  await expect(descField).toBeVisible();
  await descField.click();
  await page.waitForTimeout(700);

  const editState = await descState(descField);
  expect(editState.hasEditorArea, 'full-page empty description becomes editable').toBe(true);
  expect(editState.mdInEdit, '_mdInEdit true after click on full page').toBe(true);
});

test('Bug 3: clicking the editor mode switch keeps the description editor open', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await openTaskPopup(page);

  const descField = page.locator('.kb-task-popup .kb-desc-field');
  await descField.click();
  await page.waitForTimeout(700);
  expect((await descState(descField)).hasEditorArea, 'editor open').toBe(true);

  // Clicking the "Markdown" mode-switch tab (a non-focusable element inside the
  // editor) must NOT collapse the editor — it should switch modes and stay open.
  const modeBtn = descField.locator('.toastui-editor-mode-switch .tab-item', { hasText: 'Markdown' });
  await expect(modeBtn).toHaveCount(1);
  await modeBtn.first().click();
  await page.waitForTimeout(600);

  const st = await descField.evaluate((el: any) => ({
    mdInEdit: el.children[0]?._mdInEdit,
    hasMdContainer: !!el.querySelector('.toastui-editor-md-container'),
  }));
  expect(st.mdInEdit, 'editor still in edit mode after mode switch').toBe(true);
});

test('Bug 3b: clicking empty space inside the description editor keeps it open', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await openTaskPopup(page);

  const descField = page.locator('.kb-task-popup .kb-desc-field');
  await descField.click();
  await page.waitForTimeout(700);
  expect((await descState(descField)).hasEditorArea, 'editor open').toBe(true);

  // Click in the lower (empty) area of the editor content, below any text.
  const box = await descField.boundingBox();
  if (box) {
    await page.mouse.click(box.x + box.width / 2, box.y + box.height - 12);
  }
  await page.waitForTimeout(600);

  const st = await descState(descField);
  expect(st.hasEditorArea, 'editor stays open after clicking empty content area').toBe(true);
  expect(st.mdInEdit, '_mdInEdit still true').toBe(true);
});

test('Bug 4: More dropdown closes when clicking off it (popup)', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await openTaskPopup(page);

  await page.locator('.kb-task-popup .body').evaluate(el => el.scrollTo(0, el.scrollHeight));
  await page.waitForTimeout(400);
  await page.evaluate(() => {
    const dlg = document.querySelector('.Wt-dialog.kb-task-popup') as HTMLElement | null;
    if (dlg) {
      dlg.style.setProperty('min-width', '490px', 'important');
      dlg.style.setProperty('max-width', '490px', 'important');
    }
  });
  await page.waitForTimeout(700);

  const compose = page.locator('.kb-task-popup .kb-comment-compose');
  const moreBtn = compose.locator('.toastui-editor-toolbar-icons.more').filter({ visible: true });
  await expect(moreBtn).toHaveCount(1);

  await compose.locator('.ProseMirror[contenteditable="true"]').filter({ visible: true }).first().click();
  await page.waitForTimeout(200);
  await moreBtn.click();
  await page.waitForTimeout(500);

  // Dropdown is open.
  let dd = await compose.evaluate((el: any) => {
    const d = el.querySelector('.toastui-editor-dropdown-toolbar');
    return d ? window.getComputedStyle(d).display : 'none';
  });
  expect(dd, 'dropdown open after More click').not.toBe('none');

  // Click off the dropdown — on the comment section header (inside the popup, away
  // from the dropdown).  The dropdown must close.
  await page.locator('.kb-task-popup .kb-comment-section .section-header').first().click();
  await page.waitForTimeout(500);

  dd = await compose.evaluate((el: any) => {
    const d = el.querySelector('.toastui-editor-dropdown-toolbar');
    return d ? window.getComputedStyle(d).display : 'none';
  });
  expect(dd, 'dropdown closes after clicking off it').toBe('none');
});

test('Bug 5: editing the description, collapsing and saving persists the change', async ({ page }) => {
  await loginAs(page, 'admin', 'testpass');
  await page.goto(boardUrl);
  await expect(page.locator('.kb-board')).toBeVisible(NAV);
  // Use EmptyDescTask so the test is order-independent (BugTestTask title may have
  // been edited by other suites); give it a description here.
  await page.locator('.kb-card', { hasText: 'EmptyDescTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible(NAV);
  await page.waitForTimeout(600);

  const descField = page.locator('.kb-task-popup .kb-desc-field');
  await descField.click();
  await page.waitForTimeout(700);
  const pm = descField.locator('.ProseMirror[contenteditable="true"]').filter({ visible: true }).first();
  await pm.click();
  const stamp = 'persist-check-' + Date.now();
  await pm.type(stamp);
  await page.waitForTimeout(300);

  // Collapse by clicking outside (titlebar) → marks dirty + enables Save.
  await page.locator('.kb-task-popup .titlebar').click();
  await page.waitForTimeout(500);
  const saveBtn = page.locator('.kb-task-popup .editor-btn-row .editor-btn').filter({ hasText: 'Save Changes' });
  await expect(saveBtn).toBeEnabled();
  await saveBtn.click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeHidden();

  // Reopen and confirm the new text persisted in the description.
  await page.locator('.kb-card', { hasText: 'EmptyDescTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible(NAV);
  await page.waitForTimeout(700);
  await expect(page.locator('.kb-task-popup .kb-desc-field')).toContainText(stamp);
});
