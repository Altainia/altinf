(function () {
  'use strict';

  var COLUMNS = [
    { id: 'todo',        label: 'To Do' },
    { id: 'in_progress', label: 'In Progress' },
    { id: 'review',      label: 'Review' },
    { id: 'done',        label: 'Done' },
  ];

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

  function makeCard(task, canMoveColumns, canMoveDone, cbId) {
    var card = document.createElement('div');
    card.className = 'kb-card';
    card.dataset.id = task.id;

    if (task.color) {
      card.style.borderLeftColor = task.color;
    }

    var canDrag = (task.status === 'done') ? canMoveDone : canMoveColumns;
    var _dragged = false;
    if (canDrag) {
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
      if (!_dragged) { triggerCallback(cbId, 'EDIT:' + task.id); }
    });

    var title = document.createElement('div');
    title.className = 'kb-card-title';
    title.textContent = task.title;
    card.appendChild(title);

    if (task.assignees && task.assignees.length > 0) {
      var assignee = document.createElement('div');
      assignee.className = 'kb-card-assignee';
      assignee.textContent = task.assignees.join(', ');
      card.appendChild(assignee);
    }

    if (task.start_date || task.end_date) {
      var dates = document.createElement('div');
      dates.className = 'kb-card-dates';
      dates.textContent = (task.start_date || '?') + ' – ' + (task.end_date || '?');
      card.appendChild(dates);
    }

    return card;
  }

  window.initKanban = function (mountId, tasks, cbId, canMoveColumns, canMoveDone) {
    var mount = document.getElementById(mountId);
    if (!mount) return;
    mount.innerHTML = '';
    mount.className = 'kb-board';

    var byStatus = {};
    COLUMNS.forEach(function (c) { byStatus[c.id] = []; });
    tasks.forEach(function (t) {
      var col = byStatus[t.status] || byStatus['todo'];
      col.push(t);
    });

    COLUMNS.forEach(function (col) {
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
        cards.appendChild(makeCard(task, canMoveColumns, canMoveDone, cbId));
      });
      colEl.appendChild(cards);

      var colAcceptsDrop = (col.id === 'done') ? canMoveDone : canMoveColumns;
      if (colAcceptsDrop) {
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
    });
  };
}());
