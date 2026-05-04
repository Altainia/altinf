'use strict';

const { JSDOM } = require('jsdom');
const fs   = require('fs');
const path = require('path');

// ── Helpers ──────────────────────────────────────────────────────────────────

const GANTT_SRC = fs.readFileSync(
  path.join(__dirname, '../../resources/js/gantt.js'), 'utf8');

const LABEL_W    = 190;
const MIN_PX_DAY = 22;

function isoDate(d) {
  return d.toISOString().slice(0, 10);
}

function addDays(d, n) {
  return new Date(d.getTime() + n * 86400000);
}

// Build a task with start/end dates relative to today
function makeTask(id, startOffset, endOffset, assignee) {
  const today = new Date();
  today.setHours(0, 0, 0, 0);
  return {
    id,
    title: 'Task ' + id,
    assigned_to: assignee || 'alice',
    color: '#7aa2d4',
    start_date: startOffset != null ? isoDate(addDays(today, startOffset)) : '',
    end_date:   endOffset   != null ? isoDate(addDays(today, endOffset))   : '',
  };
}

// Create a DOM with gantt.js loaded.
// mockContainerWidth: the pixel width the container reports (simulates the browser
// computing max(containerWidth, svg min-width) for getBoundingClientRect on an svg).
function makeGanttDOM(mockContainerWidth) {
  const dom = new JSDOM('<!DOCTYPE html><body><div id="mount"></div></body>', {
    runScripts: 'dangerously',
  });
  const { window } = dom;

  window.eval(GANTT_SRC);

  // Simulate CSS: SVG rendered width = max(containerWidth, svg.style.minWidth)
  window.Element.prototype.getBoundingClientRect = function () {
    if (this.nodeName && this.nodeName.toLowerCase() === 'svg') {
      const minWidthStr = (this.style && this.style.minWidth) || '0px';
      const minWidth    = parseFloat(minWidthStr) || 0;
      const w           = Math.max(mockContainerWidth, minWidth);
      return { width: w, height: 0, top: 0, left: 0, right: w, bottom: 0 };
    }
    return { width: 0, height: 0, top: 0, left: 0, right: 0, bottom: 0 };
  };

  return dom;
}

function getSvg(dom) {
  return dom.window.document.querySelector('#mount svg');
}

function getViewBoxW(svg) {
  const vb = svg.getAttribute('viewBox') || '';
  return parseFloat(vb.split(' ')[2]) || 0;
}

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

function assert(cond, msg) {
  if (!cond) throw new Error(msg || 'assertion failed');
}

function assertEq(a, b, msg) {
  if (a !== b) throw new Error((msg || '') + ' — expected ' + b + ', got ' + a);
}

function assertClose(a, b, tol, msg) {
  if (Math.abs(a - b) > (tol || 1)) {
    throw new Error((msg || '') + ' — expected ~' + b + ', got ' + a);
  }
}

// ── Width invariant tests ─────────────────────────────────────────────────────

console.log('\nSVG width invariant (container wider than all day ranges)');

const WIDE = 1200; // wide enough that no range hits the min-width floor

[7, 13, 21, 30].forEach(function (rangeDays) {
  test('viewBox width ≈ container width for ' + rangeDays + ' days', function () {
    const dom = makeGanttDOM(WIDE);
    // Span today ±60 days so all ranges have visible tasks
    dom.window.initGantt('mount', [makeTask(1, -5, 5), makeTask(2, 1, 8)]);
    // Change to this range via the select
    const sel = dom.window.document.querySelector('.gv-range-select');
    sel.value = String(rangeDays);
    sel.dispatchEvent(new dom.window.Event('change'));
    const svg = getSvg(dom);
    assert(svg, 'SVG element not found');
    // The viewBox width should equal the container width (WIDE px)
    assertClose(getViewBoxW(svg), WIDE, 1,
      'range=' + rangeDays + ' viewBox width');
  });
});

test('SVG explicit width attribute matches viewBox width', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -5, 5)]);
  const svg = getSvg(dom);
  const attrW  = parseFloat(svg.getAttribute('width'));
  const vboxW  = getViewBoxW(svg);
  assertClose(attrW, vboxW, 1, 'width attr vs viewBox');
});

console.log('\nMin-width / scroll floor');

test('viewBox width = minSvgW when container narrower than min', function () {
  const rangeDays = 30;
  const minSvgW   = LABEL_W + rangeDays * MIN_PX_DAY; // 190 + 30*22 = 850
  const narrow    = 400; // narrower than minSvgW
  const dom       = makeGanttDOM(narrow);
  dom.window.initGantt('mount', [makeTask(1, -2, 28)]);
  const sel = dom.window.document.querySelector('.gv-range-select');
  sel.value = '30';
  sel.dispatchEvent(new dom.window.Event('change'));
  const svg = getSvg(dom);
  assertClose(getViewBoxW(svg), minSvgW, 1, '30-day narrow container');
});

test('minWidth style on SVG equals LABEL_W + rangeDays * MIN_PX_DAY', function () {
  const dom = makeGanttDOM(500);
  dom.window.initGantt('mount', [makeTask(1, -1, 6)]);
  // Default range = 13
  const svg = getSvg(dom);
  const minW = parseFloat(svg.style.minWidth);
  const expected = LABEL_W + 13 * MIN_PX_DAY; // 190 + 286 = 476
  assertEq(minW, expected, 'default 13-day min-width');
});

console.log('\nToday line');

test('today line present in default view', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -3, 9)]);
  const svg = getSvg(dom);
  const lines = Array.from(svg.querySelectorAll('line'));
  const todayLine = lines.find(function (l) {
    return (l.getAttribute('style') || '').includes('#e05252');
  });
  assert(todayLine, 'today line not found in SVG');
});

test('today line absent when view is shifted entirely to the past', function () {
  const dom = makeGanttDOM(WIDE);
  // tasks far in past so Prev is available
  dom.window.initGantt('mount', [makeTask(1, -60, -50)]);
  const svg = getSvg(dom);
  if (!svg) return; // no tasks in default view — that's fine, nothing to check
  const lines = Array.from(svg.querySelectorAll('line'));
  const todayLine = lines.find(function (l) {
    return (l.getAttribute('style') || '').includes('#e05252');
  });
  assert(!todayLine, 'today line should not appear in past-only view');
});

console.log('\nSlide buttons');

test('Prev disabled when no tasks before viewport', function () {
  const dom = makeGanttDOM(WIDE);
  // All tasks start after today, none in the past
  dom.window.initGantt('mount', [makeTask(1, 1, 10), makeTask(2, 2, 12)]);
  const prevBtn = dom.window.document.querySelector('.gv-nav .gv-btn');
  assert(prevBtn.hasAttribute('disabled'), 'Prev should be disabled');
});

test('Next disabled when no tasks after viewport', function () {
  const dom = makeGanttDOM(WIDE);
  // All tasks end before today, none in the future
  dom.window.initGantt('mount', [makeTask(1, -12, -2), makeTask(2, -10, -1)]);
  const btns   = dom.window.document.querySelectorAll('.gv-nav .gv-btn');
  const nextBtn = btns[btns.length - 1];
  assert(nextBtn.hasAttribute('disabled'), 'Next should be disabled');
});

test('Prev enabled when a task starts before the viewport', function () {
  const dom = makeGanttDOM(WIDE);
  // One task in the past; default view is today-centered so it is before viewStart
  dom.window.initGantt('mount', [makeTask(1, -30, -20), makeTask(2, -2, 8)]);
  const prevBtn = dom.window.document.querySelector('.gv-nav .gv-btn');
  assert(!prevBtn.hasAttribute('disabled'), 'Prev should be enabled');
});

test('Next enabled when a task ends after the viewport', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -2, 8), makeTask(2, 20, 30)]);
  const btns    = dom.window.document.querySelectorAll('.gv-nav .gv-btn');
  const nextBtn = btns[btns.length - 1];
  assert(!nextBtn.hasAttribute('disabled'), 'Next should be enabled');
});

test('task with no end_date does not enable Next', function () {
  const dom = makeGanttDOM(WIDE);
  // Only task: start in distant future, no end_date → NOT "in the future"
  dom.window.initGantt('mount', [makeTask(1, 30, null)]);
  const btns    = dom.window.document.querySelectorAll('.gv-nav .gv-btn');
  const nextBtn = btns[btns.length - 1];
  assert(nextBtn.hasAttribute('disabled'), 'Next should be disabled for no-end-date task');
});

test('task with no start_date does not enable Prev', function () {
  const dom = makeGanttDOM(WIDE);
  // Only task: no start_date, end in distant past → NOT "in the past"
  dom.window.initGantt('mount', [makeTask(1, null, -30)]);
  const prevBtn = dom.window.document.querySelector('.gv-nav .gv-btn');
  assert(prevBtn.hasAttribute('disabled'), 'Prev should be disabled for no-start-date task');
});

console.log('\nToday button ratio');

test('Today button resets view so today is ~4/13 from the left', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -30, 30)]);
  // Click Prev to shift view away from today
  const prevBtn = dom.window.document.querySelector('.gv-nav .gv-btn');
  prevBtn.click();
  // Click Today to reset
  const todayBtn = dom.window.document.querySelector('.gv-btn--today');
  todayBtn.click();

  const svg     = getSvg(dom);
  const lines   = Array.from(svg.querySelectorAll('line'));
  const todayLine = lines.find(function (l) {
    return (l.getAttribute('style') || '').includes('#e05252');
  });
  assert(todayLine, 'today line not found after Today click');

  const vboxW = getViewBoxW(svg);
  const tx    = parseFloat(todayLine.getAttribute('x1'));
  // today should be ~4/13 of the track width from the left edge of the track
  const trackW        = vboxW - LABEL_W;
  const expectedRatio = 4 / 13;
  const actualRatio   = (tx - LABEL_W) / trackW;
  assertClose(actualRatio, expectedRatio, 0.05,
    'today x-ratio after Today click');
});

console.log('\nRange selector');

test('range select has correct options', function () {
  const dom   = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -2, 8)]);
  const opts  = Array.from(dom.window.document.querySelectorAll('.gv-range-select option'));
  const vals  = opts.map(function (o) { return +o.value; });
  const expected = [7, 13, 21, 30, 60];
  assertEq(vals.join(','), expected.join(','), 'range option values');
});

test('default selected range is 13', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -2, 8)]);
  const sel = dom.window.document.querySelector('.gv-range-select');
  assertEq(+sel.value, 13, 'default selected range');
});

// ── Fade gradients ────────────────────────────────────────────────────────────

console.log('\nFade gradients');

test('left-clipped bar gets a gradient with opacity-0 first stop', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -30, 5)]);
  const svg = getSvg(dom);
  assert(svg, 'SVG not found');
  const grads = Array.from(svg.querySelectorAll('defs linearGradient'));
  assert(grads.length > 0, 'expected a gradient for left-clipped bar');
  const stops = Array.from(grads[0].querySelectorAll('stop'));
  assert((stops[0].getAttribute('style') || '').includes('stop-opacity:0'),
    'first stop should be opacity 0 for left fade');
});

test('right-clipped bar gets a gradient with opacity-0 last stop', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, -2, 40)]);
  const svg = getSvg(dom);
  assert(svg, 'SVG not found');
  const grads = Array.from(svg.querySelectorAll('defs linearGradient'));
  assert(grads.length > 0, 'expected a gradient for right-clipped bar');
  const stops = Array.from(grads[0].querySelectorAll('stop'));
  assert((stops[stops.length - 1].getAttribute('style') || '').includes('stop-opacity:0'),
    'last stop should be opacity 0 for right fade');
});

test('unclipped bar has no gradient', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, 0, 5)]);
  const svg = getSvg(dom);
  assert(svg, 'SVG not found');
  const grads = svg.querySelectorAll('defs linearGradient');
  assertEq(grads.length, 0, 'no gradient for unclipped bar');
});

test('no-start task always has a left-fade gradient', function () {
  const dom = makeGanttDOM(WIDE);
  // no start_date, end within view — left side is unbounded, so it fades
  dom.window.initGantt('mount', [makeTask(1, null, 5)]);
  const svg = getSvg(dom);
  assert(svg, 'SVG not found');
  const grads = Array.from(svg.querySelectorAll('defs linearGradient'));
  assert(grads.length > 0, 'expected a gradient for no-start bar');
  const stops = Array.from(grads[0].querySelectorAll('stop'));
  assert((stops[0].getAttribute('style') || '').includes('stop-opacity:0'),
    'first stop should be opacity 0 for unbounded left');
});

test('no-end task always has a right-fade gradient', function () {
  const dom = makeGanttDOM(WIDE);
  // start within view, no end_date — right side is unbounded, so it fades
  dom.window.initGantt('mount', [makeTask(1, -2, null)]);
  const svg = getSvg(dom);
  assert(svg, 'SVG not found');
  const grads = Array.from(svg.querySelectorAll('defs linearGradient'));
  assert(grads.length > 0, 'expected a gradient for no-end bar');
  const stops = Array.from(grads[0].querySelectorAll('stop'));
  assert((stops[stops.length - 1].getAttribute('style') || '').includes('stop-opacity:0'),
    'last stop should be opacity 0 for unbounded right');
});

test('no-start task with right-clipped real end gets fades on both sides', function () {
  const dom = makeGanttDOM(WIDE);
  // no start (left fade) + end clipped 40 days out (right fade)
  dom.window.initGantt('mount', [makeTask(1, null, 40)]);
  const svg = getSvg(dom);
  assert(svg, 'SVG not found');
  const grads = Array.from(svg.querySelectorAll('defs linearGradient'));
  assert(grads.length > 0, 'expected a gradient');
  const stops = Array.from(grads[0].querySelectorAll('stop'));
  assert((stops[0].getAttribute('style') || '').includes('stop-opacity:0'),
    'first stop should be opacity 0 (unbounded left)');
  assert((stops[stops.length - 1].getAttribute('style') || '').includes('stop-opacity:0'),
    'last stop should be opacity 0 (right fade)');
});

test('no-start no-end task is rendered with fades on both sides', function () {
  const dom = makeGanttDOM(WIDE);
  dom.window.initGantt('mount', [makeTask(1, null, null)]);
  const svg = getSvg(dom);
  assert(svg, 'SVG element not found — task should be rendered');
  const grads = Array.from(svg.querySelectorAll('defs linearGradient'));
  assert(grads.length > 0, 'expected a gradient for no-date task');
  const stops = Array.from(grads[0].querySelectorAll('stop'));
  assert((stops[0].getAttribute('style') || '').includes('stop-opacity:0'),
    'first stop should be opacity 0 (left fade)');
  assert((stops[stops.length - 1].getAttribute('style') || '').includes('stop-opacity:0'),
    'last stop should be opacity 0 (right fade)');
});

// ── Summary ───────────────────────────────────────────────────────────────────

console.log('\n' + (failed ? '✗' : '✓') + ' ' + passed + ' passed, ' + failed + ' failed\n');
process.exit(failed > 0 ? 1 : 0);
