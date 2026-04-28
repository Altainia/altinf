(function () {
  'use strict';

  var ROW_H    = 36;
  var LABEL_W  = 190;
  var HDR_H    = 28;
  var PX_DAY   = 22;   // pixels per day
  var PAD_DAYS = 3;    // blank days added on each side of the date range

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

  window.initGantt = function (mountId, tasks) {
    var mount = document.getElementById(mountId);
    if (!mount) return;
    mount.innerHTML = '';

    if (!tasks.length) {
      var empty = document.createElement('p');
      empty.className = 'gv-empty';
      empty.textContent = 'No tasks with dates to display.';
      mount.appendChild(empty);
      return;
    }

    // Compute date range across all tasks
    var minDate = null, maxDate = null;
    tasks.forEach(function (t) {
      var s = parseDate(t.start_date), e = parseDate(t.end_date);
      if (s && (!minDate || s < minDate)) minDate = s;
      if (e && (!maxDate || e > maxDate)) maxDate = e;
    });

    if (!minDate || !maxDate) {
      var empty2 = document.createElement('p');
      empty2.className = 'gv-empty';
      empty2.textContent = 'No tasks with dates to display.';
      mount.appendChild(empty2);
      return;
    }

    minDate = addDays(minDate, -PAD_DAYS);
    maxDate = addDays(maxDate,  PAD_DAYS);
    var totalDays  = daysBetween(minDate, maxDate);
    var trackWidth = totalDays * PX_DAY;

    // Group by assignee (non-empty first, then empty last)
    var seen = {}, assignees = [];
    tasks.forEach(function (t) {
      if (t.assigned_to && !seen[t.assigned_to]) {
        seen[t.assigned_to] = true;
        assignees.push(t.assigned_to);
      }
    });
    var hasUnassigned = tasks.some(function (t) { return !t.assigned_to; });
    if (hasUnassigned) assignees.push('');

    var byAssignee = {};
    assignees.forEach(function (a) { byAssignee[a] = []; });
    tasks.forEach(function (t) { byAssignee[t.assigned_to || ''].push(t); });

    // Total rows = one group-header row + one row per task, per assignee group
    var totalRows = 0;
    assignees.forEach(function (a) { totalRows += 1 + byAssignee[a].length; });

    var svgW = LABEL_W + trackWidth;
    var svgH = HDR_H + totalRows * ROW_H + 4;

    var svg = svgEl('svg');
    svg.setAttribute('width',  svgW);
    svg.setAttribute('height', svgH);
    svg.setAttribute('class',  'gv-svg');

    // Background
    svg.appendChild(svgRect(0, 0, svgW, svgH, 'fill:var(--color-bg)'));

    // Week grid lines + date labels in header
    var cur = new Date(minDate);
    while (cur.getDay() !== 1) cur = addDays(cur, 1); // advance to first Monday
    while (cur < maxDate) {
      var gx = LABEL_W + daysBetween(minDate, cur) * PX_DAY;
      svg.appendChild(svgLine(gx, HDR_H, gx, svgH,
        'stroke:var(--color-border);stroke-width:1'));
      svg.appendChild(svgText(gx + 3, HDR_H - 6, fmtDate(cur),
        'fill:var(--color-muted);font-size:10px'));
      cur = addDays(cur, 7);
    }

    // Header divider
    svg.appendChild(svgLine(0, HDR_H, svgW, HDR_H,
      'stroke:var(--color-border);stroke-width:1'));

    // Label/track vertical divider
    svg.appendChild(svgLine(LABEL_W, 0, LABEL_W, svgH,
      'stroke:var(--color-border);stroke-width:1'));

    // Rows
    var rowIdx = 0;
    assignees.forEach(function (assignee) {
      // Group header
      var gy = HDR_H + rowIdx * ROW_H;
      svg.appendChild(svgRect(0, gy, svgW, ROW_H, 'fill:var(--color-surface)'));
      svg.appendChild(svgText(8, gy + ROW_H * 0.65,
        assignee || 'Unassigned',
        'fill:var(--color-accent);font-size:11px;font-weight:700;letter-spacing:0.05em'));
      rowIdx++;

      byAssignee[assignee].forEach(function (task) {
        var ry  = HDR_H + rowIdx * ROW_H;
        var s   = parseDate(task.start_date);
        var e   = parseDate(task.end_date);
        var bx  = LABEL_W + daysBetween(minDate, s) * PX_DAY;
        var bw  = Math.max(4, daysBetween(s, e) * PX_DAY);
        var clr = task.color || '#7aa2d4';

        // Task label (truncated)
        var lbl = task.title.length > 22 ? task.title.slice(0, 20) + '…' : task.title;
        svg.appendChild(svgText(LABEL_W - 8, ry + ROW_H * 0.65, lbl,
          'fill:var(--color-text);font-size:12px;text-anchor:end'));

        // Bar
        var bar = svgRect(bx, ry + ROW_H * 0.2, bw, ROW_H * 0.6,
          'fill:' + clr + ';opacity:0.88;rx:3');
        bar.setAttribute('rx', 3);
        svg.appendChild(bar);

        // Row divider
        svg.appendChild(svgLine(0, ry + ROW_H, svgW, ry + ROW_H,
          'stroke:var(--color-border);stroke-width:0.5'));

        rowIdx++;
      });
    });

    // Today line — drawn last so it sits on top of all bars
    var now = new Date();
    var today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    if (today >= minDate && today <= maxDate) {
      var tx = LABEL_W + daysBetween(minDate, today) * PX_DAY;
      svg.appendChild(svgLine(tx, HDR_H, tx, svgH,
        'stroke:#e05252;stroke-width:2;opacity:0.85'));
    }

    var wrapper = document.createElement('div');
    wrapper.className = 'gv-scroll';
    wrapper.appendChild(svg);
    mount.appendChild(wrapper);
  };
}());
