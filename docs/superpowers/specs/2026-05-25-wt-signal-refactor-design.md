# Design: Replace std::function callbacks with Wt::Signal

**Date:** 2026-05-25  
**Branch:** refactoring  
**Approach:** All-at-once (A)

## Problem

Eleven widgets and pages accept `std::function` callbacks via their constructors to notify parents of events (save, delete, login, etc.). This pattern has two downsides over `Wt::Signal`: it couples the widget to the existence of a listener at construction time, and it only allows a single consumer. `Wt::Signal` is the idiomatic Wt mechanism for widget-to-parent event notification.

## Scope

### In scope — widgets to migrate

| Widget | Removed constructor params | New public signal members |
|---|---|---|
| `login_page` | `on_login` | `Wt::Signal<> logged_in` |
| `blog_edit_page` | `on_save(slug)` | `Wt::Signal<std::string> saved` |
| `link_edit_page` | `on_save()` | `Wt::Signal<> saved` |
| `link_list_page` | `on_delete(id)` | `Wt::Signal<long long> deleted` |
| `account_edit_page` | `on_save()` | `Wt::Signal<> saved` |
| `account_list_page` | `on_delete(username)` | `Wt::Signal<std::string> deleted` |
| `task_edit_page` | `on_save()` | `Wt::Signal<> saved` |
| `notifications_page` | `on_read()` | `Wt::Signal<> read` |
| `task_editor_form_widget` | `on_saved()`, `on_cancel()` | `Wt::Signal<> saved`, `Wt::Signal<> canceled` |
| `kanban_board_widget` | `on_move(id,status,sort)`, `on_edit(id)` | `Wt::Signal<long long, std::string, int> moved`, `Wt::Signal<long long> edit_requested` |
| `gantt_view_widget` | `on_edit(id)` | `Wt::Signal<long long> edit_requested` |

### Out of scope

`live_hub` uses `std::function` for cross-session, thread-safe broadcast callbacks posted via `WServer::post()`. This is structurally different from widget-to-parent signaling and is left unchanged.

## Design

### Signal declarations

Each affected widget gains public `Wt::Signal<...>` data members and loses its `std::function` constructor parameter(s). The `<functional>` include is removed from each header.

Signals are plain public members (not accessor methods), consistent with the project's preference for directness over Wt's own accessor-wrapping convention.

Naming follows Wt convention: past-tense or noun-form, no `on_` prefix.

Internally, every `m_on_xxx(args...)` call becomes `xxx.emit(args...)`. For widgets where the callback was optional (default `{}`), the null check (`if(m_on_xxx)`) is removed — emitting a signal with no connections is a no-op.

### JS→C++ widgets (kanban_board_widget, gantt_view_widget)

These widgets use a hidden `WLineEdit` as a JS→C++ bridge: JavaScript sets the input's value and fires `change`, which calls back into C++. Currently the `std::function` callbacks are captured by the `WLineEdit::changed()` lambda at construction time rather than stored as members.

After migration, the lambda captures `this` instead and emits the signals directly:

```cpp
// Before
cb->changed().connect(
  [cb, on_move = std::move(on_move), on_edit = std::move(on_edit)]() mutable {
      ...
      on_move(tid, status, sort);
      on_edit(tid);
  });

// After
cb->changed().connect([this, cb]() {
    ...
    moved.emit(tid, status, sort);
    edit_requested.emit(tid);
});
```

`gantt_view_widget::on_edit` was optional (`= {}`). The default parameter is removed; callers simply omit connecting to `edit_requested` if they don't need it.

### Call sites

**`altinf_app.cpp`** (primary call site): all `addNew<Widget>(..., lambda)` calls become a two-step pattern — construct, then connect:

```cpp
// Before
m_content->addNew<login_page>(*m_user_db, m_session, [this] { ... });

// After
auto* page = m_content->addNew<login_page>(*m_user_db, m_session);
page->logged_in.connect([this] { ... });
```

Lambda bodies are unchanged.

**`task_edit_page.cpp`**: constructs `task_editor_form_widget` and passes two lambdas. After migration it connects to `form->saved` and `form->canceled`. `task_edit_page` also loses its own `on_save` constructor param and gains `Wt::Signal<> saved`, which it emits when `form->saved` fires. Cancel behavior (navigate to team kanban) remains internal to `task_edit_page` — it is not surfaced as a signal since no caller ever customizes it.

**`task_popup_widget.cpp`**: constructs `task_editor_form_widget` with `[this] { accept(); }` and `[this] { try_close(); }`. After migration it connects those same lambdas to `form->saved` and `form->canceled`.

## Files changed

**Headers (remove `<functional>`, remove callback params, add signal members):**
- `src/auth/pages/login_page.hpp`
- `src/blog/pages/blog_edit_page.hpp`
- `src/link/pages/link_edit_page.hpp`
- `src/link/pages/link_list_page.hpp`
- `src/admin/account/pages/account_edit_page.hpp`
- `src/admin/account/pages/account_list_page.hpp`
- `src/org/pages/task_edit_page.hpp`
- `src/org/pages/notifications_page.hpp`
- `src/org/widgets/task_editor_form_widget.hpp`
- `src/org/widgets/kanban_board_widget.hpp`
- `src/org/widgets/gantt_view_widget.hpp`

**Implementations (update emit sites, remove stored callback members):**
- `src/auth/pages/login_page.cpp`
- `src/blog/pages/blog_edit_page.cpp`
- `src/link/pages/link_edit_page.cpp`
- `src/link/pages/link_list_page.cpp`
- `src/admin/account/pages/account_edit_page.cpp`
- `src/admin/account/pages/account_list_page.cpp`
- `src/org/pages/task_edit_page.cpp`
- `src/org/pages/notifications_page.cpp`
- `src/org/widgets/task_editor_form_widget.cpp`
- `src/org/widgets/kanban_board_widget.cpp`
- `src/org/widgets/gantt_view_widget.cpp`

**Call sites (connect after construction):**
- `src/altinf_app.cpp`
- `src/org/widgets/task_popup_widget.cpp`
