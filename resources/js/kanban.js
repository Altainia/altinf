(function () {
  'use strict';

  var COLUMNS = [
    { id: 'todo',        label: 'To Do' },
    { id: 'in_progress', label: 'In Progress' },
    { id: 'review',      label: 'Review' },
    { id: 'done',        label: 'Done' },
  ];

  // Keep in sync with $bp-mobile in resources/scss/_variables.scss.
  var MOBILE_BP = 768;
  function isMobile() { return window.innerWidth <= MOBILE_BP; }

  // Tracks per-mount resize listeners so refresh() re-inits don't stack them.
  var resizeHandlers = {};

  // Remembers the active mobile column per mount so a refresh (e.g. after a move)
  // keeps you on the column you were viewing instead of snapping back to To Do.
  var activeColumns = {};

  function escHtml(s) {
    return String(s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function triggerCallback(cbId, payload) {
    var el = document.getElementById(cbId);
    if (!el) return;
    // Always change value so Wt fires changed() even for repeated payloads.
    el.value = '';
    el.value = payload;
    el.dispatchEvent(new Event('change'));
  }

  function colAccepts(statusId, canMoveColumns, canMoveDone) {
    return (statusId === 'done') ? canMoveDone : canMoveColumns;
  }

  // Compact menu (mobile) listing the statuses a card can be moved to.
  function openMoveMenu(anchor, task, byStatus, canMoveColumns, canMoveDone, cbId) {
    closeMoveMenu();

    var menu = document.createElement('div');
    menu.className = 'kb-move-menu';

    COLUMNS.forEach(function (col) {
      if (col.id === task.status) return;
      // A move into or out of 'done' requires done-move rights; otherwise
      // column-move rights. The card itself is only movable when its own
      // column accepts moves, so gate the targets the same way.
      var allowed = (col.id === 'done' || task.status === 'done')
        ? canMoveDone : canMoveColumns;
      if (!allowed) return;

      var item = document.createElement('button');
      item.type = 'button';
      item.className = 'kb-move-item';
      item.textContent = col.label;
      item.addEventListener('click', function (e) {
        e.stopPropagation();
        var sortOrder = (byStatus[col.id] || []).length;
        triggerCallback(cbId, 'MOVE:' + task.id + ':' + col.id + ':' + sortOrder);
        closeMoveMenu();
      });
      menu.appendChild(item);
    });

    if (!menu.children.length) return;

    anchor.appendChild(menu);

    // Dismiss on any outside tap (mirrors the popup dismiss pattern).
    setTimeout(function () {
      document.addEventListener('mousedown', onOutsideMoveMenu, true);
      document.addEventListener('touchstart', onOutsideMoveMenu, true);
    }, 0);
  }

  function onOutsideMoveMenu(e) {
    var menu = document.querySelector('.kb-move-menu');
    if (menu && !menu.contains(e.target) && !e.target.classList.contains('kb-card-move')) {
      closeMoveMenu();
    }
  }

  function closeMoveMenu() {
    var menu = document.querySelector('.kb-move-menu');
    if (menu && menu.parentNode) { menu.parentNode.removeChild(menu); }
    document.removeEventListener('mousedown', onOutsideMoveMenu, true);
    document.removeEventListener('touchstart', onOutsideMoveMenu, true);
  }

  function makeCard(task, canMoveColumns, canMoveDone, cbId, byStatus) {
    var card = document.createElement('div');
    card.className = 'kb-card';
    card.dataset.id = task.id;

    if (task.color) {
      card.style.borderLeftColor = task.color;
    }

    var canDrag = colAccepts(task.status, canMoveColumns, canMoveDone);
    var _dragged = false;
    // Desktop drag-and-drop is unchanged; mobile uses the quick-move menu instead.
    if (canDrag && !isMobile()) {
      card.setAttribute('draggable', 'true');
      card.addEventListener('dragstart', function (e) {
        _dragged = true;
        e.dataTransfer.setData('text/plain', String(task.id));
        e.dataTransfer.effectAllowed = 'move';
        card.classList.add('kb-card--dragging');
      });
      card.addEventListener('dragend', function () {
        card.classList.remove('kb-card--dragging');
        setTimeout(function () { _dragged = false; }, 0);
        document.querySelectorAll('.kb-column--over').forEach(function (c) {
          c.classList.remove('kb-column--over');
        });
      });
    }

    card.addEventListener('click', function () {
      if (_dragged) return;
      // On mobile, navigate to the full-page editor; on desktop, open the popup.
      triggerCallback(cbId, (isMobile() ? 'NAV:' : 'EDIT:') + task.id);
    });

    var title = document.createElement('div');
    title.className = 'kb-card-title';
    title.textContent = task.title;
    card.appendChild(title);

    if (task.assignees && task.assignees.length > 0) {
      var assignee = document.createElement('div');
      assignee.className = 'kb-card-assignee';
      task.assignees.forEach(function (a, i) {
        if (i > 0) assignee.appendChild(document.createTextNode(', '));
        var span = document.createElement('span');
        span.textContent = a.name;
        if (a.deleted) span.className = 'user-deleted';
        assignee.appendChild(span);
      });
      card.appendChild(assignee);
    }

    if (task.start_date || task.end_date) {
      var dates = document.createElement('div');
      dates.className = 'kb-card-dates';
      dates.textContent = (task.start_date || '?') + ' – ' + (task.end_date || '?');
      card.appendChild(dates);
    }

    // Mobile quick-move affordance (replaces drag-and-drop for moving columns).
    if (canDrag && isMobile()) {
      var move = document.createElement('button');
      move.type = 'button';
      move.className = 'kb-card-move';
      move.setAttribute('aria-label', 'Move task');
      move.textContent = '⋯'; // ⋯
      move.addEventListener('click', function (e) {
        e.stopPropagation();
        if (card.querySelector('.kb-move-menu')) { closeMoveMenu(); return; }
        openMoveMenu(card, task, byStatus, canMoveColumns, canMoveDone, cbId);
      });
      card.appendChild(move);
    }

    return card;
  }

  window.initKanban = function (mountId, tasks, cbId, canMoveColumns, canMoveDone) {
    var mount = document.getElementById(mountId);
    if (!mount) return;
    closeMoveMenu();
    mount.innerHTML = '';
    mount.className = isMobile() ? 'kb-board kb-board--mobile' : 'kb-board';

    var byStatus = {};
    COLUMNS.forEach(function (c) { byStatus[c.id] = []; });
    tasks.forEach(function (t) {
      var col = byStatus[t.status] || byStatus['todo'];
      col.push(t);
    });

    var mobile = isMobile();
    var columnEls = {};

    // On mobile, a segmented control switches which single column is shown.
    var tabsEl = null;
    if (mobile) {
      tabsEl = document.createElement('div');
      tabsEl.className = 'kb-status-tabs';
      mount.appendChild(tabsEl);
    }

    COLUMNS.forEach(function (col, idx) {
      var colEl = document.createElement('div');
      colEl.className = 'kb-column';
      colEl.dataset.status = col.id;

      var hdr = document.createElement('div');
      hdr.className = 'kb-col-header';
      hdr.innerHTML =
        '<span class="kb-col-title">' + escHtml(col.label) + '</span>' +
        '<span class="kb-col-count">' + byStatus[col.id].length + '</span>';
      colEl.appendChild(hdr);

      var cards = document.createElement('div');
      cards.className = 'kb-cards';
      byStatus[col.id].forEach(function (task) {
        cards.appendChild(makeCard(task, canMoveColumns, canMoveDone, cbId, byStatus));
      });
      colEl.appendChild(cards);

      // Drag-and-drop targets only on desktop.
      if (!mobile && colAccepts(col.id, canMoveColumns, canMoveDone)) {
        colEl.addEventListener('dragover', function (e) {
          e.preventDefault();
          e.dataTransfer.dropEffect = 'move';
          colEl.classList.add('kb-column--over');
        });
        colEl.addEventListener('dragleave', function (e) {
          if (!colEl.contains(e.relatedTarget)) {
            colEl.classList.remove('kb-column--over');
          }
        });
        colEl.addEventListener('drop', function (e) {
          e.preventDefault();
          colEl.classList.remove('kb-column--over');
          var taskId = e.dataTransfer.getData('text/plain');
          var newStatus = col.id;
          var sortOrder = cards.querySelectorAll('.kb-card').length;
          triggerCallback(cbId, 'MOVE:' + taskId + ':' + newStatus + ':' + sortOrder);
        });
      }

      mount.appendChild(colEl);
      columnEls[col.id] = colEl;

      if (mobile) {
        var tab = document.createElement('button');
        tab.type = 'button';
        tab.className = 'kb-status-tab';
        tab.dataset.status = col.id;
        tab.innerHTML =
          escHtml(col.label) +
          ' <span class="kb-status-tab-count">' + byStatus[col.id].length + '</span>';
        tab.addEventListener('click', function () { showColumn(col.id); });
        tabsEl.appendChild(tab);
      }
    });

    function showColumn(statusId) {
      activeColumns[mountId] = statusId;
      COLUMNS.forEach(function (col) {
        var active = (col.id === statusId);
        if (columnEls[col.id]) {
          columnEls[col.id].classList.toggle('kb-column--hidden', !active);
        }
      });
      if (tabsEl) {
        tabsEl.querySelectorAll('.kb-status-tab').forEach(function (t) {
          t.classList.toggle('kb-status-tab--active', t.dataset.status === statusId);
        });
      }
      closeMoveMenu();
    }

    if (mobile) { showColumn(activeColumns[mountId] || 'todo'); }

    // Re-render when crossing the mobile/desktop breakpoint (rotation/resize).
    // Dedup per mount so live-update refreshes don't stack listeners.
    if (resizeHandlers[mountId]) {
      window.removeEventListener('resize', resizeHandlers[mountId]);
    }
    var lastMobile = mobile;
    var timer = null;
    var handler = function () {
      clearTimeout(timer);
      timer = setTimeout(function () {
        if (isMobile() !== lastMobile) {
          lastMobile = isMobile();
          window.initKanban(mountId, tasks, cbId, canMoveColumns, canMoveDone);
        }
      }, 150);
    };
    resizeHandlers[mountId] = handler;
    window.addEventListener('resize', handler);
  };
}());
