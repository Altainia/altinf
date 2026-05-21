# AltInf URL Routing Refactor — Step 2

**Date:** 2026-05-21
**Branch:** refactoring (continue from step 1)
**Scope:** Domain-prefixed URLs, centralized `paths.hpp`, domain-method dispatch in `altinf_app`, page/class renames, shared not-found/forbidden widgets.

---

## Goal

Replace the current flat/mixed URL structure with domain-aligned prefixes (`/blog/`, `/link/`, `/org/`, `/team/`, `/task/`, `/admin/`). Centralize all path constants and named builders in `src/paths.hpp`. Restructure `altinf_app::handle_path` into a lean dispatcher that delegates to per-domain private methods. Rename page files/classes that no longer match their role.

---

## Constraints

- C++20 `std::format` is used for all path construction — no manual string concatenation.
- `src/paths.hpp` is header-only (all `inline` functions and `constexpr` constants).
- All named path builder functions used at more than one call site live in `paths.hpp`.
- `altinf_app` remains the single owner of DB handles and session state; domain handlers are private member methods.
- Build must compile cleanly and all three test suites must pass after each task.
- E2E tests update in the final task.

---

## URL Inventory

### Before → After

| Current | New |
|---|---|
| `/blog` | `/blog/list` |
| `/blog/<slug>` | `/blog/view/<slug>` |
| `/admin/new` | `/blog/new` |
| `/admin/edit/<slug>` | `/blog/edit/<slug>` |
| `/links` | `/link/list` |
| `/admin/links/new` | `/link/new` |
| `/admin/links/edit/<id>` | `/link/edit/<id>` |
| `/admin/accounts` | `/admin/account/list` |
| `/admin/accounts/new` | `/admin/account/new` |
| `/admin/accounts/edit/<username>` | `/admin/account/edit/<username>` |
| `/admin/org` | `/admin/org/list` |
| `/org/<id>` | `/org/view/<id>` |
| `/org/<id>/board` | `/org/view/<id>/board` |
| `/org/<id>/manage` | `/org/edit/<id>` |
| `/org/<id>/types` | `/org/view/<id>/types` |
| `/board/<id>` (kanban) | `/team/view/<id>/kanban` |
| `/board/<id>/gantt` | `/team/view/<id>/gantt` |
| `/board/<id>/task/new` | `/team/view/<id>/task/new` |
| `/board/<id>/task/<tid>/edit` | `/task/edit/<tid>` |
| `/board/<id>/archive` | `/team/view/<id>/archive` |
| `/board/<id>/manage` | `/team/edit/<id>/members` |
| `/board/<id>/settings` | `/team/edit/<id>/settings` |
| `/notifications` | `/notifications` (unchanged) |
| `/settings` | `/settings` (unchanged) |
| `/login` | `/login` (unchanged) |
| `/logout` | `/logout` (unchanged) |

### Redirect rules (bare domain roots)

Bare roots with no action segment redirect to their canonical URL:

| Path | Redirects to |
|---|---|
| `/blog` | `/blog/list` |
| `/link` | `/link/list` |
| `/admin/account` | `/admin/account/list` |
| `/admin/org` | `/admin/org/list` |
| `/team/edit/<id>` | `/team/edit/<id>/members` |

`/org/view/<id>` with no further segment is the org landing page (no redirect needed).

### Notes on specific routes

- **`/task/edit/<id>`** — task has a `team_id` field in the DB; the handler loads the task, resolves team context from it, and passes the back-URL `paths::team_kanban(task.team_id)` to `task_edit_page`.
- **`/team/view/<id>`** (bare, no sub-segment) — reserved for a future team landing page; currently redirects to `/team/view/<id>/kanban`.
- **`/team/edit/<id>/members`** and **`/team/edit/<id>/settings`** — two separate existing pages (`team_edit_page` and `team_settings_page`), not merged.
- **`handle_admin`** dispatches on two sub-domains: the first segment is `"account"` or `"org"`. Within `"account"`: `list`, `new`, `edit/<username>`. Within `"org"`: `list` only (shows `org_admin_page`).

---

## `src/paths.hpp`

Header-only. All constants are `inline constexpr std::string_view`; all builders are `inline` functions returning `std::string` via `std::format`.

```cpp
#pragma once
#include <charconv>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace paths
{
    // ── Domain prefixes (include trailing slash) ──────────────────────────────
    inline constexpr std::string_view blog_prefix  = "/blog/";
    inline constexpr std::string_view link_prefix  = "/link/";
    inline constexpr std::string_view org_prefix   = "/org/";
    inline constexpr std::string_view team_prefix  = "/team/";
    inline constexpr std::string_view task_prefix  = "/task/";
    inline constexpr std::string_view admin_prefix = "/admin/";

    // ── Common action segments ────────────────────────────────────────────────
    inline constexpr std::string_view list_seg = "list";
    inline constexpr std::string_view view_seg = "view";
    inline constexpr std::string_view edit_seg = "edit";
    inline constexpr std::string_view new_seg  = "new";

    // ── Fixed paths ───────────────────────────────────────────────────────────
    inline constexpr std::string_view login_path         = "/login";
    inline constexpr std::string_view logout_path        = "/logout";
    inline constexpr std::string_view notifications_path = "/notifications";
    inline constexpr std::string_view settings_path      = "/settings";

    // ── String_view path helpers ──────────────────────────────────────────────

    // Consume the next slash-delimited segment from sv.
    // Leading '/' is stripped first if present. sv is updated to the remainder
    // (without a leading slash). Returns the consumed segment (empty if sv was empty).
    inline std::string_view take_segment(std::string_view& sv)
    {
        if (!sv.empty() && sv.front() == '/') sv.remove_prefix(1);
        const auto slash = sv.find('/');
        if (slash == sv.npos)
        {
            auto seg = sv;
            sv = {};
            return seg;
        }
        auto seg = sv.substr(0, slash);
        sv = sv.substr(slash + 1);
        return seg;
    }

    // Consume a numeric ID segment. Returns nullopt if absent or non-numeric.
    inline std::optional<long long> take_id(std::string_view& sv)
    {
        const auto seg = take_segment(sv);
        if (seg.empty()) return std::nullopt;
        long long id{};
        const auto* end = seg.data() + seg.size();
        if (std::from_chars(seg.data(), end, id).ptr != end) return std::nullopt;
        return id;
    }

    // ── Named path builders ───────────────────────────────────────────────────

    // Blog
    inline std::string blog_list()                       { return std::format("{}{}",       blog_prefix, list_seg); }
    inline std::string blog_view(std::string_view slug)  { return std::format("{}{}/{}",    blog_prefix, view_seg, slug); }
    inline std::string blog_edit(std::string_view slug)  { return std::format("{}{}/{}",    blog_prefix, edit_seg, slug); }
    inline std::string blog_new()                        { return std::format("{}{}",       blog_prefix, new_seg); }

    // Link
    inline std::string link_list()                       { return std::format("{}{}",       link_prefix, list_seg); }
    inline std::string link_edit(long long id)           { return std::format("{}{}/{}",    link_prefix, edit_seg, id); }
    inline std::string link_new()                        { return std::format("{}{}",       link_prefix, new_seg); }

    // Admin — account
    inline std::string account_list()                          { return std::format("{}account/{}", admin_prefix, list_seg); }
    inline std::string account_new()                           { return std::format("{}account/{}", admin_prefix, new_seg); }
    inline std::string account_edit(std::string_view username) { return std::format("{}account/{}/{}", admin_prefix, edit_seg, username); }

    // Admin — org
    inline std::string admin_org_list()                  { return std::format("{}org/{}", admin_prefix, list_seg); }

    // Org
    inline std::string org_view(long long id)            { return std::format("{}{}/{}",   org_prefix, view_seg, id); }
    inline std::string org_board(long long id)           { return std::format("{}{}/{}/board", org_prefix, view_seg, id); }
    inline std::string org_edit(long long id)            { return std::format("{}{}/{}",   org_prefix, edit_seg, id); }
    inline std::string org_types(long long id)           { return std::format("{}{}/{}/types", org_prefix, view_seg, id); }

    // Team
    inline std::string team_kanban(long long id)         { return std::format("{}{}/{}/kanban",  team_prefix, view_seg, id); }
    inline std::string team_gantt(long long id)          { return std::format("{}{}/{}/gantt",   team_prefix, view_seg, id); }
    inline std::string team_task_new(long long id)       { return std::format("{}{}/{}/task/new",team_prefix, view_seg, id); }
    inline std::string team_archive(long long id)        { return std::format("{}{}/{}/archive", team_prefix, view_seg, id); }
    inline std::string team_edit_members(long long id)   { return std::format("{}{}/{}/members", team_prefix, edit_seg, id); }
    inline std::string team_edit_settings(long long id)  { return std::format("{}{}/{}/settings",team_prefix, edit_seg, id); }

    // Task
    inline std::string task_edit(long long id)           { return std::format("{}{}/{}",   task_prefix, edit_seg, id); }
}
```

---

## `altinf_app` Restructuring

### `handle_path` — dispatcher

```cpp
void altinf_app::handle_path(const std::string& path)
{
    m_content->clear();
    m_notifications_page = nullptr;
    set_wide(false);

    const std::string_view sv{path};

    if (sv == "/" || sv.empty())           { show_main(); return; }
    if (sv == paths::login_path)           { handle_login(); return; }
    if (sv == paths::logout_path)          { handle_logout(); return; }
    if (sv == paths::notifications_path)   { handle_notifications(); return; }
    if (sv == paths::settings_path)        { handle_settings(); return; }

    // Bare domain roots → redirect
    if (sv == "/blog")           { setInternalPath(paths::blog_list(), true); return; }
    if (sv == "/link")           { setInternalPath(paths::link_list(), true); return; }
    if (sv == "/admin/account")  { setInternalPath(paths::account_list(), true); return; }
    if (sv == "/admin/org")      { setInternalPath(paths::admin_org_list(), true); return; }

    if (sv.starts_with(paths::blog_prefix))  { handle_blog(sv.substr(paths::blog_prefix.size())); return; }
    if (sv.starts_with(paths::link_prefix))  { handle_link(sv.substr(paths::link_prefix.size())); return; }
    if (sv.starts_with(paths::org_prefix))   { handle_org(sv.substr(paths::org_prefix.size())); return; }
    if (sv.starts_with(paths::team_prefix))  { handle_team(sv.substr(paths::team_prefix.size())); return; }
    if (sv.starts_with(paths::task_prefix))  { handle_task(sv.substr(paths::task_prefix.size())); return; }
    if (sv.starts_with(paths::admin_prefix)) { handle_admin(sv.substr(paths::admin_prefix.size())); return; }

    show_not_found();
}
```

### New private methods

```cpp
// Domain handlers
void handle_blog(std::string_view rem);
void handle_link(std::string_view rem);
void handle_org(std::string_view rem);
void handle_team(std::string_view rem);
void handle_task(std::string_view rem);
void handle_admin(std::string_view rem);

// Fixed-path handlers (extracted from handle_path for clarity)
void handle_login();
void handle_logout();
void handle_notifications();
void handle_settings();
void show_main();

// Layout helper
void set_wide(bool wide);
```

`set_wide(true)` is called inside `handle_team` when dispatching to kanban/gantt views, and inside `handle_org` when dispatching to the org board. This replaces the current string-inspection hack at the top of `handle_path`.

---

## Shared Widgets

```
src/widgets/not_found_widget.cpp/.hpp
src/widgets/forbidden_widget.cpp/.hpp
```

`altinf_app::show_not_found()` and `show_forbidden()` instantiate these instead of bare `Wt::WText`. Styling is deferred — the widgets start as simple text containers, allowing CSS to be added later without touching every call site.

---

## Page/Class Renames

All renames are `git mv` (file + class name). Includes in all consumers update accordingly.

| Old | New | Location |
|---|---|---|
| `blog_page` | `blog_list_page` | `src/blog/pages/` |
| `blog_post_page` | `blog_view_page` | `src/blog/pages/` |
| `post_editor_page` | `blog_edit_page` | `src/blog/pages/` |
| `links_page` | `link_list_page` | `src/link/pages/` |
| `link_editor_page` | `link_edit_page` | `src/link/pages/` |
| `account_manager_page` | `account_list_page` | `src/admin/account/pages/` |
| `account_editor_page` | `account_edit_page` | `src/admin/account/pages/` |
| `kanban_board_page` | `team_kanban_page` | `src/org/pages/` |
| `kanban_task_editor_page` | `task_edit_page` | `src/org/pages/` |
| `kanban_archive_page` | `team_archive_page` | `src/org/pages/` |
| `kanban_team_page` | `team_edit_page` | `src/org/pages/` |

`team_settings_page`, `org_landing_page`, `org_board_page`, `org_admin_page`, `org_type_manager_page`, `notifications_page`, `settings_page` keep their names.

---

## Consumer Updates

Every `Wt::WAnchor`, `Wt::WLink`, and `setInternalPath` call that hard-codes a path string is replaced with the corresponding `paths::` builder or constant. Full call-site inventory:

### `src/widgets/nav_bar.cpp`
- `"/links"` → `paths::link_list()`
- `"/blog"` → `paths::blog_list()`
- `"/login"` → `std::string{paths::login_path}`
- `"/logout"` → `std::string{paths::logout_path}`
- `"/admin/new"` → `paths::blog_new()`
- `"/admin/accounts"` → `paths::account_list()`
- `"/admin/org"` → `paths::admin_org_list()`
- `"/settings"` → `std::string{paths::settings_path}`
- `"/org/" + id` → `paths::org_view(id)`

### `src/org/pages/team_kanban_page.cpp`
- `team_url = "/board/" + id` → `paths::team_kanban(id)`
- `+ "/gantt"` → `paths::team_gantt(id)`
- `+ "/task/new"` → `paths::team_task_new(id)`
- `+ "/manage"` → `paths::team_edit_members(id)`
- `+ "/settings"` → `paths::team_edit_settings(id)`
- `+ "/archive"` → `paths::team_archive(id)`

### `src/org/pages/team_archive_page.cpp`
- `board_url` → `paths::team_kanban(team_id)`
- edit URL → `paths::task_edit(task_id)`

### `src/org/pages/task_edit_page.cpp`
- `board_url` → `paths::team_kanban(team_id)`

### `src/org/pages/team_settings_page.cpp`
- `board_url` → `paths::team_kanban(team_id)`

### `src/org/pages/org_landing_page.cpp`
- `/org/<id>/manage` → `paths::org_edit(id)`
- `/org/<id>/board` → `paths::org_board(id)`
- `/org/<id>/types` → `paths::org_types(id)`
- `/board/<id>` → `paths::team_kanban(id)`

### `src/org/pages/org_board_page.cpp`
- `/org/<id>` → `paths::org_view(id)`
- `/board/<id>` → `paths::team_kanban(id)`
- task edit URL → `paths::task_edit(tid)`

### `src/org/pages/team_edit_page.cpp`
- back URL → `paths::team_kanban(id)` or `paths::org_view(id)`
- `/board/<id>/settings` → `paths::team_edit_settings(id)`

### `src/org/pages/notifications_page.cpp`
- All 6 task link URLs → `paths::task_edit(tid)`

### `src/org/pages/org_admin_page.cpp`
- `/org/<id>` → `paths::org_view(id)`

### `src/org/pages/org_type_manager_page.cpp`
- back URL → `paths::org_view(id)`

### `src/org/widgets/task_popup_widget.cpp`
- edit URL → `paths::task_edit(tid)`

### `src/link/pages/link_list_page.cpp`
- `"/admin/links/new"` → `paths::link_new()`
- `"/admin/links/edit/" + id` → `paths::link_edit(id)`

### `src/link/pages/link_edit_page.cpp`
- `"/links"` → `paths::link_list()`

### `src/blog/pages/blog_list_page.cpp`
- `"/blog/" + slug` → `paths::blog_view(slug)`

### `src/blog/pages/blog_view_page.cpp`
- `"/blog"` → `paths::blog_list()`
- `"/admin/edit/" + slug` → `paths::blog_edit(slug)`

### `src/blog/pages/blog_edit_page.cpp`
- cancel path → `paths::blog_list()` or `paths::blog_view(slug)`
- post-save redirect → `paths::blog_view(slug)`

### `src/admin/account/pages/account_list_page.cpp`
- `"/admin/accounts/new"` → `paths::account_new()`
- `"/admin/accounts/edit/" + u` → `paths::account_edit(u)`

### `src/admin/account/pages/account_edit_page.cpp`
- `"/admin/accounts"` → `paths::account_list()`

### `src/widgets/notification_bell.cpp`
- `"/notifications"` → `std::string{paths::notifications_path}`

### `altinf_app.cpp` (internal navigations)
All `setInternalPath(...)` calls in the login handler, account deletion handler, and task/link callbacks use the appropriate builder.

---

## Execution Order

Each task: rename/create files → fix includes and path strings → build → run all tests → commit.

1. **`paths.hpp` + shared widgets** — create `src/paths.hpp`, `not_found_widget`, `forbidden_widget`; wire into `altinf_app`
2. **Page renames** — `git mv` all 11 page pairs; update includes and class names in all consumers
3. **`altinf_app` restructure** — rewrite `handle_path` as dispatcher + domain methods; update all `setInternalPath` calls in `altinf_app.cpp` to use `paths::` builders
4. **Consumer path updates** — update all `WAnchor`/`WLink`/`setInternalPath` in the 20 consumer files
5. **E2E test updates** — update all path strings in `e2e/`

---

## Testing

After each task:
- `cmake --build` (Catch2 + binary)
- `cd tests/js && npm test`
- `cd e2e && npx playwright test` (E2E failures on old paths are expected until task 5)
