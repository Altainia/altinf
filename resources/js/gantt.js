(function () {
  'use strict';

  var ROW_H              = 36;
  var LABEL_W            = 190;
  var HDR_H              = 28;
  var MIN_PX_DAY         = 22;   // minimum px per day; triggers scroll when exceeded
  var DEFAULT_RANGE_DAYS = 13;
  var DEFAULT_PAST_DAYS  = 4;
  var RANGE_OPTIONS      = [7, 13, 21, 30, 60];
  var FADE_PX            = 24;   // max fade width in SVG units at a clipped edge

  function svgEl(tag) {
    return document.createElementNS('http://www.w3.org/2000/svg', tag);
  }

  function svgRect(x, y, w, h, style) {
    var r = svgEl('rect');
    r.setAttribute('x', x);
    r.setAttribute('y', y);
    r.setAttribute('width', Math.max(w, 0));
    r.setAttribute('height', h);
    if (style) r.setAttribute('style', style);
    return r;
  }

  function svgLine(x1, y1, x2, y2, style) {
    var l = svgEl('line');
    l.setAttribute('x1', x1); l.setAttribute('y1', y1);
    l.setAttribute('x2', x2); l.setAttribute('y2', y2);
    if (style) l.setAttribute('style', style);
    return l;
  }

  function svgText(x, y, text, style) {
    var t = svgEl('text');
    t.setAttribute('x', x);
    t.setAttribute('y', y);
    if (style) t.setAttribute('style', style);
    t.textContent = text;
    return t;
  }

  function parseDate(s) {
    if (!s) return null;
    var parts = s.split('-');
    return new Date(+parts[0], +parts[1] - 1, +parts[2]);
  }

  function daysBetween(a, b) {
    return Math.round((b - a) / 86400000);
  }

  function addDays(d, n) {
    return new Date(d.getTime() + n * 86400000);
  }

  function fmtDate(d) {
    var m = String(d.getMonth() + 1).padStart(2, '0');
    var day = String(d.getDate()).padStart(2, '0');
    return m + '-' + day;
  }

  function todayDate() {
    var now = new Date();
    return new Date(now.getFullYear(), now.getMonth(), now.getDate());
  }

  function todayViewStart(rangeDays) {
    var pastDays = Math.round(rangeDays * DEFAULT_PAST_DAYS / DEFAULT_RANGE_DAYS);
    return addDays(todayDate(), -pastDays);
  }

  function renderGantt(mount, tasks, viewStart, rangeDays) {
    mount.innerHTML = '';

    var viewEnd = addDays(viewStart, rangeDays);

    // Slide boundary detection — per spec:
    //   tasks with no end_date are NOT "in the future"
    //   tasks with no start_date are NOT "in the past"
    var canGoBack    = tasks.some(function (t) { return t._startDate && t._startDate < viewStart; });
    var canGoForward = tasks.some(function (t) { return t._endDate   && t._endDate   > viewEnd;  });

    // ── Controls ────────────────────────────────────────────────────────────────
    var ctrl = document.createElement('div');
    ctrl.className = 'gv-controls';

    var navGroup = document.createElement('div');
    navGroup.className = 'gv-nav';

    var prevBtn = document.createElement('button');
    prevBtn.className = 'gv-btn';
    prevBtn.textContent = '← Prev';
    if (!canGoBack) prevBtn.setAttribute('disabled', '');

    var todayBtn = document.createElement('button');
    todayBtn.className = 'gv-btn gv-btn--today';
    todayBtn.textContent = 'Today';

    var nextBtn = document.createElement('button');
    nextBtn.className = 'gv-btn';
    nextBtn.textContent = 'Next →';
    if (!canGoForward) nextBtn.setAttribute('disabled', '');

    navGroup.appendChild(prevBtn);
    navGroup.appendChild(todayBtn);
    navGroup.appendChild(nextBtn);

    var rangeGroup = document.createElement('div');
    rangeGroup.className = 'gv-range-group';

    var rangeLabel = document.createElement('label');
    rangeLabel.className = 'gv-range-label';
    rangeLabel.textContent = 'Show:';

    var rangeSelect = document.createElement('select');
    rangeSelect.className = 'gv-range-select';
    RANGE_OPTIONS.forEach(function (d) {
      var opt = document.createElement('option');
      opt.value = d;
      opt.textContent = d + ' days';
      if (d === rangeDays) opt.selected = true;
      rangeSelect.appendChild(opt);
    });

    rangeGroup.appendChild(rangeLabel);
    rangeGroup.appendChild(rangeSelect);
    ctrl.appendChild(navGroup);
    ctrl.appendChild(rangeGroup);
    mount.appendChild(ctrl);

    var slideAmt = Math.max(1, Math.floor(rangeDays / 2));
    prevBtn.addEventListener('click', function () {
      renderGantt(mount, tasks, addDays(viewStart, -slideAmt), rangeDays);
    });
    todayBtn.addEventListener('click', function () {
      renderGantt(mount, tasks, todayViewStart(rangeDays), rangeDays);
    });
    nextBtn.addEventListener('click', function () {
      renderGantt(mount, tasks, addDays(viewStart, slideAmt), rangeDays);
    });
    rangeSelect.addEventListener('change', function () {
      var newRange = +rangeSelect.value;
      renderGantt(mount, tasks, todayViewStart(newRange), newRange);
    });

    // ── Task filtering and grouping ──────────────────────────────────────────────
    // Tasks with no dates at all are always visible (they span the whole viewport
    // with fades on both sides). Partial-date tasks are shown extending to the
    // viewport edge on the dateless side, with a fade there too.
    var renderTasks = tasks.filter(function (t) {
      var s = t._startDate, e = t._endDate;
      if (!s && !e) return true;
      if (s && e)   return s < viewEnd && e > viewStart;
      if (s)        return s < viewEnd;
      return e > viewStart;
    });

    if (!renderTasks.length) {
      var empty = document.createElement('p');
      empty.className = 'gv-empty';
      empty.textContent = 'No tasks with dates in this range.';
      mount.appendChild(empty);
      return;
    }

    var seen = {}, assignees = [];
    renderTasks.forEach(function (t) {
      if (t.assigned_to && !seen[t.assigned_to]) {
        seen[t.assigned_to] = true;
        assignees.push(t.assigned_to);
      }
    });
    var hasUnassigned = renderTasks.some(function (t) { return !t.assigned_to; });
    if (hasUnassigned) assignees.push('');

    var byAssignee = {};
    assignees.forEach(function (a) { byAssignee[a] = []; });
    renderTasks.forEach(function (t) { byAssignee[t.assigned_to || ''].push(t); });

    var totalRows = 0;
    assignees.forEach(function (a) { totalRows += 1 + byAssignee[a].length; });

    // ── SVG sizing ──────────────────────────────────────────────────────────────
    // Attach the SVG to the DOM first with CSS-driven width so the browser can
    // compute the actual available width via getBoundingClientRect (synchronous
    // reflow).  CSS rule: width:100% fills the container; min-width ensures the
    // minimum px-per-day so the scrollbar appears when needed.
    var minSvgW = LABEL_W + rangeDays * MIN_PX_DAY;
    var svgH    = HDR_H + totalRows * ROW_H + 4;

    var svg = svgEl('svg');
    svg.setAttribute('height', svgH);
    svg.setAttribute('class',  'gv-svg');
    svg.style.width    = '100%';
    svg.style.minWidth = minSvgW + 'px';

    var wrapper = document.createElement('div');
    wrapper.className = 'gv-scroll';
    wrapper.appendChild(svg);
    mount.appendChild(wrapper);

    // getBoundingClientRect forces a synchronous reflow, giving the actual
    // rendered width regardless of when in the page lifecycle we run.
    var svgW = Math.round(svg.getBoundingClientRect().width);
    if (svgW < minSvgW) svgW = minSvgW; // guard: layout not ready yet

    // Set explicit width + viewBox so SVG internal coordinates match display size
    svg.setAttribute('width',   svgW);
    svg.setAttribute('viewBox', '0 0 ' + svgW + ' ' + svgH);

    var pxDay = (svgW - LABEL_W) / rangeDays;

    // ── Draw SVG contents ────────────────────────────────────────────────────────
    var defs = svgEl('defs');
    svg.appendChild(defs);

    svg.appendChild(svgRect(0, 0, svgW, svgH, 'fill:var(--color-bg)'));

    // Week grid lines + date labels in header
    var cur = new Date(viewStart);
    while (cur.getDay() !== 1) cur = addDays(cur, 1);
    while (cur < viewEnd) {
      var gx = LABEL_W + daysBetween(viewStart, cur) * pxDay;
      svg.appendChild(svgLine(gx, HDR_H, gx, svgH,
        'stroke:var(--color-border);stroke-width:1'));
      svg.appendChild(svgText(gx + 3, HDR_H - 6, fmtDate(cur),
        'fill:var(--color-muted);font-size:10px'));
      cur = addDays(cur, 7);
    }

    svg.appendChild(svgLine(0, HDR_H, svgW, HDR_H,
      'stroke:var(--color-border);stroke-width:1'));
    svg.appendChild(svgLine(LABEL_W, 0, LABEL_W, svgH,
      'stroke:var(--color-border);stroke-width:1'));

    var rowIdx = 0;
    assignees.forEach(function (assignee) {
      var gy = HDR_H + rowIdx * ROW_H;
      svg.appendChild(svgRect(0, gy, svgW, ROW_H, 'fill:var(--color-surface)'));
      svg.appendChild(svgText(8, gy + ROW_H * 0.65,
        assignee || 'Unassigned',
        'fill:var(--color-accent);font-size:11px;font-weight:700;letter-spacing:0.05em'));
      rowIdx++;

      byAssignee[assignee].forEach(function (task) {
        var ry  = HDR_H + rowIdx * ROW_H;
        var s   = task._startDate;
        var e   = task._endDate;
        // Clamp bar to viewport edges; null date means the bar extends to that edge
        var cs  = (s && s > viewStart) ? s : viewStart;
        var ce  = (e && e < viewEnd)   ? e : viewEnd;
        var bx  = LABEL_W + daysBetween(viewStart, cs) * pxDay;
        var bw  = Math.max(4, daysBetween(cs, ce) * pxDay);
        var clr = task.color || '#7aa2d4';

        // Fade at any edge where the bar is unbounded: either because the task
        // has no date on that side, or because the real date is clipped off-screen.
        var fadeL = !s || s < viewStart;
        var fadeR = !e || e > viewEnd;

        var lbl = task.title.length > 22 ? task.title.slice(0, 20) + '…' : task.title;
        svg.appendChild(svgText(LABEL_W - 8, ry + ROW_H * 0.65, lbl,
          'fill:var(--color-text);font-size:12px;text-anchor:end'));

        var barFill;
        if (fadeL || fadeR) {
          var gradId  = 'grd-' + rowIdx;
          var grad    = svgEl('linearGradient');
          grad.setAttribute('id', gradId);
          grad.setAttribute('gradientUnits', 'userSpaceOnUse');
          grad.setAttribute('x1', bx);
          grad.setAttribute('y1', 0);
          grad.setAttribute('x2', bx + bw);
          grad.setAttribute('y2', 0);

          var fadePx   = Math.min(FADE_PX, bw * 0.35);
          var stopData = [];
          if (fadeL) {
            stopData.push({ off: 0,             op: 0 });
            stopData.push({ off: fadePx / bw,   op: 1 });
          } else {
            stopData.push({ off: 0, op: 1 });
          }
          if (fadeR) {
            stopData.push({ off: 1 - fadePx / bw, op: 1 });
            stopData.push({ off: 1,                op: 0 });
          } else {
            stopData.push({ off: 1, op: 1 });
          }
          stopData.forEach(function (sd) {
            var stop = svgEl('stop');
            stop.setAttribute('offset', (sd.off * 100).toFixed(1) + '%');
            stop.setAttribute('style', 'stop-color:' + clr + ';stop-opacity:' + sd.op);
            grad.appendChild(stop);
          });
          defs.appendChild(grad);
          barFill = 'url(#' + gradId + ')';
        } else {
          barFill = clr;
        }

        var bar = svgRect(bx, ry + ROW_H * 0.2, bw, ROW_H * 0.6,
          'fill:' + barFill + ';opacity:0.88');
        bar.setAttribute('rx', 3);
        svg.appendChild(bar);

        svg.appendChild(svgLine(0, ry + ROW_H, svgW, ry + ROW_H,
          'stroke:var(--color-border);stroke-width:0.5'));

        rowIdx++;
      });
    });

    // Today line — drawn last so it sits on top of all bars
    var today = todayDate();
    if (today >= viewStart && today <= viewEnd) {
      var tx = LABEL_W + daysBetween(viewStart, today) * pxDay;
      svg.appendChild(svgLine(tx, HDR_H, tx, svgH,
        'stroke:#e05252;stroke-width:2;opacity:0.85'));
    }
  }

  window.initGantt = function (mountId, tasks) {
    var mount = document.getElementById(mountId);
    if (!mount) return;

    // Pre-parse dates once; used for rendering and slide boundary checks
    tasks.forEach(function (t) {
      t._startDate = parseDate(t.start_date);
      t._endDate   = parseDate(t.end_date);
    });

    renderGantt(mount, tasks, todayViewStart(DEFAULT_RANGE_DAYS), DEFAULT_RANGE_DAYS);
  };
}());
