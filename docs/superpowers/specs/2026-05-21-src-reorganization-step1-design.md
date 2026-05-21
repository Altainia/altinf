# AltInf src/ Reorganization — Step 1

**Date:** 2026-05-21
**Branch:** refactoring (new branch off master)
**Scope:** Redistribute all source files under `src/` into domain-aligned directories with `pages/` and `widgets/` subdirectories. Step 1 of a larger refactoring effort.

---

## Goal

The `src/pages/` directory has grown into a catch-all for all UI pages regardless of domain. `src/kanban/` is a mix of widgets, pages, data models, and DB access. This reorganization collapses those into clear domain boundaries so each area of the app owns its full vertical slice.

---

## Constraints

- CMakeLists.txt uses `GLOB_RECURSE` on `src/*.cpp` — no build system changes needed.
- `src/auth/`, `src/api/`, and `src/widgets/` are cross-cutting and stay at the top level unchanged.
- Each file move requires updating `#include` paths in all consumers — both `src/` files and `tests/` Catch2 test files (which use paths like `"kanban/kanban_db.hpp"`).
- Build must compile cleanly after each domain step (Approach B: domain by domain).

---

## Target Structure

### Unchanged

```
src/api/           post_api_resource.cpp/hpp
src/auth/          api_token.hpp, permission.hpp, session_data.hpp,
                   session_token.hpp, user.hpp, user_db.cpp/hpp
src/widgets/       footer.cpp/hpp, live_hub.cpp/hpp, nav_bar.cpp/hpp,
                   notification_bell.cpp/hpp
src/main.cpp
src/altinf_app.cpp/hpp
src/pages/         main_page.cpp/hpp  (shrinks to this only)
```

### src/blog/ — expand in place

```
src/blog/
  blog_loader.cpp/hpp
  blog_post.hpp
  post_writer.cpp/hpp
  pages/
    blog_page.cpp/hpp
    blog_post_page.cpp/hpp
    post_editor_page.cpp/hpp
```

Sources moved from `src/pages/`.

### src/auth/ — add pages subdirectory

```
src/auth/
  api_token.hpp
  permission.hpp
  session_data.hpp
  session_token.hpp
  user.hpp
  user_db.cpp/hpp
  pages/
    login_page.cpp/hpp
```

Source moved from `src/pages/`.

### src/link/ — rename from src/links/, add pages

```
src/link/
  link.hpp
  link_db.cpp/hpp
  pages/
    link_editor_page.cpp/hpp
    links_page.cpp/hpp
```

`src/links/` is deleted after move.

### src/admin/account/ — new, pages only

```
src/admin/account/
  pages/
    account_editor_page.cpp/hpp
    account_manager_page.cpp/hpp
```

Sources moved from `src/pages/`.

### src/org/ — expand; src/kanban/ dissolved into it

```
src/org/
  org.hpp
  org_db.cpp/hpp
  kanban.hpp
  kanban_db.cpp/hpp
  kanban_notifications.cpp/hpp
  team_cap.hpp
  widgets/
    gantt_view_widget.cpp/hpp
    kanban_board_widget.cpp/hpp
    task_editor_form_widget.cpp/hpp
    task_popup_widget.cpp/hpp
  pages/
    org_admin_page.cpp/hpp
    org_board_page.cpp/hpp
    org_landing_page.cpp/hpp
    org_type_manager_page.cpp/hpp
    team_settings_page.cpp/hpp
    kanban_archive_page.cpp/hpp
    kanban_board_page.cpp/hpp
    kanban_task_editor_page.cpp/hpp
    kanban_team_page.cpp/hpp
    notifications_page.cpp/hpp
    settings_page.cpp/hpp
```

`src/kanban/` is deleted after all contents are absorbed.

---

## Execution Order (Approach B — domain by domain)

Each step: move files → fix includes → build → commit.

1. **blog** — move 3 pages into `src/blog/pages/`
2. **auth** — move `login_page` into `src/auth/pages/`
3. **link** — rename `src/links/` to `src/link/`, move 2 pages into `src/link/pages/`
4. **admin/account** — create `src/admin/account/pages/`, move 2 pages
5. **org** — move all kanban files and all org/kanban pages into `src/org/`; delete `src/kanban/`

After step 5, `src/pages/` contains only `main_page.cpp/hpp`.

---

## Testing

After each domain step, run the full test suite:
- `cmake --build` (Catch2 + binary)
- JS unit tests
- Playwright E2E tests

The build passing after each step is the primary correctness signal for a pure file-move refactor.
