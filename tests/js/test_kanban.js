'use strict';

const { JSDOM } = require('jsdom');
const fs   = require('fs');
const path = require('path');

const KANBAN_SRC = fs.readFileSync(
  path.join(__dirname, '../../resources/js/kanban.js'), 'utf8');

// ── Test runner ───────────────────────────────────────────────────────────────

let passed = 0, failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log('  PASS  ' + name);
    passed++;
  } catch (e) {
    console.log('  FAIL  ' + name);
    console.log('        ' + e.message);
    failed++;
  }
}

function assert(cond, msg) { if (!cond) throw new Error(msg || 'assertion failed'); }
function assertEq(a, b, msg) {
  if (a !== b) throw new Error((msg || '') + ' — expected ' + b + ', got ' + a);
}

// Build a DOM with kanban.js loaded at a given viewport width.
function makeBoardDOM(width, cbId) {
  const cbHtml = cbId ? '<input id="' + cbId + '">' : '';
  const dom = new JSDOM(
    '<!DOCTYPE html><body>' + cbHtml + '<div id="mount"></div></body>',
    { runScripts: 'dangerously' });
  Object.defineProperty(dom.window, 'innerWidth', { value: width, configurable: true });
  dom.window.eval(KANBAN_SRC);
  return dom;
}

function tasks() {
  return [
    { id: 1, status: 'todo',        title: 'A', assignees: ['ana'], color: '#7aa2d4' },
    { id: 2, status: 'in_progress', title: 'B', assignees: [],      color: '#e6b800' },
    { id: 3, status: 'todo',        title: 'C', assignees: ['sam'], color: '#93c47d' },
  ];
}

// ── Desktop layout (regression guard) ────────────────────────────────────────

console.log('\nDesktop board');

test('desktop renders no status tabs and all columns visible', function () {
  const dom = makeBoardDOM(1200);
  dom.window.initKanban('mount', tasks(), null, true, true);
  const doc = dom.window.document;
  assert(!doc.querySelector('.kb-status-tabs'), 'no status tabs on desktop');
  const hidden = doc.querySelectorAll('.kb-column--hidden');
  assertEq(hidden.length, 0, 'no columns hidden on desktop');
});

test('desktop card click fires EDIT', function () {
  const dom = makeBoardDOM(1200, 'cb');
  let fired = '';
  dom.window.document.getElementById('cb')
    .addEventListener('change', function () { fired = this.value; });
  dom.window.initKanban('mount', tasks(), 'cb', true, true);
  const card = dom.window.document.querySelector('.kb-card');
  card.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  assertEq(fired, 'EDIT:1', 'desktop card click');
});

test('desktop cards are draggable; no quick-move button', function () {
  const dom = makeBoardDOM(1200);
  dom.window.initKanban('mount', tasks(), null, true, true);
  const card = dom.window.document.querySelector('.kb-card');
  assertEq(card.getAttribute('draggable'), 'true', 'card draggable on desktop');
  assert(!dom.window.document.querySelector('.kb-card-move'), 'no move button on desktop');
});

// ── Mobile layout ────────────────────────────────────────────────────────────

console.log('\nMobile board');

test('mobile renders status tabs with todo active and only todo visible', function () {
  const dom = makeBoardDOM(390);
  dom.window.initKanban('mount', tasks(), null, true, true);
  const doc = dom.window.document;
  const tabs = doc.querySelectorAll('.kb-status-tabs .kb-status-tab');
  assertEq(tabs.length, 4, 'four status tabs');
  const active = doc.querySelector('.kb-status-tab--active');
  assertEq(active.dataset.status, 'todo', 'todo tab active by default');
  const todoCol = doc.querySelector('.kb-column[data-status="todo"]');
  const progCol = doc.querySelector('.kb-column[data-status="in_progress"]');
  assert(!todoCol.classList.contains('kb-column--hidden'), 'todo column visible');
  assert(progCol.classList.contains('kb-column--hidden'), 'other columns hidden');
});

test('tapping a tab switches the visible column', function () {
  const dom = makeBoardDOM(390);
  dom.window.initKanban('mount', tasks(), null, true, true);
  const doc = dom.window.document;
  const progTab = doc.querySelector('.kb-status-tab[data-status="in_progress"]');
  progTab.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  assert(doc.querySelector('.kb-column[data-status="in_progress"]')
            .classList.contains('kb-column--hidden') === false, 'in_progress now visible');
  assert(doc.querySelector('.kb-column[data-status="todo"]')
            .classList.contains('kb-column--hidden'), 'todo now hidden');
});

test('mobile card body tap fires NAV (full-page editor)', function () {
  const dom = makeBoardDOM(390, 'cb');
  let fired = '';
  dom.window.document.getElementById('cb')
    .addEventListener('change', function () { fired = this.value; });
  dom.window.initKanban('mount', tasks(), 'cb', true, true);
  const card = dom.window.document.querySelector('.kb-column[data-status="todo"] .kb-card');
  card.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  assertEq(fired, 'NAV:1', 'mobile card click should fire NAV');
});

test('quick-move menu fires MOVE with target status and end sort order', function () {
  const dom = makeBoardDOM(390, 'cb');
  let fired = '';
  dom.window.document.getElementById('cb')
    .addEventListener('change', function () { fired = this.value; });
  dom.window.initKanban('mount', tasks(), 'cb', true, true);
  const doc = dom.window.document;
  const card = doc.querySelector('.kb-column[data-status="todo"] .kb-card');
  const moveBtn = card.querySelector('.kb-card-move');
  assert(moveBtn, 'expected a quick-move button');
  moveBtn.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  const menu = card.querySelector('.kb-move-menu');
  assert(menu, 'expected a move menu to open');
  // Move the todo card into in_progress (currently holds task 2 → next sort = 1).
  const items = Array.from(menu.querySelectorAll('.kb-move-item'));
  const target = items.find(function (i) { return i.textContent === 'In Progress'; });
  assert(target, 'expected an "In Progress" target');
  target.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  assertEq(fired, 'MOVE:1:in_progress:1', 'MOVE payload');
});

test('refresh keeps the active column (does not snap back to To Do)', function () {
  const dom = makeBoardDOM(390);
  dom.window.initKanban('mount', tasks(), null, true, true);
  const doc = dom.window.document;
  // View the In Progress column...
  doc.querySelector('.kb-status-tab[data-status="in_progress"]')
     .dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  // ...then a live update re-inits the board (as refresh() does after a move).
  dom.window.initKanban('mount', tasks(), null, true, true);
  const active = doc.querySelector('.kb-status-tab--active');
  assertEq(active.dataset.status, 'in_progress', 'active column preserved across refresh');
  assert(!doc.querySelector('.kb-column[data-status="in_progress"]')
             .classList.contains('kb-column--hidden'), 'in_progress still visible');
});

test('move menu omits the card\'s current status', function () {
  const dom = makeBoardDOM(390, 'cb');
  dom.window.initKanban('mount', tasks(), 'cb', true, true);
  const card = dom.window.document.querySelector('.kb-column[data-status="todo"] .kb-card');
  card.querySelector('.kb-card-move')
      .dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  const labels = Array.from(card.querySelectorAll('.kb-move-item'))
    .map(function (i) { return i.textContent; });
  assert(labels.indexOf('To Do') === -1, 'current status should be excluded');
  assertEq(labels.length, 3, 'three move targets');
});

// ── Summary ───────────────────────────────────────────────────────────────────

console.log('\n' + (failed ? '✗' : '✓') + ' ' + passed + ' passed, ' + failed + ' failed\n');
process.exit(failed > 0 ? 1 : 0);
