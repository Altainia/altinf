# URL Routing Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Introduce domain-prefixed URLs (`/blog/`, `/link/`, `/team/`, `/org/`, `/task/`, `/admin/`), a centralized `src/paths.hpp` with named path builders, domain-method dispatch in `altinf_app`, page/class renames, and shared error widgets.

**Architecture:** `src/paths.hpp` is a header-only library of `constexpr` path constants, `std::string_view` segment helpers, and `std::format`-based named builders. `altinf_app::handle_path` becomes a lean dispatcher that strips domain prefixes and delegates to private methods (`handle_blog`, `handle_link`, `handle_org`, `handle_team`, `handle_task`, `handle_admin`). All `WAnchor`/`WLink`/`setInternalPath` call sites use `paths::` builders instead of inline string literals.

**Tech Stack:** C++20 (`std::format`, `std::from_chars`, `std::string_view`), Wt 4.13, Catch2 v3, Playwright.

---

## File Structure

**Create:**
- `src/paths.hpp` — path constants, `take_segment`, `take_id`, named builders
- `src/widgets/not_found_widget.hpp/.cpp` — shared 404 widget
- `src/widgets/forbidden_widget.hpp/.cpp` — shared 403 widget
- `tests/test_paths.cpp` — Catch2 unit tests for `take_segment`, `take_id`, and builders

**Rename (git mv):**
- `src/blog/pages/blog_page` → `blog_list_page`
- `src/blog/pages/blog_post_page` → `blog_view_page`
- `src/blog/pages/post_editor_page` → `blog_edit_page`
- `src/link/pages/links_page` → `link_list_page`
- `src/link/pages/link_editor_page` → `link_edit_page`
- `src/admin/account/pages/account_manager_page` → `account_list_page`
- `src/admin/account/pages/account_editor_page` → `account_edit_page`
- `src/org/pages/kanban_board_page` → `team_kanban_page`
- `src/org/pages/kanban_task_editor_page` → `task_edit_page`
- `src/org/pages/kanban_archive_page` → `team_archive_page`
- `src/org/pages/kanban_team_page` → `team_edit_page`

**Modify:**
- `src/altinf_app.hpp` — add new private method declarations, `#include <string_view>`
- `src/altinf_app.cpp` — full restructure (Tasks 3–4 combined in this file)
- `src/widgets/nav_bar.cpp` — path constants
- `src/org/pages/team_kanban_page.cpp` — path builders (post-rename)
- `src/org/pages/team_archive_page.cpp` — path builders
- `src/org/pages/task_edit_page.cpp` — path builders
- `src/org/pages/team_settings_page.cpp` — path builders
- `src/org/pages/team_edit_page.cpp` — path builders + default back-url fix
- `src/org/pages/org_landing_page.cpp` — path builders
- `src/org/pages/org_board_page.cpp` — path builders
- `src/org/pages/org_admin_page.cpp` — path builders
- `src/org/pages/org_type_manager_page.cpp` — path builders
- `src/org/pages/notifications_page.cpp` — path builders (6 sites)
- `src/org/widgets/task_popup_widget.cpp` — path builders
- `src/link/pages/link_list_page.cpp` — path builders (post-rename)
- `src/link/pages/link_edit_page.cpp` — path builders (post-rename)
- `src/blog/pages/blog_list_page.cpp` — path builders (post-rename)
- `src/blog/pages/blog_view_page.cpp` — path builders (post-rename)
- `src/blog/pages/blog_edit_page.cpp` — path builders (post-rename)
- `src/admin/account/pages/account_list_page.cpp` — path builders (post-rename)
- `src/admin/account/pages/account_edit_page.cpp` — path builders (post-rename)
- `src/widgets/notification_bell.cpp` — path constant
- `tests/CMakeLists.txt` — add `test_paths` target
- `e2e/specs/blog.spec.ts` — URL updates
- `e2e/specs/home.spec.ts` — URL updates
- `e2e/specs/links.spec.ts` — URL updates
- `e2e/specs/board.spec.ts` — URL updates

---

## Task 1: `src/paths.hpp` + shared error widgets

**Files:**
- Create: `tests/test_paths.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `src/paths.hpp`
- Create: `src/widgets/not_found_widget.hpp`
- Create: `src/widgets/not_found_widget.cpp`
- Create: `src/widgets/forbidden_widget.hpp`
- Create: `src/widgets/forbidden_widget.cpp`
- Modify: `src/altinf_app.cpp` (includes + `show_not_found` + `show_forbidden`)

- [ ] **Step 1: Write the failing tests for `paths.hpp`**

Create `tests/test_paths.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "paths.hpp"

TEST_CASE("take_segment - reads up to slash")
{
    std::string_view sv = "view/42/kanban";
    CHECK(paths::take_segment(sv) == "view");
    CHECK(sv == "42/kanban");
}

TEST_CASE("take_segment - last segment consumes all")
{
    std::string_view sv = "kanban";
    CHECK(paths::take_segment(sv) == "kanban");
    CHECK(sv.empty());
}

TEST_CASE("take_segment - empty input returns empty")
{
    std::string_view sv = "";
    CHECK(paths::take_segment(sv).empty());
    CHECK(sv.empty());
}

TEST_CASE("take_segment - strips leading slash first")
{
    std::string_view sv = "/view/42";
    CHECK(paths::take_segment(sv) == "view");
    CHECK(sv == "42");
}

TEST_CASE("take_id - valid numeric segment")
{
    std::string_view sv = "42/kanban";
    const auto id = paths::take_id(sv);
    REQUIRE(id.has_value());
    CHECK(*id == 42);
    CHECK(sv == "kanban");
}

TEST_CASE("take_id - non-numeric returns nullopt")
{
    std::string_view sv = "abc";
    CHECK_FALSE(paths::take_id(sv).has_value());
}

TEST_CASE("take_id - empty returns nullopt")
{
    std::string_view sv = "";
    CHECK_FALSE(paths::take_id(sv).has_value());
}

TEST_CASE("named path builders")
{
    CHECK(paths::blog_list()              == "/blog/list");
    CHECK(paths::blog_view("hello")       == "/blog/view/hello");
    CHECK(paths::blog_edit("hello")       == "/blog/edit/hello");
    CHECK(paths::blog_new()               == "/blog/new");
    CHECK(paths::link_list()              == "/link/list");
    CHECK(paths::link_edit(5)             == "/link/edit/5");
    CHECK(paths::link_new()               == "/link/new");
    CHECK(paths::account_list()           == "/admin/account/list");
    CHECK(paths::account_new()            == "/admin/account/new");
    CHECK(paths::account_edit("alice")    == "/admin/account/edit/alice");
    CHECK(paths::admin_org_list()         == "/admin/org/list");
    CHECK(paths::org_view(3)              == "/org/view/3");
    CHECK(paths::org_board(3)             == "/org/view/3/board");
    CHECK(paths::org_edit(3)              == "/org/edit/3");
    CHECK(paths::org_types(3)             == "/org/view/3/types");
    CHECK(paths::team_kanban(7)           == "/team/view/7/kanban");
    CHECK(paths::team_gantt(7)            == "/team/view/7/gantt");
    CHECK(paths::team_task_new(7)         == "/team/view/7/task/new");
    CHECK(paths::team_archive(7)          == "/team/view/7/archive");
    CHECK(paths::team_edit_members(7)     == "/team/edit/7/members");
    CHECK(paths::team_edit_settings(7)    == "/team/edit/7/settings");
    CHECK(paths::task_edit(99)            == "/task/edit/99");
}
```

- [ ] **Step 2: Add `test_paths` to `tests/CMakeLists.txt`**

Add after the `test_permissions` block (around line 20):

```cmake
# test_paths — header-only, no extra libs
add_executable(test_paths test_paths.cpp)
target_include_directories(test_paths PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_paths PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_paths)
```

- [ ] **Step 3: Run the test to verify it fails (paths.hpp does not exist yet)**

```bash
cd build && cmake --build . --target test_paths 2>&1 | head -20
```

Expected: compile error — `"paths.hpp" not found`.

- [ ] **Step 4: Create `src/paths.hpp`**

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

    // ── Segment helpers ───────────────────────────────────────────────────────

    // Consume the next slash-delimited segment from sv.
    // Strips a leading '/' first if present. Updates sv to the remainder
    // (no leading slash). Returns the consumed segment (empty if sv was empty).
    inline std::string_view take_segment(std::string_view& sv)
    {
        if(!sv.empty() && sv.front() == '/')
            sv.remove_prefix(1);
        const auto slash = sv.find('/');
        if(slash == sv.npos)
        {
            const auto seg = sv;
            sv             = {};
            return seg;
        }
        const auto seg = sv.substr(0, slash);
        sv             = sv.substr(slash + 1);
        return seg;
    }

    // Consume a numeric ID segment. Returns nullopt if absent or non-numeric.
    inline std::optional<long long> take_id(std::string_view& sv)
    {
        const auto seg = take_segment(sv);
        if(seg.empty())
            return std::nullopt;
        long long   id{};
        const auto* end = seg.data() + seg.size();
        if(std::from_chars(seg.data(), end, id).ptr != end)
            return std::nullopt;
        return id;
    }

    // ── Named path builders ───────────────────────────────────────────────────

    // Blog
    inline std::string blog_list()
    {
        return std::format("{}{}", blog_prefix, list_seg);
    }
    inline std::string blog_view(std::string_view slug)
    {
        return std::format("{}{}/{}", blog_prefix, view_seg, slug);
    }
    inline std::string blog_edit(std::string_view slug)
    {
        return std::format("{}{}/{}", blog_prefix, edit_seg, slug);
    }
    inline std::string blog_new()
    {
        return std::format("{}{}", blog_prefix, new_seg);
    }

    // Link
    inline std::string link_list()
    {
        return std::format("{}{}", link_prefix, list_seg);
    }
    inline std::string link_edit(long long id)
    {
        return std::format("{}{}/{}", link_prefix, edit_seg, id);
    }
    inline std::string link_new()
    {
        return std::format("{}{}", link_prefix, new_seg);
    }

    // Admin — account
    inline std::string account_list()
    {
        return std::format("{}account/{}", admin_prefix, list_seg);
    }
    inline std::string account_new()
    {
        return std::format("{}account/{}", admin_prefix, new_seg);
    }
    inline std::string account_edit(std::string_view username)
    {
        return std::format("{}account/{}/{}", admin_prefix, edit_seg, username);
    }

    // Admin — org
    inline std::string admin_org_list()
    {
        return std::format("{}org/{}", admin_prefix, list_seg);
    }

    // Org
    inline std::string org_view(long long id)
    {
        return std::format("{}{}/{}", org_prefix, view_seg, id);
    }
    inline std::string org_board(long long id)
    {
        return std::format("{}{}/{}/board", org_prefix, view_seg, id);
    }
    inline std::string org_edit(long long id)
    {
        return std::format("{}{}/{}", org_prefix, edit_seg, id);
    }
    inline std::string org_types(long long id)
    {
        return std::format("{}{}/{}/types", org_prefix, view_seg, id);
    }

    // Team
    inline std::string team_kanban(long long id)
    {
        return std::format("{}{}/{}/kanban", team_prefix, view_seg, id);
    }
    inline std::string team_gantt(long long id)
    {
        return std::format("{}{}/{}/gantt", team_prefix, view_seg, id);
    }
    inline std::string team_task_new(long long id)
    {
        return std::format("{}{}/{}/task/new", team_prefix, view_seg, id);
    }
    inline std::string team_archive(long long id)
    {
        return std::format("{}{}/{}/archive", team_prefix, view_seg, id);
    }
    inline std::string team_edit_members(long long id)
    {
        return std::format("{}{}/{}/members", team_prefix, edit_seg, id);
    }
    inline std::string team_edit_settings(long long id)
    {
        return std::format("{}{}/{}/settings", team_prefix, edit_seg, id);
    }

    // Task
    inline std::string task_edit(long long id)
    {
        return std::format("{}{}/{}", task_prefix, edit_seg, id);
    }
}
```

- [ ] **Step 5: Run the test to verify it passes**

```bash
cd build && cmake --build . --target test_paths && ctest -R test_paths --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 6: Create `src/widgets/not_found_widget.hpp`**

```cpp
#pragma once

#include <Wt/WContainerWidget.h>
#include <string>

class not_found_widget: public Wt::WContainerWidget
{
public:
    explicit not_found_widget(const std::string& msg = "Page not found.");
};
```

- [ ] **Step 7: Create `src/widgets/not_found_widget.cpp`**

```cpp
#include "not_found_widget.hpp"

#include <Wt/WText.h>

not_found_widget::not_found_widget(const std::string& msg)
{
    setStyleClass("error-page error-page--not-found");
    addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
}
```

- [ ] **Step 8: Create `src/widgets/forbidden_widget.hpp`**

```cpp
#pragma once

#include <Wt/WContainerWidget.h>

class forbidden_widget: public Wt::WContainerWidget
{
public:
    forbidden_widget();
};
```

- [ ] **Step 9: Create `src/widgets/forbidden_widget.cpp`**

```cpp
#include "forbidden_widget.hpp"

#include <Wt/WText.h>

forbidden_widget::forbidden_widget()
{
    setStyleClass("error-page error-page--forbidden");
    addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
}
```

- [ ] **Step 10: Wire the new widgets into `src/altinf_app.cpp`**

Add two includes near the top of `altinf_app.cpp` (after the existing widget includes):

```cpp
#include "widgets/not_found_widget.hpp"
#include "widgets/forbidden_widget.hpp"
```

Replace the `show_forbidden` body:

```cpp
void altinf_app::show_forbidden()
{
    m_content->addNew<forbidden_widget>();
}
```

Replace the `show_not_found` body:

```cpp
void altinf_app::show_not_found(const std::string& msg)
{
    m_content->addNew<not_found_widget>(msg);
}
```

- [ ] **Step 11: Build and run all tests**

```bash
cd build && cmake --build . && ctest --output-on-failure
```

Expected: all Catch2 tests pass. JS unit tests:

```bash
cd tests/js && npm test
```

Expected: pass.

- [ ] **Step 12: Commit**

```bash
git add src/paths.hpp \
        src/widgets/not_found_widget.hpp src/widgets/not_found_widget.cpp \
        src/widgets/forbidden_widget.hpp src/widgets/forbidden_widget.cpp \
        src/altinf_app.cpp \
        tests/test_paths.cpp tests/CMakeLists.txt
git commit -m "feat: add paths.hpp, not_found_widget, forbidden_widget"
```

---

## Task 2: Page renames

Rename 11 page pairs. For each: `git mv` the files, rename the class inside both files, then update all consumers.

**Files:**
- `git mv` (×11 pairs, listed below)
- Modify: `src/altinf_app.cpp` — 11 includes + ~17 `addNew<>` call sites

The consumer update to `altinf_app.cpp` is the only external change; all other consumers are internal to the renamed files themselves.

- [ ] **Step 1: Run the git mv commands**

```bash
cd /home/altainia/code/altinf

git mv src/blog/pages/blog_page.cpp         src/blog/pages/blog_list_page.cpp
git mv src/blog/pages/blog_page.hpp         src/blog/pages/blog_list_page.hpp

git mv src/blog/pages/blog_post_page.cpp    src/blog/pages/blog_view_page.cpp
git mv src/blog/pages/blog_post_page.hpp    src/blog/pages/blog_view_page.hpp

git mv src/blog/pages/post_editor_page.cpp  src/blog/pages/blog_edit_page.cpp
git mv src/blog/pages/post_editor_page.hpp  src/blog/pages/blog_edit_page.hpp

git mv src/link/pages/links_page.cpp        src/link/pages/link_list_page.cpp
git mv src/link/pages/links_page.hpp        src/link/pages/link_list_page.hpp

git mv src/link/pages/link_editor_page.cpp  src/link/pages/link_edit_page.cpp
git mv src/link/pages/link_editor_page.hpp  src/link/pages/link_edit_page.hpp

git mv src/admin/account/pages/account_manager_page.cpp  src/admin/account/pages/account_list_page.cpp
git mv src/admin/account/pages/account_manager_page.hpp  src/admin/account/pages/account_list_page.hpp

git mv src/admin/account/pages/account_editor_page.cpp   src/admin/account/pages/account_edit_page.cpp
git mv src/admin/account/pages/account_editor_page.hpp   src/admin/account/pages/account_edit_page.hpp

git mv src/org/pages/kanban_board_page.cpp        src/org/pages/team_kanban_page.cpp
git mv src/org/pages/kanban_board_page.hpp        src/org/pages/team_kanban_page.hpp

git mv src/org/pages/kanban_task_editor_page.cpp  src/org/pages/task_edit_page.cpp
git mv src/org/pages/kanban_task_editor_page.hpp  src/org/pages/task_edit_page.hpp

git mv src/org/pages/kanban_archive_page.cpp      src/org/pages/team_archive_page.cpp
git mv src/org/pages/kanban_archive_page.hpp      src/org/pages/team_archive_page.hpp

git mv src/org/pages/kanban_team_page.cpp         src/org/pages/team_edit_page.cpp
git mv src/org/pages/kanban_team_page.hpp         src/org/pages/team_edit_page.hpp
```

- [ ] **Step 2: Rename classes inside the blog files**

In `src/blog/pages/blog_list_page.hpp`: change `class blog_page` → `class blog_list_page`.

In `src/blog/pages/blog_list_page.cpp`: change `#include "blog_page.hpp"` → `#include "blog_list_page.hpp"`, and all occurrences of `blog_page::` → `blog_list_page::`.

In `src/blog/pages/blog_view_page.hpp`: change `class blog_post_page` → `class blog_view_page`.

In `src/blog/pages/blog_view_page.cpp`: change `#include "blog_post_page.hpp"` → `#include "blog_view_page.hpp"`, and all `blog_post_page::` → `blog_view_page::`.

In `src/blog/pages/blog_edit_page.hpp`: change `class post_editor_page` → `class blog_edit_page`.

In `src/blog/pages/blog_edit_page.cpp`: change `#include "post_editor_page.hpp"` → `#include "blog_edit_page.hpp"`, and all `post_editor_page::` → `blog_edit_page::`.

- [ ] **Step 3: Rename classes inside the link files**

In `src/link/pages/link_list_page.hpp`: change `class links_page` → `class link_list_page`.

In `src/link/pages/link_list_page.cpp`: change `#include "links_page.hpp"` → `#include "link_list_page.hpp"`, and all `links_page::` → `link_list_page::`.

In `src/link/pages/link_edit_page.hpp`: change `class link_editor_page` → `class link_edit_page`.

In `src/link/pages/link_edit_page.cpp`: change `#include "link_editor_page.hpp"` → `#include "link_edit_page.hpp"`, and all `link_editor_page::` → `link_edit_page::`.

- [ ] **Step 4: Rename classes inside the admin/account files**

In `src/admin/account/pages/account_list_page.hpp`: change `class account_manager_page` → `class account_list_page`.

In `src/admin/account/pages/account_list_page.cpp`: change `#include "account_manager_page.hpp"` → `#include "account_list_page.hpp"`, and all `account_manager_page::` → `account_list_page::`.

In `src/admin/account/pages/account_edit_page.hpp`: change `class account_editor_page` → `class account_edit_page`.

In `src/admin/account/pages/account_edit_page.cpp`: change `#include "account_editor_page.hpp"` → `#include "account_edit_page.hpp"`, and all `account_editor_page::` → `account_edit_page::`.

- [ ] **Step 5: Rename classes inside the org/pages files**

In `src/org/pages/team_kanban_page.hpp`: change `class kanban_board_page` → `class team_kanban_page`.

In `src/org/pages/team_kanban_page.cpp`: change `#include "kanban_board_page.hpp"` → `#include "team_kanban_page.hpp"`, and all `kanban_board_page::` → `team_kanban_page::`.

In `src/org/pages/task_edit_page.hpp`: change `class kanban_task_editor_page` → `class task_edit_page`.

In `src/org/pages/task_edit_page.cpp`: change `#include "kanban_task_editor_page.hpp"` → `#include "task_edit_page.hpp"`, and all `kanban_task_editor_page::` → `task_edit_page::`.

In `src/org/pages/team_archive_page.hpp`: change `class kanban_archive_page` → `class team_archive_page`.

In `src/org/pages/team_archive_page.cpp`: change `#include "kanban_archive_page.hpp"` → `#include "team_archive_page.hpp"`, and all `kanban_archive_page::` → `team_archive_page::`.

In `src/org/pages/team_edit_page.hpp`: change `class kanban_team_page` → `class team_edit_page`.

In `src/org/pages/team_edit_page.cpp`: change `#include "kanban_team_page.hpp"` → `#include "team_edit_page.hpp"`, and all `kanban_team_page::` → `team_edit_page::`.

- [ ] **Step 6: Update includes in `src/altinf_app.cpp`**

Replace the 11 old page include lines with the new paths. Find and replace each:

```cpp
// Old → New
#include "blog/pages/blog_page.hpp"          → #include "blog/pages/blog_list_page.hpp"
#include "blog/pages/blog_post_page.hpp"     → #include "blog/pages/blog_view_page.hpp"
#include "blog/pages/post_editor_page.hpp"   → #include "blog/pages/blog_edit_page.hpp"
#include "link/pages/links_page.hpp"         → #include "link/pages/link_list_page.hpp"
#include "link/pages/link_editor_page.hpp"   → #include "link/pages/link_edit_page.hpp"
#include "admin/account/pages/account_manager_page.hpp" → #include "admin/account/pages/account_list_page.hpp"
#include "admin/account/pages/account_editor_page.hpp"  → #include "admin/account/pages/account_edit_page.hpp"
#include "org/pages/kanban_board_page.hpp"       → #include "org/pages/team_kanban_page.hpp"
#include "org/pages/kanban_task_editor_page.hpp" → #include "org/pages/task_edit_page.hpp"
#include "org/pages/kanban_archive_page.hpp"     → #include "org/pages/team_archive_page.hpp"
#include "org/pages/kanban_team_page.hpp"        → #include "org/pages/team_edit_page.hpp"
```

- [ ] **Step 7: Update `addNew<>` class names in `src/altinf_app.cpp`**

Replace all old class names at instantiation sites (use the class name only, not the full expression):

```
blog_page           → blog_list_page
blog_post_page      → blog_view_page
post_editor_page    → blog_edit_page
links_page          → link_list_page
link_editor_page    → link_edit_page
account_manager_page → account_list_page
account_editor_page  → account_edit_page
kanban_board_page        → team_kanban_page
kanban_task_editor_page  → task_edit_page
kanban_archive_page      → team_archive_page
kanban_team_page         → team_edit_page
```

- [ ] **Step 8: Build**

```bash
cd build && cmake --build .
```

Expected: compiles cleanly. The routing logic is still old — only the class/file names changed.

- [ ] **Step 9: Run all tests**

```bash
cd build && ctest --output-on-failure
cd /home/altainia/code/altinf/tests/js && npm test
cd /home/altainia/code/altinf/e2e && npx playwright test
```

Expected: Catch2 and JS unit tests pass. E2E tests pass (routing unchanged so far).

- [ ] **Step 10: Commit**

```bash
git add -u
git commit -m "refactor: rename page files and classes to match domain structure"
```

---

## Task 3: `altinf_app` restructure

Rewrite `handle_path` as a lean dispatcher, add all domain handler methods, and update `altinf_app.hpp`. Remove the old `parse_id`/`suffix_after_id` static helpers (replaced by `paths::take_id`/`paths::take_segment`).

**Note:** From this task onward, E2E tests will fail because the URLs have changed. That is expected — they are fixed in Task 5. Catch2 and JS unit tests must continue to pass after every task.

**Files:**
- Modify: `src/altinf_app.hpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Update `src/altinf_app.hpp`**

Add `#include <string_view>` to the includes section (after `#include <vector>`).

Replace the private section's method declarations. The new private section is:

```cpp
private:
    session_data                     m_session;
    std::string                      m_session_token;
    std::unique_ptr<user_db>         m_user_db;
    std::filesystem::path            m_posts_dir;
    std::vector<blog_post>           m_posts;
    std::unique_ptr<link_db>         m_link_db;
    std::vector<link_entry>          m_links;
    std::optional<link_entry>        m_edit_link;
    std::unique_ptr<kanban_db>       m_kanban_db;
    std::unique_ptr<org_db>          m_org_db;
    std::optional<kanban_task_entry> m_edit_task;
    std::optional<user_entry>        m_edit_user;
    nav_bar*                         m_nav{nullptr};
    Wt::WContainerWidget*            m_content{nullptr};
    notifications_page*              m_notifications_page{nullptr};

    void handle_path(const std::string& path);

    // Domain handlers — each receives the path remainder after the prefix
    void handle_blog(std::string_view rem);
    void handle_link(std::string_view rem);
    void handle_org(std::string_view rem);
    void handle_team(std::string_view rem);
    void handle_task(std::string_view rem);
    void handle_admin(std::string_view rem);

    // Fixed-path handlers
    void handle_login();
    void handle_logout();
    void handle_notifications();
    void handle_settings();
    void show_main();

    void reload_posts();
    void reload_links();
    void show_forbidden();
    void show_not_found(const std::string& msg = "Page not found.");
    void set_wide(bool wide);
    void register_with_hub();

    bool            resolve_is_org_lead(long long org_id);
    team_cap::flags resolve_team_caps(long long team_id, long long org_id);
};
```

- [ ] **Step 2: Add `#include "paths.hpp"` to `src/altinf_app.cpp`**

Add it in the includes section, after the local page includes:

```cpp
#include "paths.hpp"
```

- [ ] **Step 3: Remove the old `parse_id` and `suffix_after_id` free functions from `src/altinf_app.cpp`**

Delete the two static helper functions at lines 40–63 (the `parse_id` and `suffix_after_id` functions). They are replaced by `paths::take_id` and `paths::take_segment`.

- [ ] **Step 4: Rewrite `altinf_app::handle_path`**

Replace the entire `handle_path` function body:

```cpp
void altinf_app::handle_path(const std::string& path)
{
    m_content->clear();
    m_notifications_page = nullptr;
    set_wide(false);

    const std::string_view sv{path};

    if(sv == "/" || sv.empty()) { show_main(); return; }
    if(sv == paths::login_path)         { handle_login(); return; }
    if(sv == paths::logout_path)        { handle_logout(); return; }
    if(sv == paths::notifications_path) { handle_notifications(); return; }
    if(sv == paths::settings_path)      { handle_settings(); return; }

    // Bare domain roots → canonical redirect
    if(sv == "/blog")          { setInternalPath(paths::blog_list(), true); return; }
    if(sv == "/link")          { setInternalPath(paths::link_list(), true); return; }
    if(sv == "/admin/account") { setInternalPath(paths::account_list(), true); return; }
    if(sv == "/admin/org")     { setInternalPath(paths::admin_org_list(), true); return; }

    if(sv.starts_with(paths::blog_prefix))
    { handle_blog(sv.substr(paths::blog_prefix.size())); return; }
    if(sv.starts_with(paths::link_prefix))
    { handle_link(sv.substr(paths::link_prefix.size())); return; }
    if(sv.starts_with(paths::org_prefix))
    { handle_org(sv.substr(paths::org_prefix.size())); return; }
    if(sv.starts_with(paths::team_prefix))
    { handle_team(sv.substr(paths::team_prefix.size())); return; }
    if(sv.starts_with(paths::task_prefix))
    { handle_task(sv.substr(paths::task_prefix.size())); return; }
    if(sv.starts_with(paths::admin_prefix))
    { handle_admin(sv.substr(paths::admin_prefix.size())); return; }

    show_not_found();
}
```

- [ ] **Step 5: Add `set_wide` and `show_main`**

Add after `show_not_found`:

```cpp
void altinf_app::set_wide(bool wide)
{
    m_content->setStyleClass(
      wide ? "site-content site-content--wide" : "site-content");
}

void altinf_app::show_main()
{
    m_content->addNew<main_page>();
}
```

- [ ] **Step 6: Add `handle_login` and `handle_logout`**

Extract the existing login and logout bodies from the old `handle_path` into new methods:

```cpp
void altinf_app::handle_login()
{
    m_content->addNew<login_page>(*m_user_db, m_session, [this] {
        try
        {
            const auto raw_token = m_user_db->create_session_token(m_session.username);
            m_session_token      = raw_token;
            Wt::Http::Cookie c{"altinf_session", raw_token};
            c.setHttpOnly(true);
            c.setSecure(true);
            c.setSameSite(Wt::Http::Cookie::SameSite::Strict);
            c.setMaxAge(std::chrono::seconds{30 * 24 * 3600});
            setCookie(c);
        }
        catch(const std::exception&)
        {
            m_session = session_data{};
            setInternalPath(std::string{paths::login_path}, true);
            return;
        }
        m_nav->update();
        register_with_hub();
        setInternalPath("/", true);
    });
}

void altinf_app::handle_logout()
{
    live_hub::instance().unsubscribe("user:" + m_session.username, sessionId());
    if(!m_session_token.empty())
    {
        m_user_db->delete_session_token(m_session_token);
        removeCookie(Wt::Http::Cookie{"altinf_session"});
        m_session_token.clear();
    }
    m_session = session_data{};
    m_nav->update();
    setInternalPath("/", true);
}
```

- [ ] **Step 7: Add `handle_notifications` and `handle_settings`**

```cpp
void altinf_app::handle_notifications()
{
    if(!m_session.logged_in)
    {
        setInternalPath(std::string{paths::login_path}, true);
        return;
    }
    m_notifications_page = m_content->addNew<notifications_page>(
      *m_org_db, m_session, [this] { m_nav->refresh_bell(); });
}

void altinf_app::handle_settings()
{
    if(!m_session.logged_in)
    {
        setInternalPath(std::string{paths::login_path}, true);
        return;
    }
    m_content->addNew<settings_page>(*m_org_db, m_session);
}
```

- [ ] **Step 8: Add `handle_blog`**

```cpp
void altinf_app::handle_blog(std::string_view rem)
{
    const auto seg = paths::take_segment(rem);
    if(seg == paths::list_seg)
    {
        m_content->addNew<blog_list_page>(m_posts);
    }
    else if(seg == paths::view_seg)
    {
        const std::string slug{rem};
        if(slug.empty()) { show_not_found(); return; }
        const auto it = std::find_if(
          m_posts.begin(), m_posts.end(),
          [&slug](const blog_post& p) { return p.slug == slug; });
        if(it == m_posts.end()) { show_not_found("Post not found."); return; }
        m_content->addNew<blog_view_page>(*it, m_session);
    }
    else if(seg == paths::edit_seg)
    {
        if(!m_session.permissions.has_any(permission::post_write))
        {
            show_forbidden();
            return;
        }
        const std::string slug{rem};
        const auto        it = std::find_if(
          m_posts.begin(), m_posts.end(),
          [&slug](const blog_post& p) { return p.slug == slug; });
        if(it == m_posts.end()) { show_not_found("Post not found."); return; }
        m_content->addNew<blog_edit_page>(
          m_posts_dir, &(*it), [this](const std::string& s) {
              reload_posts();
              setInternalPath(paths::blog_view(s), true);
          });
    }
    else if(seg == paths::new_seg)
    {
        if(!m_session.permissions.has_any(permission::post_write))
        {
            show_forbidden();
            return;
        }
        m_content->addNew<blog_edit_page>(
          m_posts_dir, nullptr, [this](const std::string& s) {
              reload_posts();
              setInternalPath(paths::blog_view(s), true);
          });
    }
    else
    {
        show_not_found();
    }
}
```

- [ ] **Step 9: Add `handle_link`**

```cpp
void altinf_app::handle_link(std::string_view rem)
{
    const auto seg = paths::take_segment(rem);
    if(seg == paths::list_seg)
    {
        m_content->addNew<link_list_page>(m_links, m_session, [this](long long id) {
            m_link_db->remove(id);
            reload_links();
            handle_path(paths::link_list());
        });
    }
    else if(seg == paths::new_seg)
    {
        if(!m_session.permissions.has_any(permission::post_write))
        {
            show_forbidden();
            return;
        }
        m_content->addNew<link_edit_page>(m_link_db.get(), nullptr, [this] {
            reload_links();
            handle_path(paths::link_list());
        });
    }
    else if(seg == paths::edit_seg)
    {
        if(!m_session.permissions.has_any(permission::post_write))
        {
            show_forbidden();
            return;
        }
        const auto id_opt = paths::take_id(rem);
        if(!id_opt) { show_not_found("Invalid link ID."); return; }
        const auto opt = m_link_db->find(*id_opt);
        if(!opt) { show_not_found("Link not found."); return; }
        m_edit_link = opt;
        m_content->addNew<link_edit_page>(m_link_db.get(), &(*m_edit_link), [this] {
            reload_links();
            handle_path(paths::link_list());
        });
    }
    else
    {
        show_not_found();
    }
}
```

- [ ] **Step 10: Add `handle_org`**

```cpp
void altinf_app::handle_org(std::string_view rem)
{
    if(!m_session.logged_in)
    {
        setInternalPath(std::string{paths::login_path}, true);
        return;
    }

    const auto seg = paths::take_segment(rem);
    if(seg == paths::view_seg)
    {
        const auto org_id_opt = paths::take_id(rem);
        if(!org_id_opt) { show_not_found(); return; }
        const long long org_id     = *org_id_opt;
        const bool      is_org_lead = resolve_is_org_lead(org_id);

        if(!is_org_lead && !m_org_db->is_org_member(org_id, m_session.username))
        {
            show_forbidden();
            return;
        }

        const auto sub = paths::take_segment(rem);
        if(sub.empty())
        {
            m_org_db->set_last_org(m_session.username, org_id);
            m_content->addNew<org_landing_page>(
              *m_org_db, *m_kanban_db, org_id, m_session, is_org_lead);
        }
        else if(sub == "board")
        {
            if(!is_org_lead) { show_forbidden(); return; }
            set_wide(true);
            m_org_db->set_last_org(m_session.username, org_id);
            m_content->addNew<org_board_page>(
              *m_org_db, *m_kanban_db, org_id, m_session);
        }
        else if(sub == "types")
        {
            if(!is_org_lead) { show_forbidden(); return; }
            m_content->addNew<org_type_manager_page>(
              *m_kanban_db, *m_org_db, org_id, m_session);
        }
        else
        {
            show_not_found();
        }
    }
    else if(seg == paths::edit_seg)
    {
        const auto org_id_opt = paths::take_id(rem);
        if(!org_id_opt) { show_not_found(); return; }
        const long long org_id     = *org_id_opt;
        const bool      is_org_lead = resolve_is_org_lead(org_id);
        if(!is_org_lead) { show_forbidden(); return; }
        m_content->addNew<team_edit_page>(
          *m_org_db, *m_kanban_db, *m_user_db, org_id, m_session,
          paths::org_view(org_id));
    }
    else
    {
        show_not_found();
    }
}
```

- [ ] **Step 11: Add `handle_team`**

```cpp
void altinf_app::handle_team(std::string_view rem)
{
    if(!m_session.logged_in)
    {
        setInternalPath(std::string{paths::login_path}, true);
        return;
    }

    const auto seg = paths::take_segment(rem);
    if(seg == paths::view_seg)
    {
        const auto team_id_opt = paths::take_id(rem);
        if(!team_id_opt) { show_not_found(); return; }
        const long long team_id = *team_id_opt;
        const auto      team    = m_kanban_db->find_team(team_id);
        if(!team) { show_not_found("Team not found."); return; }

        const auto caps     = resolve_team_caps(team_id, team->org_id);
        const auto settings = m_kanban_db->settings_for_team(team_id);
        const auto sub      = paths::take_segment(rem);

        if(sub.empty())
        {
            // Team landing reserved for future; redirect to kanban for now.
            setInternalPath(paths::team_kanban(team_id), true);
        }
        else if(sub == "kanban")
        {
            if(!caps.has_any(team_cap::view_board)) { show_forbidden(); return; }
            set_wide(true);
            m_content->addNew<team_kanban_page>(
              *m_kanban_db, *m_org_db, m_session, team_id, caps, settings, false);
        }
        else if(sub == "gantt")
        {
            if(!caps.has_any(team_cap::view_board)) { show_forbidden(); return; }
            set_wide(true);
            m_content->addNew<team_kanban_page>(
              *m_kanban_db, *m_org_db, m_session, team_id, caps, settings, true);
        }
        else if(sub == "task")
        {
            const auto task_sub = paths::take_segment(rem);
            if(task_sub == paths::new_seg)
            {
                if(!caps.has_any(team_cap::create_task)) { show_forbidden(); return; }
                m_content->addNew<task_edit_page>(
                  *m_kanban_db, *m_org_db, team_id, m_session, caps, settings,
                  nullptr,
                  [this, team_id] {
                      setInternalPath(paths::team_kanban(team_id), true);
                  });
            }
            else
            {
                show_not_found();
            }
        }
        else if(sub == "archive")
        {
            if(!caps.has_any(team_cap::view_archived)) { show_forbidden(); return; }
            m_content->addNew<team_archive_page>(*m_kanban_db, m_session, team_id);
        }
        else
        {
            show_not_found();
        }
    }
    else if(seg == paths::edit_seg)
    {
        const auto team_id_opt = paths::take_id(rem);
        if(!team_id_opt) { show_not_found(); return; }
        const long long team_id = *team_id_opt;
        const auto      team    = m_kanban_db->find_team(team_id);
        if(!team) { show_not_found("Team not found."); return; }

        const auto caps = resolve_team_caps(team_id, team->org_id);
        if(!caps.has_any(team_cap::manage_team)) { show_forbidden(); return; }

        const auto sub = paths::take_segment(rem);
        if(sub.empty())
        {
            setInternalPath(paths::team_edit_members(team_id), true);
        }
        else if(sub == "members")
        {
            m_content->addNew<team_edit_page>(
              *m_org_db, *m_kanban_db, *m_user_db, team->org_id, m_session,
              paths::team_kanban(team_id));
        }
        else if(sub == "settings")
        {
            m_content->addNew<team_settings_page>(*m_kanban_db, m_session, team_id);
        }
        else
        {
            show_not_found();
        }
    }
    else
    {
        show_not_found();
    }
}
```

- [ ] **Step 12: Add `handle_task`**

```cpp
void altinf_app::handle_task(std::string_view rem)
{
    if(!m_session.logged_in)
    {
        setInternalPath(std::string{paths::login_path}, true);
        return;
    }

    const auto seg = paths::take_segment(rem);
    if(seg == paths::edit_seg)
    {
        const auto task_id_opt = paths::take_id(rem);
        if(!task_id_opt) { show_not_found(); return; }
        const auto opt = m_kanban_db->find_task(*task_id_opt);
        if(!opt) { show_not_found("Task not found."); return; }

        const long long team_id = opt->team_id;
        const auto      team    = m_kanban_db->find_team(team_id);
        if(!team) { show_not_found("Team not found."); return; }

        const auto caps     = resolve_team_caps(team_id, team->org_id);
        const auto settings = m_kanban_db->settings_for_team(team_id);

        if(!caps.has_any(team_cap::view_board)) { show_forbidden(); return; }
        if(opt->is_archived && !caps.has_any(team_cap::view_archived))
        {
            show_not_found("Task not found.");
            return;
        }

        m_edit_task = opt;
        m_content->addNew<task_edit_page>(
          *m_kanban_db, *m_org_db, team_id, m_session, caps, settings,
          &(*m_edit_task),
          [this, team_id] {
              setInternalPath(paths::team_kanban(team_id), true);
          });
    }
    else
    {
        show_not_found();
    }
}
```

- [ ] **Step 13: Add `handle_admin`**

```cpp
void altinf_app::handle_admin(std::string_view rem)
{
    if(!m_session.logged_in)
    {
        setInternalPath(std::string{paths::login_path}, true);
        return;
    }

    const auto domain = paths::take_segment(rem);
    if(domain == "account")
    {
        if(!m_session.permissions.has_any(permission::admin) &&
           !m_session.permissions.has_any(permission::manage_users))
        {
            show_forbidden();
            return;
        }
        const auto seg = paths::take_segment(rem);
        if(seg == paths::list_seg)
        {
            m_content->addNew<account_list_page>(
              *m_user_db, m_session, [this](const std::string& username) {
                  if(username == m_session.username)
                      return;
                  const auto del_orgs     = m_org_db->orgs_for_user(username);
                  const auto del_team_ids = m_kanban_db->team_ids_for_user(username);
                  m_user_db->delete_user(username);
                  m_org_db->remove_user_from_all_orgs(username);
                  m_kanban_db->remove_member_from_all_teams(username);
                  live_hub::instance().broadcast("accounts");
                  for(const auto& org: del_orgs)
                      live_hub::instance().broadcast("org:" + std::to_string(org.id));
                  for(const auto tid: del_team_ids)
                      live_hub::instance().broadcast("team:" + std::to_string(tid));
                  handle_path(paths::account_list());
              });
        }
        else if(seg == paths::new_seg)
        {
            m_content->addNew<account_edit_page>(m_user_db.get(), nullptr, [this] {
                setInternalPath(paths::account_list(), true);
            });
        }
        else if(seg == paths::edit_seg)
        {
            const std::string edit_username{rem};
            const auto        users = m_user_db->list_users();
            const auto        it    = std::find_if(
              users.begin(), users.end(),
              [&edit_username](const user_entry& e) { return e.username == edit_username; });
            if(it == users.end()) { show_not_found("User not found."); return; }
            m_edit_user = *it;
            m_content->addNew<account_edit_page>(
              m_user_db.get(), &(*m_edit_user),
              [this] { setInternalPath(paths::account_list(), true); });
        }
        else
        {
            show_not_found();
        }
    }
    else if(domain == "org")
    {
        if(!m_session.permissions.has_any(permission::org_create) &&
           !m_session.permissions.has_any(permission::admin))
        {
            show_forbidden();
            return;
        }
        const auto seg = paths::take_segment(rem);
        if(seg == paths::list_seg)
        {
            m_content->addNew<org_admin_page>(*m_org_db, m_session);
        }
        else
        {
            show_not_found();
        }
    }
    else
    {
        show_not_found();
    }
}
```

- [ ] **Step 14: Build**

```bash
cd build && cmake --build .
```

Expected: compiles cleanly.

- [ ] **Step 15: Run Catch2 and JS unit tests**

```bash
cd build && ctest --output-on-failure
cd /home/altainia/code/altinf/tests/js && npm test
```

Expected: pass. (E2E tests will fail — that is expected and will be fixed in Task 5.)

- [ ] **Step 16: Commit**

```bash
git add src/altinf_app.hpp src/altinf_app.cpp
git commit -m "refactor: restructure handle_path into domain-method dispatcher"
```

---

## Task 4: Consumer path updates

Update every `Wt::WAnchor`, `Wt::WLink`, and `setInternalPath` call in the 20 consumer files to use `paths::` builders. All files must also `#include "paths.hpp"`.

**Files:** All the page/widget .cpp files listed in the File Structure section.

- [ ] **Step 1: Update `src/widgets/nav_bar.cpp`**

Add `#include "paths.hpp"` after the existing includes.

Replace path literals:

```cpp
// "/links"        → paths::link_list()
// "/blog"         → paths::blog_list()
// "/login"        → std::string{paths::login_path}
// "/logout"       → std::string{paths::logout_path}
// "/admin/new"    → paths::blog_new()
// "/admin/accounts" → paths::account_list()
// "/admin/org"    → paths::admin_org_list()
// "/settings"     → std::string{paths::settings_path}
// "/org/" + std::to_string(active_id)  → paths::org_view(active_id)
// "/org/" + std::to_string(o.id)       → paths::org_view(o.id)
```

- [ ] **Step 2: Update `src/org/pages/team_kanban_page.cpp`**

Add `#include "paths.hpp"`.

The `team_url` string variable is constructed at the top of the constructor. Replace it and all uses:

```cpp
// Remove: const std::string team_url = "/board/" + std::to_string(team_id);
// Replace each usage:
// Wt::WLink{..., team_url}              → paths::team_kanban(team_id)
// Wt::WLink{..., team_url + "/gantt"}   → paths::team_gantt(team_id)
// Wt::WLink{..., team_url + "/task/new"} → paths::team_task_new(team_id)
// Wt::WLink{..., team_url + "/manage"}  → paths::team_edit_members(team_id)
// Wt::WLink{..., team_url + "/settings"} → paths::team_edit_settings(team_id)
// Wt::WLink{..., team_url + "/archive"} → paths::team_archive(team_id)
```

- [ ] **Step 3: Update `src/org/pages/team_archive_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// Remove: const std::string board_url = "/board/" + std::to_string(team_id);
// Replace:
// Wt::WLink{..., board_url}          → paths::team_kanban(team_id)
// board_url + "/task/" + std::to_string(task.id) + "/edit"
//                                    → paths::task_edit(task.id)
```

- [ ] **Step 4: Update `src/org/pages/task_edit_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// Remove: const std::string board_url = "/board/" + std::to_string(team_id);
// Replace the setInternalPath lambda body:
//   Wt::WApplication::instance()->setInternalPath(board_url, true)
//   → Wt::WApplication::instance()->setInternalPath(paths::team_kanban(team_id), true)
```

- [ ] **Step 5: Update `src/org/pages/team_settings_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// Remove: const std::string board_url = "/board/" + std::to_string(team_id);
// Replace: Wt::WLink{..., board_url} → paths::team_kanban(team_id)
```

- [ ] **Step 6: Update `src/org/pages/team_edit_page.cpp`**

Add `#include "paths.hpp"`.

Update the default back-url fallback (line with `m_back_url = back_url.empty() ? ...`):

```cpp
// Old:
m_back_url = back_url.empty() ? "/org/" + std::to_string(org_id) : back_url;
// New:
m_back_url = back_url.empty() ? paths::org_view(org_id) : back_url;
```

Update the settings link for each team (around line 339):

```cpp
// Old: "/board/" + std::to_string(team.id) + "/settings"
// New: paths::team_edit_settings(team.id)
```

- [ ] **Step 7: Update `src/org/pages/org_landing_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/org/" + std::to_string(m_org_id) + "/manage" → paths::org_edit(m_org_id)
// "/org/" + std::to_string(m_org_id) + "/board"  → paths::org_board(m_org_id)
// "/org/" + std::to_string(m_org_id) + "/types"  → paths::org_types(m_org_id)
// "/board/" + std::to_string(t.id)               → paths::team_kanban(t.id)
```

- [ ] **Step 8: Update `src/org/pages/org_board_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/org/" + std::to_string(org_id)                → paths::org_view(org_id)
// "/board/" + std::to_string(team.id)             → paths::team_kanban(team.id)
// "/board/" + std::to_string(tid) + "/task/" + std::to_string(task_id) + "/edit"
//                                                  → paths::task_edit(task_id)
```

Note: the `tid` capture is no longer needed in the last lambda. Change to:

```cpp
[task_id](long long task_id) {
    Wt::WApplication::instance()->setInternalPath(paths::task_edit(task_id), true);
}
```

- [ ] **Step 9: Update `src/org/pages/org_admin_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/org/" + std::to_string(o.id) → paths::org_view(o.id)
```

- [ ] **Step 10: Update `src/org/pages/org_type_manager_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// back_url = "/org/" + std::to_string(org_id) + "/types" → paths::org_types(org_id)
// (the back link itself navigates to the org view, check the actual back_url value
//  in the file and replace accordingly with paths::org_view(org_id) or paths::org_types(org_id))
```

- [ ] **Step 11: Update `src/org/pages/notifications_page.cpp`**

Add `#include "paths.hpp"`.

Replace all 6 occurrences of the task edit link pattern:

```cpp
// Old (repeated 6 times with different variable names but same structure):
"/board/" + std::to_string(team_id) + "/task/" + std::to_string(task_id) + "/edit"
// New:
paths::task_edit(task_id)
```

Also replace the `team_added` notification's team link:

```cpp
// Old: "/board/" + std::to_string(team_id)
// New: paths::team_kanban(team_id)
```

- [ ] **Step 12: Update `src/org/widgets/task_popup_widget.cpp`**

Add `#include "paths.hpp"`.

```cpp
// Remove: const std::string edit_url = "/board/" + std::to_string(team_id) + ...
// Replace with: const std::string edit_url = paths::task_edit(task_id);
```

- [ ] **Step 13: Update `src/link/pages/link_list_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/admin/links/new"              → paths::link_new()
// "/admin/links/edit/" + std::to_string(link_id) → paths::link_edit(link_id)
```

- [ ] **Step 14: Update `src/link/pages/link_edit_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/links" → paths::link_list()
```

- [ ] **Step 15: Update `src/blog/pages/blog_list_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/blog/" + post.slug → paths::blog_view(post.slug)
```

- [ ] **Step 16: Update `src/blog/pages/blog_view_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/blog"                      → paths::blog_list()
// "/admin/edit/" + post.slug   → paths::blog_edit(post.slug)
```

- [ ] **Step 17: Update `src/blog/pages/blog_edit_page.cpp`**

Add `#include "paths.hpp"`.

Find the cancel link path and post-save redirect (there is a `cancel_path` variable and a post-save path):

```cpp
// cancel_path that navigates back to /blog → paths::blog_list()
// post-save that navigates to /blog/<slug> → paths::blog_view(slug)
```

- [ ] **Step 18: Update `src/admin/account/pages/account_list_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/admin/accounts/new"                  → paths::account_new()
// "/admin/accounts/edit/" + u.username   → paths::account_edit(u.username)
```

- [ ] **Step 19: Update `src/admin/account/pages/account_edit_page.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/admin/accounts" → paths::account_list()
```

- [ ] **Step 20: Update `src/widgets/notification_bell.cpp`**

Add `#include "paths.hpp"`.

```cpp
// "/notifications" → std::string{paths::notifications_path}
```

- [ ] **Step 21: Build**

```bash
cd build && cmake --build .
```

Expected: compiles cleanly.

- [ ] **Step 22: Run Catch2 and JS unit tests**

```bash
cd build && ctest --output-on-failure
cd /home/altainia/code/altinf/tests/js && npm test
```

Expected: pass.

- [ ] **Step 23: Commit**

```bash
git add -u
git commit -m "refactor: replace hardcoded path strings with paths:: builders"
```

---

## Task 5: E2E test updates

Update the hardcoded path strings in E2E specs. The dynamic URL captures (`teamUrl = page.url()`, `boardUrl = page.url()`, `taskUrl = page.url()`) update automatically — only literal path strings in `goto()` and `toHaveURL()` calls need changing.

**Files:**
- Modify: `e2e/specs/blog.spec.ts`
- Modify: `e2e/specs/home.spec.ts`
- Modify: `e2e/specs/links.spec.ts`
- Modify: `e2e/specs/board.spec.ts`

- [ ] **Step 1: Update `e2e/specs/blog.spec.ts`**

Replace all `page.goto('/blog')` with `page.goto('/blog/list')`:

```typescript
// 6 occurrences — change each:
await page.goto('/blog');
// to:
await page.goto('/blog/list');
```

- [ ] **Step 2: Update `e2e/specs/home.spec.ts`**

```typescript
// Line 23: await page.goto('/blog')  →  await page.goto('/blog/list')
// Line 31: await expect(page).toHaveURL('/blog')  →  await expect(page).toHaveURL('/blog/list')
```

- [ ] **Step 3: Update `e2e/specs/links.spec.ts`**

Replace all `page.goto('/links')` with `page.goto('/link/list')`:

```typescript
// 3 occurrences:
await page.goto('/links');
// to:
await page.goto('/link/list');
```

- [ ] **Step 4: Update `e2e/specs/board.spec.ts`**

```typescript
// Line 91: await page.goto('/board/1')  →  await page.goto('/team/view/1/kanban')
```

- [ ] **Step 5: Run the full E2E suite**

```bash
cd /home/altainia/code/altinf/e2e && npx playwright test
```

Expected: all tests pass (the 2 pre-existing failures in `comments.spec.ts` and `task-permissions.spec.ts` remain; those are not caused by this refactor).

- [ ] **Step 6: Run all three test suites to confirm no regressions**

```bash
cd build && ctest --output-on-failure
cd /home/altainia/code/altinf/tests/js && npm test
cd /home/altainia/code/altinf/e2e && npx playwright test
```

Expected: Catch2 and JS unit tests pass. E2E shows same pass/fail ratio as before this refactor.

- [ ] **Step 7: Commit**

```bash
git add e2e/specs/blog.spec.ts e2e/specs/home.spec.ts \
        e2e/specs/links.spec.ts e2e/specs/board.spec.ts
git commit -m "test(e2e): update path strings to new domain-prefixed URLs"
```
