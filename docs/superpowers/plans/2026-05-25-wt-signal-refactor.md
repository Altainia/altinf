# Wt::Signal Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all `std::function` constructor-injected callbacks in widgets and pages with `Wt::Signal<>` public member fields.

**Architecture:** Each affected widget gains one or more public `Wt::Signal<...>` data members named in Wt style (past-tense, no `on_` prefix). Constructor parameters and stored `m_on_xxx` members are removed. Call sites connect to signals after construction instead of passing lambdas. Changes are grouped so each task produces a clean build before committing.

**Tech Stack:** C++/Wt 4.13.1, CMake, ctest (Catch2), Playwright E2E, Node.js JS unit tests.

---

## Background

`Wt::Signal<Args...>` is declared in `<Wt/WSignal.h>`. That header is transitively included by `<Wt/WContainerWidget.h>` via `WInteractWidget.h → WWebWidget.h → WJavaScript.h → WSignal.h`. No new `#include` is needed in any file that already includes `<Wt/WContainerWidget.h>`. The only include change is **removing `<functional>`** from each affected header.

Emitting a `Wt::Signal` with no connections is a no-op — this replaces every `if(m_on_xxx) m_on_xxx(args)` null-guard pattern with a plain `xxx.emit(args)`.

Build commands:
```bash
cmake --build build --parallel $(nproc)
```

Test commands (run all three suites):
```bash
cd build && ctest --output-on-failure && cd ..
cd tests/js && npm test && cd ../..
cd e2e && npx playwright test && cd ..
```

---

## Task 1: Migrate task_editor_form_widget + update task_edit_page and task_popup_widget

These three files are tightly coupled: `task_edit_page` and `task_popup_widget` both construct `task_editor_form_widget` with callbacks. Change all three together so the build stays clean.

**Files:**
- Modify: `src/org/widgets/task_editor_form_widget.hpp`
- Modify: `src/org/widgets/task_editor_form_widget.cpp`
- Modify: `src/org/pages/task_edit_page.hpp`
- Modify: `src/org/pages/task_edit_page.cpp`
- Modify: `src/org/widgets/task_popup_widget.cpp`

- [ ] **Step 1: Update task_editor_form_widget.hpp**

Replace the current header with the following. Key changes: remove `#include <functional>`, remove `on_saved`/`on_cancel` constructor params, remove `m_on_saved`/`m_on_cancel` members, add `Wt::Signal<> saved` and `Wt::Signal<> canceled` public members.

```cpp
#pragma once

#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WDateEdit.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "auth/session_data.hpp"
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/org_db.hpp"
#include "org/team_cap.hpp"

class task_editor_form_widget: public Wt::WContainerWidget
{
public:
	// task_id == 0 → new-task creation mode.
	task_editor_form_widget(kanban_db&          db,
	                        org_db&             odb,
	                        long long           task_id,
	                        long long           team_id,
	                        const session_data& session,
	                        team_cap::flags     caps,
	                        team_settings_entry settings);

	~task_editor_form_widget() override;

	bool is_dirty() const;
	bool is_stale() const;

	Wt::Signal<> saved;
	Wt::Signal<> canceled;

private:
	kanban_db&            m_db;
	org_db&               m_odb;
	long long             m_task_id{0};
	long long             m_team_id{0};
	long long             m_org_id{0};
	std::string           m_username;
	team_cap::flags       m_caps;
	team_settings_entry   m_settings;
	std::string           m_session_id;
	std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

	kanban_task_entry m_original;

	Wt::WLineEdit*        m_title_edit{nullptr};
	Wt::WText*            m_title_display{nullptr};
	Wt::WContainerWidget* m_title_field{nullptr};

	Wt::WTextArea*        m_desc_edit{nullptr};
	Wt::WText*            m_desc_display{nullptr};
	Wt::WContainerWidget* m_desc_field{nullptr};

	Wt::WComboBox*        m_status_edit{nullptr};
	Wt::WText*            m_status_display{nullptr};
	Wt::WContainerWidget* m_status_field{nullptr};

	Wt::WContainerWidget*    m_assignee_list{nullptr};
	Wt::WComboBox*           m_add_member_combo{nullptr};
	std::vector<std::string> m_pending_assignees;

	Wt::WDateEdit*        m_start_date_edit{nullptr};
	Wt::WText*            m_start_date_display{nullptr};
	Wt::WContainerWidget* m_start_field{nullptr};

	Wt::WDateEdit*        m_end_date_edit{nullptr};
	Wt::WText*            m_end_date_display{nullptr};
	Wt::WContainerWidget* m_end_field{nullptr};

	long long                          m_type_id{0};
	std::vector<Wt::WContainerWidget*> m_type_chips;
	std::vector<std::string>           m_status_vals_used;

	Wt::WContainerWidget* m_stale_banner{nullptr};
	bool                  m_stale{false};
	std::set<std::string> m_dirty_fields;

	Wt::WText*            m_status_msg{nullptr};
	Wt::WPushButton*      m_save_btn{nullptr};
	Wt::WContainerWidget* m_comment_list{nullptr};
	Wt::WContainerWidget* m_comment_compose{nullptr};
	Wt::WContainerWidget* m_history_panel{nullptr};

	static const std::vector<std::string> k_status_vals;
	static const std::vector<std::string> k_status_labels;

	void        save();
	void        mark_stale();
	void        mark_field_dirty(const std::string& field, Wt::WContainerWidget* container);
	void        unmark_field_dirty(const std::string& field, Wt::WContainerWidget* container);
	void        enter_edit_mode(Wt::WText* display, Wt::WWidget* edit);
	void        exit_edit_mode(Wt::WText* display, Wt::WWidget* edit, const std::string& new_text);
	void        rebuild_comments();
	void        rebuild_history();
	std::string render_markdown(const std::string& md) const;
};
```

- [ ] **Step 2: Update task_editor_form_widget.cpp constructor signature and initializer list**

Open `src/org/widgets/task_editor_form_widget.cpp`. Find the constructor definition. It currently begins:

```cpp
task_editor_form_widget::task_editor_form_widget(kanban_db&            db,
                                                 org_db&               odb,
                                                 long long             task_id,
                                                 long long             team_id,
                                                 const session_data&   session,
                                                 team_cap::flags       caps,
                                                 team_settings_entry   settings,
                                                 std::function<void()> on_saved,
                                                 std::function<void()> on_cancel):
  m_db{db},
  m_odb{odb},
  ...
  m_on_saved{std::move(on_saved)},
  m_on_cancel{std::move(on_cancel)}
```

Replace the signature and remove the two initializer lines so it reads:

```cpp
task_editor_form_widget::task_editor_form_widget(kanban_db&          db,
                                                 org_db&             odb,
                                                 long long           task_id,
                                                 long long           team_id,
                                                 const session_data& session,
                                                 team_cap::flags     caps,
                                                 team_settings_entry settings):
  m_db{db},
  m_odb{odb},
```

Leave all other initializer lines unchanged.

- [ ] **Step 3: Update emit sites in task_editor_form_widget.cpp**

Search the file for every call to `m_on_saved()` and `m_on_cancel()`. Replace each one:

```cpp
// Find and replace:
m_on_saved();
// With:
saved.emit();

// Find and replace:
m_on_cancel();
// With:
canceled.emit();
```

There should be exactly one occurrence of each in the `save()` method and the cancel button's click handler respectively.

- [ ] **Step 4: Update task_edit_page.hpp**

Replace the current header. Key changes: remove `#include <functional>`, remove `on_save` constructor param, add `Wt::Signal<> saved` public member.

```cpp
#pragma once

#include <Wt/WContainerWidget.h>

#include <string>
#include <vector>

#include "auth/session_data.hpp"
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/org_db.hpp"
#include "org/team_cap.hpp"

class task_edit_page: public Wt::WContainerWidget
{
public:
	task_edit_page(kanban_db&               db,
	               org_db&                  odb,
	               long long                team_id,
	               const session_data&      session,
	               team_cap::flags          caps,
	               team_settings_entry      settings,
	               const kanban_task_entry* existing);

	Wt::Signal<> saved;
};
```

- [ ] **Step 5: Update task_edit_page.cpp**

Replace the entire file:

```cpp
#include "task_edit_page.hpp"

#include <Wt/WApplication.h>
#include <Wt/WText.h>

#include "org/widgets/task_editor_form_widget.hpp"
#include "paths.hpp"

task_edit_page::task_edit_page(
  kanban_db&               db,
  org_db&                  odb,
  long long                team_id,
  const session_data&      session,
  team_cap::flags          caps,
  team_settings_entry      settings,
  const kanban_task_entry* existing)
{
	setStyleClass("page kb-editor-page");

	const bool is_new = (existing == nullptr);
	addNew<Wt::WText>(
	  is_new ? "<h1>New Task</h1>" : "<h1>Edit Task</h1>",
	  Wt::TextFormat::UnsafeXHTML);

	const long long task_id = existing ? existing->id : 0;

	auto* form = addNew<task_editor_form_widget>(
	  db, odb, task_id, team_id, session, caps, settings);

	form->saved.connect([this] { saved.emit(); });
	form->canceled.connect([team_id] {
		Wt::WApplication::instance()->setInternalPath(paths::team_kanban(team_id), true);
	});
}
```

- [ ] **Step 6: Update task_popup_widget.cpp**

Find the line that constructs `task_editor_form_widget` in the constructor. It currently reads:

```cpp
m_form = contents()->addNew<task_editor_form_widget>(
  db, odb, task_id, team_id, session, caps, settings, [this] { accept(); }, [this] { try_close(); });
```

Replace it with:

```cpp
m_form = contents()->addNew<task_editor_form_widget>(
  db, odb, task_id, team_id, session, caps, settings);
m_form->saved.connect([this] { accept(); });
m_form->canceled.connect([this] { try_close(); });
```

- [ ] **Step 7: Build and verify**

```bash
cmake --build build --parallel $(nproc)
```

Expected: clean build, zero errors or warnings related to these files.

- [ ] **Step 8: Commit**

```bash
git add src/org/widgets/task_editor_form_widget.hpp \
        src/org/widgets/task_editor_form_widget.cpp \
        src/org/pages/task_edit_page.hpp \
        src/org/pages/task_edit_page.cpp \
        src/org/widgets/task_popup_widget.cpp
git commit -m "refactor: replace task_editor_form_widget callbacks with Wt::Signal"
```

---

## Task 2: Migrate kanban_board_widget + gantt_view_widget + update team_kanban_page

Both JS→C++ bridge widgets and their sole caller (`team_kanban_page`) are changed together.

**Files:**
- Modify: `src/org/widgets/kanban_board_widget.hpp`
- Modify: `src/org/widgets/kanban_board_widget.cpp`
- Modify: `src/org/widgets/gantt_view_widget.hpp`
- Modify: `src/org/widgets/gantt_view_widget.cpp`
- Modify: `src/org/pages/team_kanban_page.cpp`

- [ ] **Step 1: Update kanban_board_widget.hpp**

Replace the current header. Key changes: remove `#include <functional>`, remove `on_move` and `on_edit` constructor params, add `moved` and `edit_requested` public signal members.

```cpp
#pragma once

#include <Wt/WContainerWidget.h>

#include <map>
#include <string>
#include <vector>

#include "org/kanban.hpp"

// Renders an interactive Kanban board via client-side JavaScript.
// Drag-and-drop column changes emit moved(task_id, new_status, new_sort_order).
// Edit-button clicks emit edit_requested(task_id).
class kanban_board_widget: public Wt::WContainerWidget
{
public:
	kanban_board_widget(std::vector<kanban_task_entry>          tasks,
	                    bool                                    can_move_columns,
	                    bool                                    can_move_done,
	                    const std::map<long long, std::string>& type_colors);

	void refresh(std::vector<kanban_task_entry>          tasks,
	             bool                                    can_move_columns,
	             bool                                    can_move_done,
	             const std::map<long long, std::string>& type_colors);

	Wt::Signal<long long, std::string, int> moved;
	Wt::Signal<long long>                   edit_requested;

private:
	std::string                      m_mount_id;
	std::string                      m_cb_id;
	Wt::WContainerWidget*            m_mount{nullptr};
	std::map<long long, std::string> m_type_colors;

	std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
	void        init_js(const std::string& json, bool can_move_columns, bool can_move_done);
};
```

- [ ] **Step 2: Update kanban_board_widget.cpp constructor**

Find the constructor definition. The current signature and the `cb->changed().connect(...)` block look like this:

```cpp
kanban_board_widget::kanban_board_widget(
  std::vector<kanban_task_entry>                          tasks,
  bool                                                    can_move_columns,
  bool                                                    can_move_done,
  const std::map<long long, std::string>&                 type_colors,
  std::function<void(long long, const std::string&, int)> on_move,
  std::function<void(long long)>                          on_edit):
  m_type_colors{type_colors}
{
    ...
    cb->changed().connect(
      [cb, on_move = std::move(on_move), on_edit = std::move(on_edit)]() mutable {
          const std::string payload = cb->text().toUTF8();
          if(payload.starts_with("MOVE:"))
          {
              const auto s1 = payload.find(':', 5);
              const auto s2 = s1 != std::string::npos ? payload.find(':', s1 + 1) : std::string::npos;
              if(s1 == std::string::npos || s2 == std::string::npos)
              {
                  return;
              }
              const long long   tid    = std::stoll(payload.substr(5, s1 - 5));
              const std::string status = payload.substr(s1 + 1, s2 - s1 - 1);
              const int         sort   = std::stoi(payload.substr(s2 + 1));
              on_move(tid, status, sort);
          }
          else if(payload.starts_with("EDIT:"))
          {
              const long long tid = std::stoll(payload.substr(5));
              on_edit(tid);
          }
      });
```

Replace with:

```cpp
kanban_board_widget::kanban_board_widget(
  std::vector<kanban_task_entry>          tasks,
  bool                                    can_move_columns,
  bool                                    can_move_done,
  const std::map<long long, std::string>& type_colors):
  m_type_colors{type_colors}
{
    ...
    cb->changed().connect([this, cb]() {
        const std::string payload = cb->text().toUTF8();
        if(payload.starts_with("MOVE:"))
        {
            const auto s1 = payload.find(':', 5);
            const auto s2 = s1 != std::string::npos ? payload.find(':', s1 + 1) : std::string::npos;
            if(s1 == std::string::npos || s2 == std::string::npos)
            {
                return;
            }
            const long long   tid    = std::stoll(payload.substr(5, s1 - 5));
            const std::string status = payload.substr(s1 + 1, s2 - s1 - 1);
            const int         sort   = std::stoi(payload.substr(s2 + 1));
            moved.emit(tid, status, sort);
        }
        else if(payload.starts_with("EDIT:"))
        {
            const long long tid = std::stoll(payload.substr(5));
            edit_requested.emit(tid);
        }
    });
```

Also remove the `#include <functional>` line at the top of `kanban_board_widget.cpp` if it exists.

- [ ] **Step 3: Update gantt_view_widget.hpp**

Replace the current header. Key changes: remove `#include <functional>`, remove `on_edit` constructor param and its default `= {}`, remove `m_on_edit` member, add `edit_requested` public signal member.

```cpp
#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "org/kanban.hpp"

// Read-only Gantt timeline rendered client-side as an SVG.
// Only tasks with valid start_date and end_date are shown.
class gantt_view_widget: public Wt::WContainerWidget
{
public:
	explicit gantt_view_widget(std::vector<kanban_task_entry>          tasks,
	                           const std::map<long long, std::string>& type_colors);

	void refresh(std::vector<kanban_task_entry>          tasks,
	             const std::map<long long, std::string>& type_colors);

	Wt::Signal<long long> edit_requested;

private:
	std::map<long long, std::string> m_type_colors;
	std::string                      m_mount_id;
	std::string                      m_cb_id;

	std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
};
```

- [ ] **Step 4: Update gantt_view_widget.cpp constructor**

Find the constructor definition. The current signature and `cb->changed().connect(...)` block read:

```cpp
gantt_view_widget::gantt_view_widget(std::vector<kanban_task_entry>          tasks,
                                     const std::map<long long, std::string>& type_colors,
                                     std::function<void(long long)>          on_edit):
  m_type_colors{type_colors}, m_on_edit{std::move(on_edit)}
{
    ...
    cb->changed().connect([this, cb] {
        const std::string p = cb->text().toUTF8();
        if(p.rfind("EDIT:", 0) == 0 && m_on_edit)
        {
            long long tid = 0;
            try
            {
                tid = std::stoll(p.substr(5));
            }
            catch(...)
            {
                cb->setText(Wt::WString{});
                return;
            }
            m_on_edit(tid);
        }
        cb->setText(Wt::WString{});
    });
```

Replace with:

```cpp
gantt_view_widget::gantt_view_widget(std::vector<kanban_task_entry>          tasks,
                                     const std::map<long long, std::string>& type_colors):
  m_type_colors{type_colors}
{
    ...
    cb->changed().connect([this, cb] {
        const std::string p = cb->text().toUTF8();
        if(p.rfind("EDIT:", 0) == 0)
        {
            long long tid = 0;
            try
            {
                tid = std::stoll(p.substr(5));
            }
            catch(...)
            {
                cb->setText(Wt::WString{});
                return;
            }
            edit_requested.emit(tid);
        }
        cb->setText(Wt::WString{});
    });
```

Also remove the `#include <functional>` line at the top of `gantt_view_widget.cpp` if it exists.

- [ ] **Step 5: Update team_kanban_page.cpp**

Find the Gantt widget construction block (inside `if(show_gantt)`). It currently reads:

```cpp
m_gantt_widget = addNew<gantt_view_widget>(
  tasks,
  m_type_colors,
  [this](long long tid) {
      new task_popup_widget(
        m_db, m_odb, tid, m_session, m_caps, m_settings, m_team_id);
  });
```

Replace with:

```cpp
m_gantt_widget = addNew<gantt_view_widget>(tasks, m_type_colors);
m_gantt_widget->edit_requested.connect([this](long long tid) {
    new task_popup_widget(
      m_db, m_odb, tid, m_session, m_caps, m_settings, m_team_id);
});
```

Find the Kanban board construction block (inside `else`). It currently reads:

```cpp
m_board_widget = addNew<kanban_board_widget>(
  tasks,
  can_move_columns,
  can_move_done,
  m_type_colors,
  [this, can_move_columns, can_move_done](long long tid, const std::string& status, int sort) {
      const auto task = m_db.find_task(tid);
      if(!task)
      {
          return;
      }
      const bool from_done = (task->status == "done");
      const bool to_done   = (status == "done");
      if((from_done || to_done) && !can_move_done)
      {
          return;
      }
      if(!from_done && !to_done && !can_move_columns)
      {
          return;
      }
      m_db.update_task_status(tid, status, sort, m_username);
      live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
  },
  [this](long long tid) {
      new task_popup_widget(
        m_db, m_odb, tid, m_session, m_caps, m_settings, m_team_id);
  });
```

Replace with:

```cpp
m_board_widget = addNew<kanban_board_widget>(
  tasks, can_move_columns, can_move_done, m_type_colors);
m_board_widget->moved.connect(
  [this, can_move_columns, can_move_done](long long tid, const std::string& status, int sort) {
      const auto task = m_db.find_task(tid);
      if(!task)
      {
          return;
      }
      const bool from_done = (task->status == "done");
      const bool to_done   = (status == "done");
      if((from_done || to_done) && !can_move_done)
      {
          return;
      }
      if(!from_done && !to_done && !can_move_columns)
      {
          return;
      }
      m_db.update_task_status(tid, status, sort, m_username);
      live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
  });
m_board_widget->edit_requested.connect([this](long long tid) {
    new task_popup_widget(
      m_db, m_odb, tid, m_session, m_caps, m_settings, m_team_id);
});
```

- [ ] **Step 6: Build and verify**

```bash
cmake --build build --parallel $(nproc)
```

Expected: clean build, zero errors.

- [ ] **Step 7: Commit**

```bash
git add src/org/widgets/kanban_board_widget.hpp \
        src/org/widgets/kanban_board_widget.cpp \
        src/org/widgets/gantt_view_widget.hpp \
        src/org/widgets/gantt_view_widget.cpp \
        src/org/pages/team_kanban_page.cpp
git commit -m "refactor: replace kanban/gantt widget callbacks with Wt::Signal"
```

---

## Task 3: Migrate remaining seven pages + update altinf_app.cpp

All seven pages are leaf widgets — their only call site is `altinf_app.cpp`. Change all headers, implementations, and the call site in one step.

**Files:**
- Modify: `src/auth/pages/login_page.hpp` + `.cpp`
- Modify: `src/blog/pages/blog_edit_page.hpp` + `.cpp`
- Modify: `src/link/pages/link_edit_page.hpp` + `.cpp`
- Modify: `src/link/pages/link_list_page.hpp` + `.cpp`
- Modify: `src/admin/account/pages/account_edit_page.hpp` + `.cpp`
- Modify: `src/admin/account/pages/account_list_page.hpp` + `.cpp`
- Modify: `src/org/pages/notifications_page.hpp` + `.cpp`
- Modify: `src/altinf_app.cpp`

### 3a — login_page

- [ ] **Step 1: Update login_page.hpp**

Remove `#include <functional>`, remove `on_login` constructor param, remove `m_on_login` member, add `Wt::Signal<> logged_in`:

```cpp
#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WText.h>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

class login_page: public Wt::WContainerWidget
{
public:
	login_page(user_db& db, session_data& session);

	Wt::Signal<> logged_in;

private:
	user_db&           m_db;
	session_data&      m_session;
	Wt::WLineEdit*     m_username{nullptr};
	Wt::WPasswordEdit* m_password{nullptr};
	Wt::WText*         m_error{nullptr};

	void submit();
};
```

- [ ] **Step 2: Update login_page.cpp**

Update the constructor signature and initializer list (remove `on_login` param and `m_on_login` initializer). Replace the `m_on_login()` call in `submit()` with `logged_in.emit()`:

```cpp
// Constructor signature change:
login_page::login_page(user_db& db, session_data& session):
  m_db{db},
  m_session{session}

// In submit(), replace:
m_on_login();
// With:
logged_in.emit();
```

### 3b — blog_edit_page

- [ ] **Step 3: Update blog_edit_page.hpp**

Remove `#include <functional>`, remove `on_save` constructor param, remove `m_on_save` member, add `Wt::Signal<std::string> saved`:

```cpp
#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <filesystem>
#include <string>

#include "blog/blog_post.hpp"

class blog_edit_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new post
	// existing != nullptr  ->  edit post (slug fixed to avoid breaking URLs)
	blog_edit_page(const std::filesystem::path& posts_dir,
	               const blog_post*             existing);

	Wt::Signal<std::string> saved;

private:
	std::filesystem::path m_posts_dir;
	const blog_post*      m_existing{nullptr};
	Wt::WStackedWidget*   m_stack{nullptr};
	Wt::WLineEdit*        m_title{nullptr};
	Wt::WLineEdit*        m_tags{nullptr};
	Wt::WTextArea*        m_body{nullptr};
	Wt::WContainerWidget* m_preview{nullptr};
	Wt::WText*            m_status{nullptr};

	void               save();
	static std::string read_body(const blog_post& post);
};
```

- [ ] **Step 4: Update blog_edit_page.cpp**

Update the constructor signature and initializer list (remove `on_save` param and `m_on_save` initializer). Find the `m_on_save(slug)` call inside `save()` and replace with `saved.emit(slug)`.

```cpp
// Constructor signature:
blog_edit_page::blog_edit_page(const std::filesystem::path& posts_dir,
                                const blog_post*             existing):
  m_posts_dir{posts_dir},
  m_existing{existing}

// In save(), find and replace:
m_on_save(slug);
// With:
saved.emit(slug);
```

### 3c — link_edit_page

- [ ] **Step 5: Update link_edit_page.hpp**

Remove `#include <functional>`, remove `on_save` constructor param, remove `m_on_save` member, add `Wt::Signal<> saved`:

```cpp
#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <optional>
#include <string>

#include "link/link.hpp"
#include "link/link_db.hpp"

class link_edit_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new link
	// existing != nullptr  ->  edit link
	link_edit_page(link_db* db, const link_entry* existing);

	Wt::Signal<> saved;

private:
	link_db*                  m_db;
	std::optional<link_entry> m_existing;
	Wt::WLineEdit*            m_url{nullptr};
	Wt::WLineEdit*            m_title{nullptr};
	Wt::WTextArea*            m_description{nullptr};
	Wt::WLineEdit*            m_section{nullptr};
	Wt::WLineEdit*            m_sort_order{nullptr};
	Wt::WText*                m_status{nullptr};

	void save();
};
```

- [ ] **Step 6: Update link_edit_page.cpp**

Update constructor signature and initializer list. Replace `m_on_save()` with `saved.emit()` in `save()`.

Update the constructor signature (remove `on_save` param), remove `m_on_save` from the initializer list, and replace `m_on_save()` in `save()` with `saved.emit()`. The `m_existing` initializer is unchanged — leave it exactly as it is in the current file.

```cpp
// In save(), replace:
m_on_save();
// With:
saved.emit();
```

### 3d — link_list_page

- [ ] **Step 7: Update link_list_page.hpp**

Remove `#include <functional>`, remove `on_delete` constructor param, remove `m_on_delete` member, add `Wt::Signal<long long> deleted`:

```cpp
#pragma once

#include <Wt/WContainerWidget.h>

#include <string>
#include <vector>

#include "auth/session_data.hpp"
#include "link/link.hpp"

class link_list_page: public Wt::WContainerWidget
{
public:
	link_list_page(const std::vector<link_entry>& links, const session_data& session);

	Wt::Signal<long long> deleted;

private:
	std::vector<link_entry> m_links;
	session_data            m_session;

	void render();
};
```

- [ ] **Step 8: Update link_list_page.cpp**

Update constructor signature and initializer list. Replace `m_on_delete(link_id)` with `deleted.emit(link_id)` in the Yes-button click handler inside `render()`.

```cpp
// Constructor signature:
link_list_page::link_list_page(const std::vector<link_entry>& links,
                                const session_data&            session):
  m_links{links},
  m_session{session}

// In render(), find and replace:
yes_btn->clicked().connect([this, link_id] {
    m_on_delete(link_id);
});
// With:
yes_btn->clicked().connect([this, link_id] {
    deleted.emit(link_id);
});
```

### 3e — account_edit_page

- [ ] **Step 9: Update account_edit_page.hpp**

Remove `#include <functional>`, remove `on_save` constructor param, remove `m_on_save` member, add `Wt::Signal<> saved`:

```cpp
#pragma once

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WText.h>

#include <optional>
#include <string>

#include "auth/user_db.hpp"

class account_edit_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new user
	// existing != nullptr  ->  edit user
	account_edit_page(user_db* db, const user_entry* existing);

	Wt::Signal<> saved;

private:
	user_db*                  m_db;
	std::optional<user_entry> m_existing;
	Wt::WLineEdit*            m_username{nullptr};
	Wt::WLineEdit*            m_display_name{nullptr};
	Wt::WPasswordEdit*        m_password{nullptr};
	Wt::WPasswordEdit*        m_password_confirm{nullptr};
	Wt::WCheckBox*            m_perm_admin{nullptr};
	Wt::WCheckBox*            m_perm_manage_users{nullptr};
	Wt::WCheckBox*            m_perm_post_write{nullptr};
	Wt::WCheckBox*            m_perm_org_create{nullptr};
	Wt::WText*                m_status{nullptr};
	Wt::WContainerWidget*     m_tokens_container{nullptr};

	void save();
	void build_token_list();
	void generate_token();
};
```

- [ ] **Step 10: Update account_edit_page.cpp**

Update constructor signature and initializer list. Replace `m_on_save()` with `saved.emit()` in `save()`.

```cpp
// Constructor signature:
account_edit_page::account_edit_page(user_db* db, const user_entry* existing):
  m_db{db},
  m_existing{existing ? std::make_optional(*existing) : std::nullopt}

// In save(), replace:
m_on_save();
// With:
saved.emit();
```

### 3f — account_list_page

- [ ] **Step 11: Update account_list_page.hpp**

Remove `#include <functional>`, remove `on_delete` constructor param, remove `m_on_delete` member, add `Wt::Signal<std::string> deleted`:

```cpp
#pragma once

#include <Wt/WContainerWidget.h>

#include <memory>
#include <string>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

class account_list_page: public Wt::WContainerWidget
{
public:
	account_list_page(user_db& db, const session_data& session);

	~account_list_page() override;

	Wt::Signal<std::string> deleted;

private:
	user_db&              m_db;
	const session_data&   m_session;
	std::string           m_session_id;
	std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

	void render();
	void refresh();
};
```

- [ ] **Step 12: Update account_list_page.cpp**

Update constructor signature and initializer list. Replace all `m_on_delete(username)` calls with `deleted.emit(username)`.

```cpp
// Constructor signature:
account_list_page::account_list_page(user_db& db, const session_data& session):
  m_db{db},
  m_session{session}

// Replace every occurrence of:
m_on_delete(username);
// With:
deleted.emit(username);
```

### 3g — notifications_page

- [ ] **Step 13: Update notifications_page.hpp**

Remove `#include <functional>`, remove `on_read` constructor param and its default, remove `m_on_read` member, add `Wt::Signal<> read`:

```cpp
#pragma once

#include <Wt/WContainerWidget.h>

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

class notifications_page: public Wt::WContainerWidget
{
public:
	notifications_page(org_db& odb, const session_data& session);

	Wt::Signal<> read;

private:
	org_db&               m_db;
	const session_data&   m_session;
	Wt::WContainerWidget* m_list{nullptr};

	void add_dismiss(Wt::WContainerWidget* parent, long long nid);

public:
	void refresh();
};
```

- [ ] **Step 14: Update notifications_page.cpp**

Update constructor signature and initializer list. Replace every `if(m_on_read) { m_on_read(); }` block (there are multiple — one per button handler) with a plain `read.emit()`.

```cpp
// Constructor signature:
notifications_page::notifications_page(org_db& odb, const session_data& session):
  m_db{odb},
  m_session{session}

// Find and replace every occurrence of:
if(m_on_read)
{
    m_on_read();
}
// With:
read.emit();
```

There are exactly four such blocks: one in `add_dismiss`, and one each in the `org_invite` Acknowledge, Accept, and Decline click handlers. Replace all four.

### 3h — Update altinf_app.cpp call sites

- [ ] **Step 15: Update handle_login() in altinf_app.cpp**

Find:
```cpp
m_content->addNew<login_page>(*m_user_db, m_session, [this] {
    try
    {
        const auto raw_token = m_user_db->create_session_token(m_session.username);
        m_session_token      = raw_token;
        Wt::Http::Cookie c{"altinf_session", raw_token};
        c.setHttpOnly(true);
        c.setSecure(true);
        c.setSameSite(Wt::Http::Cookie::SameSite::Strict);
        c.setMaxAge(std::chrono::days{30});
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
```

Replace with:
```cpp
auto* login = m_content->addNew<login_page>(*m_user_db, m_session);
login->logged_in.connect([this] {
    try
    {
        const auto raw_token = m_user_db->create_session_token(m_session.username);
        m_session_token      = raw_token;
        Wt::Http::Cookie c{"altinf_session", raw_token};
        c.setHttpOnly(true);
        c.setSecure(true);
        c.setSameSite(Wt::Http::Cookie::SameSite::Strict);
        c.setMaxAge(std::chrono::days{30});
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
```

- [ ] **Step 16: Update handle_notifications() in altinf_app.cpp**

Find:
```cpp
m_notifications_page = m_content->addNew<notifications_page>(
  *m_org_db, m_session, [this] { m_nav->refresh_bell(); });
```

Replace with:
```cpp
m_notifications_page = m_content->addNew<notifications_page>(*m_org_db, m_session);
m_notifications_page->read.connect([this] { m_nav->refresh_bell(); });
```

- [ ] **Step 17: Update handle_blog() — edit case — in altinf_app.cpp**

Find the edit-existing-post call:
```cpp
m_content->addNew<blog_edit_page>(
  m_posts_dir, &(*it), [this](const std::string& s) {
      reload_posts();
      setInternalPath(paths::blog_view(s), true);
  });
```

Replace with:
```cpp
auto* p = m_content->addNew<blog_edit_page>(m_posts_dir, &(*it));
p->saved.connect([this](const std::string& s) {
    reload_posts();
    setInternalPath(paths::blog_view(s), true);
});
```

- [ ] **Step 18: Update handle_blog() — new-post case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<blog_edit_page>(
  m_posts_dir, nullptr, [this](const std::string& s) {
      reload_posts();
      setInternalPath(paths::blog_view(s), true);
  });
```

Replace with:
```cpp
auto* p = m_content->addNew<blog_edit_page>(m_posts_dir, nullptr);
p->saved.connect([this](const std::string& s) {
    reload_posts();
    setInternalPath(paths::blog_view(s), true);
});
```

- [ ] **Step 19: Update handle_link() — list case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<link_list_page>(m_links, m_session, [this](long long id) {
    m_link_db->remove(id);
    reload_links();
    handle_path(paths::link_list());
});
```

Replace with:
```cpp
auto* p = m_content->addNew<link_list_page>(m_links, m_session);
p->deleted.connect([this](long long id) {
    m_link_db->remove(id);
    reload_links();
    handle_path(paths::link_list());
});
```

- [ ] **Step 20: Update handle_link() — new-link case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<link_edit_page>(m_link_db.get(), nullptr, [this] {
    reload_links();
    handle_path(paths::link_list());
});
```

Replace with:
```cpp
auto* p = m_content->addNew<link_edit_page>(m_link_db.get(), nullptr);
p->saved.connect([this] {
    reload_links();
    handle_path(paths::link_list());
});
```

- [ ] **Step 21: Update handle_link() — edit-link case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<link_edit_page>(m_link_db.get(), &(*m_edit_link), [this] {
    reload_links();
    handle_path(paths::link_list());
});
```

Replace with:
```cpp
auto* p = m_content->addNew<link_edit_page>(m_link_db.get(), &(*m_edit_link));
p->saved.connect([this] {
    reload_links();
    handle_path(paths::link_list());
});
```

- [ ] **Step 22: Update handle_team() — new-task case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<task_edit_page>(
  *m_kanban_db, *m_org_db, team_id, m_session, caps, settings, nullptr, [this, team_id] {
      setInternalPath(paths::team_kanban(team_id), true);
  });
```

Replace with:
```cpp
auto* p = m_content->addNew<task_edit_page>(
  *m_kanban_db, *m_org_db, team_id, m_session, caps, settings, nullptr);
p->saved.connect([this, team_id] {
    setInternalPath(paths::team_kanban(team_id), true);
});
```

- [ ] **Step 23: Update handle_task() — edit-task case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<task_edit_page>(
  *m_kanban_db, *m_org_db, team_id, m_session, caps, settings, &(*m_edit_task), [this, team_id] {
      setInternalPath(paths::team_kanban(team_id), true);
  });
```

Replace with:
```cpp
auto* p = m_content->addNew<task_edit_page>(
  *m_kanban_db, *m_org_db, team_id, m_session, caps, settings, &(*m_edit_task));
p->saved.connect([this, team_id] {
    setInternalPath(paths::team_kanban(team_id), true);
});
```

- [ ] **Step 24: Update handle_admin() — new-user case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<account_edit_page>(m_user_db.get(), nullptr, [this] {
    setInternalPath(paths::account_list(), true);
});
```

Replace with:
```cpp
auto* p = m_content->addNew<account_edit_page>(m_user_db.get(), nullptr);
p->saved.connect([this] { setInternalPath(paths::account_list(), true); });
```

- [ ] **Step 25: Update handle_admin() — edit-user case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<account_edit_page>(
  m_user_db.get(), &(*m_edit_user), [this] { setInternalPath(paths::account_list(), true); });
```

Replace with:
```cpp
auto* p = m_content->addNew<account_edit_page>(m_user_db.get(), &(*m_edit_user));
p->saved.connect([this] { setInternalPath(paths::account_list(), true); });
```

- [ ] **Step 26: Update handle_admin() — account-list case — in altinf_app.cpp**

Find:
```cpp
m_content->addNew<account_list_page>(
  *m_user_db, m_session, [this](const std::string& username) {
      if(username == m_session.username)
      {
          return;
      }
      const auto del_orgs     = m_org_db->orgs_for_user(username);
      const auto del_team_ids = m_kanban_db->team_ids_for_user(username);
      m_user_db->delete_user(username);
      m_org_db->remove_user_from_all_orgs(username);
      m_kanban_db->remove_member_from_all_teams(username);
      live_hub::instance().broadcast("accounts");
      for(const auto& org: del_orgs)
      {
          live_hub::instance().broadcast("org:" + std::to_string(org.id));
      }
      for(const auto tid: del_team_ids)
      {
          live_hub::instance().broadcast("team:" + std::to_string(tid));
      }
      handle_path(paths::account_list());
  });
```

Replace with:
```cpp
auto* p = m_content->addNew<account_list_page>(*m_user_db, m_session);
p->deleted.connect([this](const std::string& username) {
    if(username == m_session.username)
    {
        return;
    }
    const auto del_orgs     = m_org_db->orgs_for_user(username);
    const auto del_team_ids = m_kanban_db->team_ids_for_user(username);
    m_user_db->delete_user(username);
    m_org_db->remove_user_from_all_orgs(username);
    m_kanban_db->remove_member_from_all_teams(username);
    live_hub::instance().broadcast("accounts");
    for(const auto& org: del_orgs)
    {
        live_hub::instance().broadcast("org:" + std::to_string(org.id));
    }
    for(const auto tid: del_team_ids)
    {
        live_hub::instance().broadcast("team:" + std::to_string(tid));
    }
    handle_path(paths::account_list());
});
```

- [ ] **Step 27: Build and verify**

```bash
cmake --build build --parallel $(nproc)
```

Expected: clean build, zero errors.

- [ ] **Step 28: Commit**

```bash
git add src/auth/pages/login_page.hpp \
        src/auth/pages/login_page.cpp \
        src/blog/pages/blog_edit_page.hpp \
        src/blog/pages/blog_edit_page.cpp \
        src/link/pages/link_edit_page.hpp \
        src/link/pages/link_edit_page.cpp \
        src/link/pages/link_list_page.hpp \
        src/link/pages/link_list_page.cpp \
        src/admin/account/pages/account_edit_page.hpp \
        src/admin/account/pages/account_edit_page.cpp \
        src/admin/account/pages/account_list_page.hpp \
        src/admin/account/pages/account_list_page.cpp \
        src/org/pages/notifications_page.hpp \
        src/org/pages/notifications_page.cpp \
        src/altinf_app.cpp
git commit -m "refactor: replace remaining page callbacks with Wt::Signal"
```

---

## Task 4: Full build and test suite

- [ ] **Step 1: Full clean build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: zero errors, zero warnings about unused variables (the removed `m_on_xxx` members).

- [ ] **Step 2: Catch2 unit tests**

```bash
cd build && ctest --output-on-failure && cd ..
```

Expected: all tests pass. These tests cover DB logic and do not instantiate widgets, so no failures are expected from this refactor. If anything fails, it is a pre-existing issue unrelated to this change — investigate before continuing.

- [ ] **Step 3: JS unit tests**

```bash
cd tests/js && npm test && cd ../..
```

Expected: all tests pass. The gantt JS logic is unchanged.

- [ ] **Step 4: Playwright E2E tests**

```bash
cd e2e && npx playwright test && cd ..
```

Expected: all tests pass. The E2E suite covers login, blog editing, link management, kanban board, gantt, task editing, task popup, and notifications — all the flows touched by this refactor.

If any E2E test fails, the signal is likely being emitted but the `.connect()` call in `altinf_app.cpp` is missing or has a typo. Cross-check the failing test's flow against the corresponding `handle_xxx()` method in `altinf_app.cpp`.

- [ ] **Step 5: Commit**

```bash
git commit --allow-empty -m "test: confirm all suites pass after Wt::Signal refactor"
```

(Use `--allow-empty` only if no source files were changed in this task. If any fixes were needed, stage and commit them normally.)
