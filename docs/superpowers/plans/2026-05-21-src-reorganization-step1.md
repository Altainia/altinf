# src/ Reorganization — Step 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redistribute source files into domain-aligned directories (`src/blog/`, `src/link/`, `src/auth/`, `src/admin/account/`, `src/org/`) each with `pages/` and `widgets/` subdirectories, dissolving `src/kanban/` into `src/org/`.

**Architecture:** Pure file move — no logic changes. CMakeLists uses `GLOB_RECURSE` so the main binary needs no build system changes; only `#include` paths in source files and explicit source paths in `tests/CMakeLists.txt` need updating. Build must compile cleanly after each domain step.

**Tech Stack:** C++/Wt, CMake, Catch2, Playwright (E2E), Node.js (JS unit tests)

---

## Build and test commands (reference)

```bash
# Build
cmake --build build --parallel $(nproc)

# Catch2 unit tests
cd build && ctest --output-on-failure && cd ..

# JS unit tests
cd tests/js && npm test && cd ../..

# E2E tests (run from repo root)
cd e2e && npx playwright test && cd ..
```

---

## Task 1: blog domain

**Files:**
- Create dir: `src/blog/pages/`
- Move: `src/pages/blog_page.cpp` → `src/blog/pages/blog_page.cpp`
- Move: `src/pages/blog_page.hpp` → `src/blog/pages/blog_page.hpp`
- Move: `src/pages/blog_post_page.cpp` → `src/blog/pages/blog_post_page.cpp`
- Move: `src/pages/blog_post_page.hpp` → `src/blog/pages/blog_post_page.hpp`
- Move: `src/pages/post_editor_page.cpp` → `src/blog/pages/post_editor_page.cpp`
- Move: `src/pages/post_editor_page.hpp` → `src/blog/pages/post_editor_page.hpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Move the blog page files**

```bash
mkdir -p src/blog/pages
git mv src/pages/blog_page.cpp      src/blog/pages/blog_page.cpp
git mv src/pages/blog_page.hpp      src/blog/pages/blog_page.hpp
git mv src/pages/blog_post_page.cpp src/blog/pages/blog_post_page.cpp
git mv src/pages/blog_post_page.hpp src/blog/pages/blog_post_page.hpp
git mv src/pages/post_editor_page.cpp src/blog/pages/post_editor_page.cpp
git mv src/pages/post_editor_page.hpp src/blog/pages/post_editor_page.hpp
```

- [ ] **Step 2: Fix includes in altinf_app.cpp**

In `src/altinf_app.cpp`, change three lines:

```cpp
// Before:
#include "pages/blog_page.hpp"
#include "pages/blog_post_page.hpp"
#include "pages/post_editor_page.hpp"

// After:
#include "blog/pages/blog_page.hpp"
#include "blog/pages/blog_post_page.hpp"
#include "blog/pages/post_editor_page.hpp"
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: zero errors, zero warnings about missing headers.

- [ ] **Step 4: Run Catch2 tests**

```bash
cd build && ctest --output-on-failure && cd ..
```

Expected: all tests pass (no blog-related test files are affected).

- [ ] **Step 5: Commit**

```bash
git add src/blog/pages/ src/altinf_app.cpp
git commit -m "refactor: move blog pages into src/blog/pages/"
```

---

## Task 2: auth/login_page

**Files:**
- Create dir: `src/auth/pages/`
- Move: `src/pages/login_page.cpp` → `src/auth/pages/login_page.cpp`
- Move: `src/pages/login_page.hpp` → `src/auth/pages/login_page.hpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Move login_page**

```bash
mkdir -p src/auth/pages
git mv src/pages/login_page.cpp src/auth/pages/login_page.cpp
git mv src/pages/login_page.hpp src/auth/pages/login_page.hpp
```

- [ ] **Step 2: Fix includes in altinf_app.cpp**

In `src/altinf_app.cpp`, change one line:

```cpp
// Before:
#include "pages/login_page.hpp"

// After:
#include "auth/pages/login_page.hpp"
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: zero errors.

- [ ] **Step 4: Run Catch2 tests**

```bash
cd build && ctest --output-on-failure && cd ..
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/auth/pages/ src/altinf_app.cpp
git commit -m "refactor: move login_page into src/auth/pages/"
```

---

## Task 3: link domain

**Files:**
- Create dir: `src/link/` and `src/link/pages/`
- Move: `src/links/link.hpp` → `src/link/link.hpp`
- Move: `src/links/link_db.cpp` → `src/link/link_db.cpp`
- Move: `src/links/link_db.hpp` → `src/link/link_db.hpp`
- Move: `src/pages/link_editor_page.cpp` → `src/link/pages/link_editor_page.cpp`
- Move: `src/pages/link_editor_page.hpp` → `src/link/pages/link_editor_page.hpp`
- Move: `src/pages/links_page.cpp` → `src/link/pages/links_page.cpp`
- Move: `src/pages/links_page.hpp` → `src/link/pages/links_page.hpp`
- Modify: `src/altinf_app.hpp`, `src/altinf_app.cpp`
- Modify: `src/link/pages/link_editor_page.hpp`, `src/link/pages/links_page.hpp`
- Modify: `tests/test_link_db.cpp`, `tests/CMakeLists.txt`
- Delete: `src/links/`

- [ ] **Step 1: Move all link files**

```bash
mkdir -p src/link/pages
git mv src/links/link.hpp          src/link/link.hpp
git mv src/links/link_db.cpp       src/link/link_db.cpp
git mv src/links/link_db.hpp       src/link/link_db.hpp
git mv src/pages/link_editor_page.cpp src/link/pages/link_editor_page.cpp
git mv src/pages/link_editor_page.hpp src/link/pages/link_editor_page.hpp
git mv src/pages/links_page.cpp    src/link/pages/links_page.cpp
git mv src/pages/links_page.hpp    src/link/pages/links_page.hpp
rmdir src/links
```

- [ ] **Step 2: Fix includes in altinf_app.hpp**

In `src/altinf_app.hpp`, change two lines:

```cpp
// Before:
#include "links/link.hpp"
#include "links/link_db.hpp"

// After:
#include "link/link.hpp"
#include "link/link_db.hpp"
```

- [ ] **Step 3: Fix includes in altinf_app.cpp**

In `src/altinf_app.cpp`, change two lines:

```cpp
// Before:
#include "pages/link_editor_page.hpp"
#include "pages/links_page.hpp"

// After:
#include "link/pages/link_editor_page.hpp"
#include "link/pages/links_page.hpp"
```

- [ ] **Step 4: Fix includes in the moved page headers**

In `src/link/pages/link_editor_page.hpp`, change two lines:

```cpp
// Before:
#include "links/link.hpp"
#include "links/link_db.hpp"

// After:
#include "link/link.hpp"
#include "link/link_db.hpp"
```

In `src/link/pages/links_page.hpp`, change one line:

```cpp
// Before:
#include "links/link.hpp"

// After:
#include "link/link.hpp"
```

- [ ] **Step 5: Fix test include in tests/test_link_db.cpp**

```cpp
// Before:
#include "links/link_db.hpp"

// After:
#include "link/link_db.hpp"
```

- [ ] **Step 6: Fix source path in tests/CMakeLists.txt**

In `tests/CMakeLists.txt`, change one line in the `test_link_db` target:

```cmake
# Before:
  ${CMAKE_SOURCE_DIR}/src/links/link_db.cpp)

# After:
  ${CMAKE_SOURCE_DIR}/src/link/link_db.cpp)
```

- [ ] **Step 7: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: zero errors. CMake will automatically re-process `tests/CMakeLists.txt` because it changed.

- [ ] **Step 8: Run Catch2 tests**

```bash
cd build && ctest --output-on-failure && cd ..
```

Expected: `test_link_db` still passes.

- [ ] **Step 9: Commit**

```bash
git add src/link/ src/altinf_app.hpp src/altinf_app.cpp \
        tests/test_link_db.cpp tests/CMakeLists.txt
git commit -m "refactor: rename src/links/ to src/link/, move link pages in"
```

---

## Task 4: admin/account domain

**Files:**
- Create dir: `src/admin/account/pages/`
- Move: `src/pages/account_editor_page.cpp` → `src/admin/account/pages/account_editor_page.cpp`
- Move: `src/pages/account_editor_page.hpp` → `src/admin/account/pages/account_editor_page.hpp`
- Move: `src/pages/account_manager_page.cpp` → `src/admin/account/pages/account_manager_page.cpp`
- Move: `src/pages/account_manager_page.hpp` → `src/admin/account/pages/account_manager_page.hpp`
- Modify: `src/altinf_app.cpp`

- [ ] **Step 1: Move account page files**

```bash
mkdir -p src/admin/account/pages
git mv src/pages/account_editor_page.cpp  src/admin/account/pages/account_editor_page.cpp
git mv src/pages/account_editor_page.hpp  src/admin/account/pages/account_editor_page.hpp
git mv src/pages/account_manager_page.cpp src/admin/account/pages/account_manager_page.cpp
git mv src/pages/account_manager_page.hpp src/admin/account/pages/account_manager_page.hpp
```

- [ ] **Step 2: Fix includes in altinf_app.cpp**

In `src/altinf_app.cpp`, change two lines:

```cpp
// Before:
#include "pages/account_editor_page.hpp"
#include "pages/account_manager_page.hpp"

// After:
#include "admin/account/pages/account_editor_page.hpp"
#include "admin/account/pages/account_manager_page.hpp"
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: zero errors.

- [ ] **Step 4: Run Catch2 tests**

```bash
cd build && ctest --output-on-failure && cd ..
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/admin/ src/altinf_app.cpp
git commit -m "refactor: move account pages into src/admin/account/pages/"
```

---

## Task 5: org domain (dissolve src/kanban/)

This is the largest task. `src/kanban/` is fully absorbed: non-widget files go to `src/org/`, widgets go to `src/org/widgets/`, all kanban and org pages go to `src/org/pages/`.

**Files:**
- Create dirs: `src/org/widgets/`, `src/org/pages/`
- Move from `src/kanban/` (non-widget):
  - `kanban.hpp` → `src/org/kanban.hpp`
  - `kanban_db.cpp/hpp` → `src/org/kanban_db.cpp/hpp`
  - `kanban_notifications.cpp/hpp` → `src/org/kanban_notifications.cpp/hpp`
  - `team_cap.hpp` → `src/org/team_cap.hpp`
- Move from `src/kanban/` (widgets):
  - `gantt_view_widget.cpp/hpp` → `src/org/widgets/`
  - `kanban_board_widget.cpp/hpp` → `src/org/widgets/`
  - `task_editor_form_widget.cpp/hpp` → `src/org/widgets/`
  - `task_popup_widget.cpp/hpp` → `src/org/widgets/`
- Move from `src/pages/` (11 pages):
  - `kanban_archive_page`, `kanban_board_page`, `kanban_task_editor_page`, `kanban_team_page`
  - `org_admin_page`, `org_board_page`, `org_landing_page`, `org_type_manager_page`
  - `notifications_page`, `settings_page`, `team_settings_page`
- Modify: `src/altinf_app.hpp`, `src/altinf_app.cpp`
- Modify: `src/org/kanban_notifications.hpp`, `src/org/kanban_db.hpp`
- Modify: `src/org/widgets/kanban_board_widget.hpp`, `src/org/widgets/gantt_view_widget.hpp`
- Modify: `src/org/widgets/task_editor_form_widget.cpp`
- Modify: `src/org/pages/kanban_archive_page.hpp`
- Modify: `src/org/pages/kanban_board_page.cpp`, `src/org/pages/kanban_board_page.hpp`
- Modify: `src/org/pages/kanban_task_editor_page.cpp`, `src/org/pages/kanban_task_editor_page.hpp`
- Modify: `src/org/pages/kanban_team_page.hpp`
- Modify: `src/org/pages/org_board_page.cpp`, `src/org/pages/org_board_page.hpp`
- Modify: `src/org/pages/org_landing_page.hpp`, `src/org/pages/org_type_manager_page.hpp`
- Modify: `src/org/pages/team_settings_page.hpp`
- Modify: `tests/test_kanban_db.cpp`, `tests/test_kanban_notifications.cpp`, `tests/test_team_cap.cpp`
- Modify: `tests/CMakeLists.txt`
- Delete: `src/kanban/`

- [ ] **Step 1: Move kanban non-widget files into src/org/**

```bash
mkdir -p src/org/widgets src/org/pages
git mv src/kanban/kanban.hpp              src/org/kanban.hpp
git mv src/kanban/kanban_db.cpp           src/org/kanban_db.cpp
git mv src/kanban/kanban_db.hpp           src/org/kanban_db.hpp
git mv src/kanban/kanban_notifications.cpp src/org/kanban_notifications.cpp
git mv src/kanban/kanban_notifications.hpp src/org/kanban_notifications.hpp
git mv src/kanban/team_cap.hpp            src/org/team_cap.hpp
```

- [ ] **Step 2: Move kanban widget files into src/org/widgets/**

```bash
git mv src/kanban/gantt_view_widget.cpp        src/org/widgets/gantt_view_widget.cpp
git mv src/kanban/gantt_view_widget.hpp        src/org/widgets/gantt_view_widget.hpp
git mv src/kanban/kanban_board_widget.cpp      src/org/widgets/kanban_board_widget.cpp
git mv src/kanban/kanban_board_widget.hpp      src/org/widgets/kanban_board_widget.hpp
git mv src/kanban/task_editor_form_widget.cpp  src/org/widgets/task_editor_form_widget.cpp
git mv src/kanban/task_editor_form_widget.hpp  src/org/widgets/task_editor_form_widget.hpp
git mv src/kanban/task_popup_widget.cpp        src/org/widgets/task_popup_widget.cpp
git mv src/kanban/task_popup_widget.hpp        src/org/widgets/task_popup_widget.hpp
rmdir src/kanban
```

- [ ] **Step 3: Move org and kanban pages into src/org/pages/**

```bash
git mv src/pages/kanban_archive_page.cpp    src/org/pages/kanban_archive_page.cpp
git mv src/pages/kanban_archive_page.hpp    src/org/pages/kanban_archive_page.hpp
git mv src/pages/kanban_board_page.cpp      src/org/pages/kanban_board_page.cpp
git mv src/pages/kanban_board_page.hpp      src/org/pages/kanban_board_page.hpp
git mv src/pages/kanban_task_editor_page.cpp src/org/pages/kanban_task_editor_page.cpp
git mv src/pages/kanban_task_editor_page.hpp src/org/pages/kanban_task_editor_page.hpp
git mv src/pages/kanban_team_page.cpp       src/org/pages/kanban_team_page.cpp
git mv src/pages/kanban_team_page.hpp       src/org/pages/kanban_team_page.hpp
git mv src/pages/notifications_page.cpp     src/org/pages/notifications_page.cpp
git mv src/pages/notifications_page.hpp     src/org/pages/notifications_page.hpp
git mv src/pages/org_admin_page.cpp         src/org/pages/org_admin_page.cpp
git mv src/pages/org_admin_page.hpp         src/org/pages/org_admin_page.hpp
git mv src/pages/org_board_page.cpp         src/org/pages/org_board_page.cpp
git mv src/pages/org_board_page.hpp         src/org/pages/org_board_page.hpp
git mv src/pages/org_landing_page.cpp       src/org/pages/org_landing_page.cpp
git mv src/pages/org_landing_page.hpp       src/org/pages/org_landing_page.hpp
git mv src/pages/org_type_manager_page.cpp  src/org/pages/org_type_manager_page.cpp
git mv src/pages/org_type_manager_page.hpp  src/org/pages/org_type_manager_page.hpp
git mv src/pages/settings_page.cpp          src/org/pages/settings_page.cpp
git mv src/pages/settings_page.hpp          src/org/pages/settings_page.hpp
git mv src/pages/team_settings_page.cpp     src/org/pages/team_settings_page.cpp
git mv src/pages/team_settings_page.hpp     src/org/pages/team_settings_page.hpp
```

- [ ] **Step 4: Fix includes in src/altinf_app.hpp**

In `src/altinf_app.hpp`, change three lines:

```cpp
// Before:
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/team_cap.hpp"

// After:
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/team_cap.hpp"
```

- [ ] **Step 5: Fix includes in src/altinf_app.cpp**

In `src/altinf_app.cpp`, change eleven lines (all the kanban and org page includes):

```cpp
// Before:
#include "pages/kanban_archive_page.hpp"
#include "pages/kanban_board_page.hpp"
#include "pages/kanban_task_editor_page.hpp"
#include "pages/kanban_team_page.hpp"
#include "pages/notifications_page.hpp"
#include "pages/org_admin_page.hpp"
#include "pages/org_board_page.hpp"
#include "pages/org_landing_page.hpp"
#include "pages/org_type_manager_page.hpp"
#include "pages/settings_page.hpp"
#include "pages/team_settings_page.hpp"

// After:
#include "org/pages/kanban_archive_page.hpp"
#include "org/pages/kanban_board_page.hpp"
#include "org/pages/kanban_task_editor_page.hpp"
#include "org/pages/kanban_team_page.hpp"
#include "org/pages/notifications_page.hpp"
#include "org/pages/org_admin_page.hpp"
#include "org/pages/org_board_page.hpp"
#include "org/pages/org_landing_page.hpp"
#include "org/pages/org_type_manager_page.hpp"
#include "org/pages/settings_page.hpp"
#include "org/pages/team_settings_page.hpp"
```

- [ ] **Step 6: Fix includes in the moved kanban model/DB files**

In `src/org/kanban_db.hpp`, change one line (was a directory-relative include, now use src-rooted path):

```cpp
// Before:
#include "kanban.hpp"

// After:
#include "org/kanban.hpp"
```

In `src/org/kanban_db.cpp`, change one line:

```cpp
// Before:
#include "kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `src/org/kanban_notifications.hpp`, change one line:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `src/org/widgets/task_editor_form_widget.cpp`, change one line:

```cpp
// Before:
#include "kanban/kanban_notifications.hpp"

// After:
#include "org/kanban_notifications.hpp"
```

- [ ] **Step 7: Fix includes in the moved widget headers**

In `src/org/widgets/kanban_board_widget.hpp`, change one line (was directory-relative, now broken without fix):

```cpp
// Before:
#include "kanban.hpp"

// After:
#include "org/kanban.hpp"
```

In `src/org/widgets/gantt_view_widget.hpp`, change one line:

```cpp
// Before:
#include "kanban.hpp"

// After:
#include "org/kanban.hpp"
```

- [ ] **Step 8: Fix includes in the moved page headers and source files**

In `src/org/pages/kanban_archive_page.hpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `src/org/pages/kanban_board_page.hpp`:

```cpp
// Before:
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/kanban_board_widget.hpp"
#include "kanban/gantt_view_widget.hpp"
#include "kanban/team_cap.hpp"

// After:
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/widgets/kanban_board_widget.hpp"
#include "org/widgets/gantt_view_widget.hpp"
#include "org/team_cap.hpp"
```

In `src/org/pages/kanban_board_page.cpp`:

```cpp
// Before:
#include "kanban/gantt_view_widget.hpp"
#include "kanban/task_popup_widget.hpp"

// After:
#include "org/widgets/gantt_view_widget.hpp"
#include "org/widgets/task_popup_widget.hpp"
```

In `src/org/pages/kanban_task_editor_page.hpp`:

```cpp
// Before:
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "kanban/team_cap.hpp"

// After:
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/team_cap.hpp"
```

In `src/org/pages/kanban_task_editor_page.cpp`:

```cpp
// Before:
#include "kanban/task_editor_form_widget.hpp"

// After:
#include "org/widgets/task_editor_form_widget.hpp"
```

In `src/org/pages/kanban_team_page.hpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `src/org/pages/org_board_page.hpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `src/org/pages/org_board_page.cpp`:

```cpp
// Before:
#include "kanban/kanban_board_widget.hpp"

// After:
#include "org/widgets/kanban_board_widget.hpp"
```

In `src/org/pages/org_landing_page.hpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `src/org/pages/org_type_manager_page.hpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `src/org/pages/team_settings_page.hpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

- [ ] **Step 9: Fix includes in Catch2 test files**

In `tests/test_kanban_db.cpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"

// After:
#include "org/kanban_db.hpp"
```

In `tests/test_kanban_notifications.cpp`:

```cpp
// Before:
#include "kanban/kanban_db.hpp"
#include "kanban/kanban_notifications.hpp"

// After:
#include "org/kanban_db.hpp"
#include "org/kanban_notifications.hpp"
```

In `tests/test_team_cap.cpp`:

```cpp
// Before:
#include "kanban/team_cap.hpp"

// After:
#include "org/team_cap.hpp"
```

- [ ] **Step 10: Fix source paths in tests/CMakeLists.txt**

In `tests/CMakeLists.txt`, change the explicit source paths in three test targets:

```cmake
# test_kanban_db — Before:
add_executable(test_kanban_db test_kanban_db.cpp
  ${CMAKE_SOURCE_DIR}/src/kanban/kanban_db.cpp)

# After:
add_executable(test_kanban_db test_kanban_db.cpp
  ${CMAKE_SOURCE_DIR}/src/org/kanban_db.cpp)
```

```cmake
# test_kanban_notifications — Before:
add_executable(test_kanban_notifications test_kanban_notifications.cpp
  ${CMAKE_SOURCE_DIR}/src/kanban/kanban_db.cpp
  ${CMAKE_SOURCE_DIR}/src/kanban/kanban_notifications.cpp
  ...

# After:
add_executable(test_kanban_notifications test_kanban_notifications.cpp
  ${CMAKE_SOURCE_DIR}/src/org/kanban_db.cpp
  ${CMAKE_SOURCE_DIR}/src/org/kanban_notifications.cpp
  ...
```

(Leave all other source paths in `test_kanban_notifications` — `src/org/org_db.cpp`, `src/widgets/live_hub.cpp` — unchanged.)

- [ ] **Step 11: Build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: zero errors. CMake re-processes `tests/CMakeLists.txt` automatically.

- [ ] **Step 12: Run Catch2 tests**

```bash
cd build && ctest --output-on-failure && cd ..
```

Expected: all tests pass, including `test_kanban_db`, `test_kanban_notifications`, `test_team_cap`.

- [ ] **Step 13: Run JS unit tests**

```bash
cd tests/js && npm test && cd ../..
```

Expected: all gantt tests pass.

- [ ] **Step 14: Run E2E tests**

```bash
cd e2e && npx playwright test && cd ..
```

Expected: all Playwright tests pass. This is the final full-suite check confirming nothing was broken by the structural changes.

- [ ] **Step 15: Commit**

```bash
git add src/org/ src/altinf_app.hpp src/altinf_app.cpp \
        tests/test_kanban_db.cpp tests/test_kanban_notifications.cpp \
        tests/test_team_cap.cpp tests/CMakeLists.txt
git commit -m "refactor: dissolve src/kanban/ into src/org/, move org pages in"
```

---

## Final state check

After all 5 tasks, verify:

```bash
# src/pages/ should contain only main_page
ls src/pages/

# src/kanban/ should not exist
ls src/kanban/ 2>&1

# src/links/ should not exist
ls src/links/ 2>&1
```

Expected output:
```
src/pages/: main_page.cpp  main_page.hpp
src/kanban/: No such file or directory
src/links/:  No such file or directory
```
