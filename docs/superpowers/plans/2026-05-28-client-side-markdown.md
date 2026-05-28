# Client-Side Markdown Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all server-side cmark markdown rendering with two new Wt widgets (`markdown_viewer_widget`, `markdown_editor_widget`) backed by Toast UI Editor v3, so markdown is rendered and edited entirely client-side.

**Architecture:** Two self-contained Wt widgets in `src/widgets/`. The viewer wraps `toastui.Viewer` (lightweight read-only renderer); the editor wraps `toastui.Editor` (WYSIWYG + raw Markdown tabs). Both load their JS bundles via `WApplication::require()` (deduplicated per session). The editor syncs its markdown value to C++ through the hidden-input callback pattern: `onChange` silently updates a hidden `WLineEdit`'s `.value`; `onBlur` dispatches a DOM `change` event to trigger the Wt round-trip.

**Tech Stack:** C++23, Wt 4.13.1, Toast UI Editor v3 (npm `@toast-ui/editor`), Playwright for E2E tests.

---

## Task 1: Obtain Toast UI bundles and update CMakeLists.txt

**Files:**
- Create: `resources/js/toastui-editor.min.js`
- Create: `resources/js/toastui-editor-viewer.min.js`
- Create: `resources/css/toastui-editor.min.css`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Install the npm package to a scratch location and copy the three dist files**

```bash
cd /tmp
npm install @toast-ui/editor@3
cp node_modules/@toast-ui/editor/dist/toastui-editor.min.js \
   /home/altainia/code/altinf/resources/js/
cp node_modules/@toast-ui/editor/dist/toastui-editor-viewer.min.js \
   /home/altainia/code/altinf/resources/js/
cp node_modules/@toast-ui/editor/dist/toastui-editor.min.css \
   /home/altainia/code/altinf/resources/css/
```

Verify the files are present:
```bash
ls -lh /home/altainia/code/altinf/resources/js/toastui-editor*.js \
        /home/altainia/code/altinf/resources/css/toastui-editor*.css
```
Expected: three files, sizes roughly 420 KB (editor JS), 180 KB (viewer JS), 60–80 KB (CSS).

- [ ] **Step 2: Remove cmark from CMakeLists.txt and add a note about the bundles**

In `CMakeLists.txt`, remove these two lines:
```cmake
find_library(CMARK_LIB cmark REQUIRED)
```
and from `target_link_libraries`:
```cmake
  ${CMARK_LIB}
```

The full `target_link_libraries` block after the change:
```cmake
target_link_libraries(altinf PRIVATE
  Wt::Wt Wt::HTTP wtdbo wtdbosqlite3
  OpenSSL::SSL OpenSSL::Crypto
)
```

- [ ] **Step 3: Do a clean configure + build to verify it compiles without cmark**

```bash
cd /home/altainia/code/altinf/build
cmake .. && make -j$(nproc) 2>&1 | tail -20
```

Expected: build succeeds. At this point the binary still uses cmark in the C++ source — that is fine; CMakeLists.txt removal is done first so we know the linker dependency is gone. The C++ `#include <cmark.h>` will be removed source-file by source-file in later tasks.

- [ ] **Step 4: Commit**

```bash
cd /home/altainia/code/altinf
git add resources/js/toastui-editor.min.js \
        resources/js/toastui-editor-viewer.min.js \
        resources/css/toastui-editor.min.css \
        CMakeLists.txt
git commit -m "build: add Toast UI Editor v3 bundles; remove cmark from CMake"
```

---

## Task 2: markdown_viewer_widget

**Files:**
- Create: `src/widgets/markdown_viewer_widget.hpp`
- Create: `src/widgets/markdown_viewer_widget.cpp`

- [ ] **Step 1: Create the header**

`src/widgets/markdown_viewer_widget.hpp`:
```cpp
#pragma once

#include <Wt/WContainerWidget.h>

#include <string>

class markdown_viewer_widget: public Wt::WContainerWidget
{
public:
    explicit markdown_viewer_widget(const std::string& markdown = "");

    void set_content(const std::string& markdown);

private:
    std::string m_mount_id;
};
```

- [ ] **Step 2: Create the implementation**

`src/widgets/markdown_viewer_widget.cpp`:
```cpp
#include "markdown_viewer_widget.hpp"

#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WString.h>

markdown_viewer_widget::markdown_viewer_widget(const std::string& markdown)
{
    WApplication::instance()->require("js/toastui-editor-viewer.min.js");
    WApplication::instance()->useStyleSheet(Wt::WLink("css/toastui-editor.min.css"));

    auto* mount = addNew<Wt::WContainerWidget>();
    m_mount_id  = mount->id();

    const auto js_md = Wt::WString::fromUTF8(markdown).jsStringLiteral('"');
    doJavaScript(
      "var el=document.getElementById('" + m_mount_id + "');"
      "el._viewer=new toastui.Viewer({el:el,initialValue:" + js_md + "});"
    );
}

void markdown_viewer_widget::set_content(const std::string& markdown)
{
    const auto js_md = Wt::WString::fromUTF8(markdown).jsStringLiteral('"');
    doJavaScript(
      "var el=document.getElementById('" + m_mount_id + "');"
      "if(el&&el._viewer){el._viewer.setMarkdown(" + js_md + ");}"
    );
}
```

- [ ] **Step 3: Build to verify it compiles**

```bash
cd /home/altainia/code/altinf/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: builds cleanly. The new source file is picked up automatically by `file(GLOB_RECURSE SOURCES src/*.cpp)`.

- [ ] **Step 4: Commit**

```bash
cd /home/altainia/code/altinf
git add src/widgets/markdown_viewer_widget.hpp src/widgets/markdown_viewer_widget.cpp
git commit -m "feat: add markdown_viewer_widget (Toast UI Viewer wrapper)"
```

---

## Task 3: markdown_editor_widget

**Files:**
- Create: `src/widgets/markdown_editor_widget.hpp`
- Create: `src/widgets/markdown_editor_widget.cpp`

- [ ] **Step 1: Create the header**

`src/widgets/markdown_editor_widget.hpp`:
```cpp
#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WSignal.h>

#include <string>

class markdown_editor_widget: public Wt::WContainerWidget
{
public:
    explicit markdown_editor_widget(const std::string& initial = "");

    // Last value synced from the editor (updated when the editor loses focus).
    const std::string& value() const;

    // Call after showing this widget (e.g. entering edit mode) to
    // move keyboard focus into the TUI editor surface.
    void focus();

    // Fires when the editor loses focus, carrying the current markdown string.
    Wt::Signal<std::string>& changed();

private:
    std::string             m_mount_id;
    std::string             m_value;
    Wt::WLineEdit*          m_hidden{nullptr};
    Wt::Signal<std::string> m_changed;
};
```

- [ ] **Step 2: Create the implementation**

`src/widgets/markdown_editor_widget.cpp`:
```cpp
#include "markdown_editor_widget.hpp"

#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WString.h>

markdown_editor_widget::markdown_editor_widget(const std::string& initial):
  m_value{initial}
{
    WApplication::instance()->require("js/toastui-editor.min.js");
    WApplication::instance()->useStyleSheet(Wt::WLink("css/toastui-editor.min.css"));

    auto* mount = addNew<Wt::WContainerWidget>();
    m_mount_id  = mount->id();

    m_hidden = addNew<Wt::WLineEdit>();
    m_hidden->hide();
    const std::string cb_id = "mdcb_" + id();
    m_hidden->setId(cb_id);
    m_hidden->changed().connect([this] {
        m_value = m_hidden->text().toUTF8();
        m_changed.emit(m_value);
    });

    const auto js_md = Wt::WString::fromUTF8(initial).jsStringLiteral('"');
    doJavaScript(
      "var el=document.getElementById('" + m_mount_id + "');"
      "var cb=document.getElementById('" + cb_id + "');"
      "el._editor=new toastui.Editor({"
      "  el:el,height:'auto',initialEditType:'wysiwyg',"
      "  initialValue:" + js_md + ","
      "  events:{"
      "    change:function(){cb.value=el._editor.getMarkdown();},"
      "    blur:function(){cb.dispatchEvent(new Event('change'));}"
      "  }"
      "});"
    );
}

const std::string& markdown_editor_widget::value() const
{
    return m_value;
}

void markdown_editor_widget::focus()
{
    doJavaScript(
      "var el=document.getElementById('" + m_mount_id + "');"
      "if(el&&el._editor){el._editor.focus();}"
    );
}

Wt::Signal<std::string>& markdown_editor_widget::changed()
{
    return m_changed;
}
```

- [ ] **Step 3: Build to verify it compiles**

```bash
cd /home/altainia/code/altinf/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: builds cleanly.

- [ ] **Step 4: Commit**

```bash
cd /home/altainia/code/altinf
git add src/widgets/markdown_editor_widget.hpp src/widgets/markdown_editor_widget.cpp
git commit -m "feat: add markdown_editor_widget (Toast UI WYSIWYG wrapper)"
```

---

## Task 4: Replace blog_view_page

**Files:**
- Modify: `src/blog/pages/blog_view_page.cpp`

The blog view page reads a markdown file and renders it server-side with cmark. Replace the entire rendering section with `markdown_viewer_widget`.

- [ ] **Step 1: Rewrite `blog_view_page.cpp`**

Replace the content of `src/blog/pages/blog_view_page.cpp` in full. The only changes are: remove `#include <cmark.h>`, add `#include "widgets/markdown_viewer_widget.hpp"`, and replace the cmark call + WText with a `markdown_viewer_widget`:

```cpp
#include "blog_view_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <fstream>
#include <sstream>

#include "paths.hpp"
#include "widgets/markdown_viewer_widget.hpp"

blog_view_page::blog_view_page(const blog_post& post, const session_data& session)
{
    setStyleClass("page blog-post-page");

    auto* header = addNew<Wt::WContainerWidget>();
    header->setStyleClass("post-header");

    header->addNew<Wt::WText>("<h1>" + post.title + "</h1>", Wt::TextFormat::UnsafeXHTML);

    std::string date_html =
      "<span class=\"post-date\">Posted: " + post.date.toString("yyyy-MM-dd").toUTF8() + "</span>";
    if(post.last_modified)
    {
        date_html +=
          " <span class=\"post-date post-date-modified\">Updated: " +
          post.last_modified->toString("yyyy-MM-dd").toUTF8() + "</span>";
    }
    header->addNew<Wt::WText>(date_html, Wt::TextFormat::UnsafeXHTML);

    if(!post.tags.empty())
    {
        auto* tag_row = header->addNew<Wt::WContainerWidget>();
        tag_row->setStyleClass("tag-row");

        for(const auto& tag: post.tags)
        {
            auto* chip = tag_row->addNew<Wt::WAnchor>(
              Wt::WLink{Wt::LinkType::InternalPath, paths::blog_list()}, tag);
            chip->setStyleClass("tag-chip");
        }
    }

    if(session.permissions.has_any(permission::post_write))
    {
        auto* edit_link = header->addNew<Wt::WAnchor>(
          Wt::WLink{Wt::LinkType::InternalPath, paths::blog_edit(post.slug)}, "Edit");
        edit_link->setStyleClass("post-edit-link");
    }

    // Read markdown body (after frontmatter)
    std::ifstream file{post.filepath};
    std::string   line;
    bool          in_frontmatter   = false;
    bool          past_frontmatter = false;
    std::string   body;

    while(std::getline(file, line))
    {
        if(!past_frontmatter)
        {
            if(line == "---" && !in_frontmatter)
            {
                in_frontmatter = true;
                continue;
            }
            if(line == "---" && in_frontmatter)
            {
                past_frontmatter = true;
                continue;
            }
            if(in_frontmatter)
            {
                continue;
            }
        }
        body += line + "\n";
    }

    auto* content = addNew<Wt::WContainerWidget>();
    content->setStyleClass("post-content");
    content->addNew<markdown_viewer_widget>(body);
}
```

- [ ] **Step 2: Build**

```bash
cd /home/altainia/code/altinf/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: builds cleanly.

- [ ] **Step 3: Smoke test — navigate to a blog post and verify it renders**

Run the app and open a blog post in the browser. The post content should render as formatted HTML (headings, paragraphs, code blocks) rather than raw markdown text.

- [ ] **Step 4: Commit**

```bash
cd /home/altainia/code/altinf
git add src/blog/pages/blog_view_page.cpp
git commit -m "feat: render blog posts with markdown_viewer_widget (client-side)"
```

---

## Task 5: Replace blog_edit_page

**Files:**
- Modify: `src/blog/pages/blog_edit_page.hpp`
- Modify: `src/blog/pages/blog_edit_page.cpp`

The blog edit page has a `WTextArea`, a `WStackedWidget`, and a Write/Preview tab bar backed by a server-side cmark preview. All of that is replaced by a single `markdown_editor_widget`. Toast UI's own WYSIWYG/Markdown tabs replace the Write/Preview bar.

- [ ] **Step 1: Rewrite `blog_edit_page.hpp`**

```cpp
#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>

#include <filesystem>
#include <string>

#include "blog/blog_post.hpp"
#include "widgets/markdown_editor_widget.hpp"

class blog_edit_page: public Wt::WContainerWidget
{
public:
    // existing == nullptr  ->  new post
    // existing != nullptr  ->  edit post (slug fixed to avoid breaking URLs)
    blog_edit_page(const std::filesystem::path& posts_dir,
                   const blog_post*             existing);

    Wt::Signal<std::string> saved;

private:
    std::filesystem::path  m_posts_dir;
    const blog_post*       m_existing{nullptr};
    Wt::WLineEdit*         m_title{nullptr};
    Wt::WLineEdit*         m_tags{nullptr};
    markdown_editor_widget* m_body_editor{nullptr};
    Wt::WText*             m_status{nullptr};

    void               save();
    static std::string read_body(const blog_post& post);
};
```

- [ ] **Step 2: Rewrite `blog_edit_page.cpp`**

```cpp
#include "blog_edit_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WDate.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <ranges>
#include <sstream>

#include "blog/post_writer.hpp"
#include "paths.hpp"

blog_edit_page::blog_edit_page(const std::filesystem::path& posts_dir,
                               const blog_post*             existing):
  m_posts_dir{posts_dir},
  m_existing{existing}
{
    setStyleClass("page post-editor-page");

    auto* form = addNew<Wt::WContainerWidget>();
    form->setStyleClass("post-editor-form");

    const auto heading = m_existing ? std::string{"Edit Post"} : std::string{"New Post"};
    form->addNew<Wt::WText>("<h2>" + heading + "</h2>", Wt::TextFormat::UnsafeXHTML);

    m_title = form->addNew<Wt::WLineEdit>();
    m_title->setPlaceholderText("Title");
    m_title->setStyleClass("editor-field");

    m_tags = form->addNew<Wt::WLineEdit>();
    m_tags->setPlaceholderText("Tags (comma-separated)");
    m_tags->setStyleClass("editor-field");

    const auto initial_body = m_existing ? read_body(*m_existing) : std::string{};
    m_body_editor = form->addNew<markdown_editor_widget>(initial_body);
    m_body_editor->setStyleClass("post-body-editor");

    m_status = form->addNew<Wt::WText>();
    m_status->setStyleClass("editor-status");

    auto* btn_row = form->addNew<Wt::WContainerWidget>();
    btn_row->setStyleClass("editor-btn-row");

    auto* save_btn = btn_row->addNew<Wt::WPushButton>("Save");
    save_btn->setStyleClass("editor-btn");
    save_btn->clicked().connect(this, &blog_edit_page::save);

    const auto cancel_path =
      m_existing ? paths::blog_view(m_existing->slug) : paths::blog_list();
    auto* cancel_btn = btn_row->addNew<Wt::WAnchor>(
      Wt::WLink{Wt::LinkType::InternalPath, cancel_path}, "Cancel");
    cancel_btn->setStyleClass("editor-btn editor-btn-cancel");

    if(m_existing)
    {
        m_title->setText(m_existing->title);

        const auto tags_str =
          m_existing->tags |
          std::views::join_with(std::string(", ")) |
          std::ranges::to<std::string>();

        m_tags->setText(tags_str);
    }
}

void blog_edit_page::save()
{
    const auto title = m_title->text().toUTF8();
    const auto tags  = m_tags->text().toUTF8();
    const auto body  = m_body_editor->value();

    if(title.empty())
    {
        m_status->setText("Title is required.");
        return;
    }

    m_status->setText("");

    std::filesystem::path    filepath;
    std::string              slug;
    Wt::WDate                date;
    std::optional<Wt::WDate> last_modified;

    if(m_existing)
    {
        filepath      = m_existing->filepath;
        slug          = m_existing->slug;
        date          = m_existing->date;
        last_modified = Wt::WDate::currentDate();
    }
    else
    {
        auto result = resolve_new_post(m_posts_dir, title);
        filepath    = result.filepath;
        slug        = result.slug;
        date        = Wt::WDate::currentDate();
    }

    if(!write_post_file(filepath, title, date, last_modified, tags, body))
    {
        m_status->setText("Failed to write file.");
        return;
    }

    saved.emit(slug);
}

std::string blog_edit_page::read_body(const blog_post& post)
{
    std::ifstream file{post.filepath};
    std::string   line;
    bool          in_frontmatter   = false;
    bool          past_frontmatter = false;
    std::string   body;

    while(std::getline(file, line))
    {
        if(!past_frontmatter)
        {
            if(line == "---" && !in_frontmatter)
            {
                in_frontmatter = true;
                continue;
            }
            if(line == "---" && in_frontmatter)
            {
                past_frontmatter = true;
                continue;
            }
            if(in_frontmatter)
                continue;
        }
        body += line + "\n";
    }
    return body;
}
```

- [ ] **Step 3: Build**

```bash
cd /home/altainia/code/altinf/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: builds cleanly. cmark is now gone from both blog pages.

- [ ] **Step 4: Smoke test — create or edit a blog post**

Run the app, navigate to a blog post, click Edit. The WYSIWYG editor should load with the post body. Switch to its "Markdown" tab to see the raw source. Edit content, save — the post should update.

- [ ] **Step 5: Commit**

```bash
cd /home/altainia/code/altinf
git add src/blog/pages/blog_edit_page.hpp src/blog/pages/blog_edit_page.cpp
git commit -m "feat: replace blog editor with markdown_editor_widget; remove server preview"
```

---

## Task 6: Replace task description (task_editor_form_widget)

**Files:**
- Modify: `src/org/widgets/task_editor_form_widget.hpp`
- Modify: `src/org/widgets/task_editor_form_widget.cpp`

Replace `m_desc_display` (`Wt::WText*`) and `m_desc_edit` (`Wt::WTextArea*`) with `m_desc_viewer` (`markdown_viewer_widget*`) and `m_desc_editor` (`markdown_editor_widget*`). Update `save()` and the description click-to-edit handlers.

- [ ] **Step 1: Update the header**

In `src/org/widgets/task_editor_form_widget.hpp`:

1. Remove `#include <Wt/WTextArea.h>` — it is no longer used anywhere in the header. (The .cpp uses it only for description and comments — both being replaced.)
2. Add after the existing includes:
```cpp
#include "widgets/markdown_editor_widget.hpp"
#include "widgets/markdown_viewer_widget.hpp"
```
3. Replace member declarations:
```cpp
// Remove:
Wt::WTextArea*        m_desc_edit{nullptr};
Wt::WText*            m_desc_display{nullptr};

// Add:
markdown_editor_widget* m_desc_editor{nullptr};
markdown_viewer_widget* m_desc_viewer{nullptr};
```
4. Remove the `render_markdown` private method declaration:
```cpp
// Remove:
std::string render_markdown(const std::string& md) const;
```

- [ ] **Step 2: Update the description construction block in `task_editor_form_widget.cpp`**

The block currently at lines 195–235 (description display + edit widgets + click/blur handlers). Replace it entirely:

```cpp
form->addNew<Wt::WText>("<h2>Description</h2>", Wt::TextFormat::UnsafeXHTML);
m_desc_field = form->addNew<Wt::WContainerWidget>();
m_desc_field->setStyleClass("kb-popup-field");

const std::string desc_val = is_new ? "" : m_original.description;

m_desc_viewer = m_desc_field->addNew<markdown_viewer_widget>(desc_val);
m_desc_viewer->setStyleClass("kb-desc-display");
if(can_edit && !is_new)
{
    m_desc_viewer->addStyleClass("kb-popup-display");
}
if(is_new)
{
    m_desc_viewer->hide();
}

m_desc_editor = m_desc_field->addNew<markdown_editor_widget>(desc_val);
m_desc_editor->setStyleClass("kb-desc-field");
if(!is_new)
{
    m_desc_editor->hide();
}

if(can_edit && !is_new)
{
    m_desc_viewer->clicked().connect([this] {
        m_desc_viewer->hide();
        m_desc_editor->show();
        m_desc_editor->focus();
    });

    m_desc_editor->changed().connect([this](const std::string& v) {
        m_desc_editor->hide();
        m_desc_viewer->set_content(v);
        m_desc_viewer->show();
        if(v == m_original.description)
        {
            unmark_field_dirty("description", m_desc_field);
        }
        else
        {
            mark_field_dirty("description", m_desc_field);
        }
    });
}
```

- [ ] **Step 3: Update `save()` to read from `m_desc_editor`**

In `save()`, change the one line that reads the description:
```cpp
// Remove:
t.description = m_desc_edit->text().toUTF8();

// Replace with:
t.description = m_desc_editor->value();
```

- [ ] **Step 4: Remove `render_markdown()` from the .cpp**

Delete the private helper at the bottom of the file (the entire function body, lines ~872–885):
```cpp
std::string task_editor_form_widget::render_markdown(const std::string& md) const
{
    if(md.empty())
    {
        return "<em>(none)</em>";
    }
    char*       raw  = cmark_markdown_to_html(md.c_str(), md.size(), CMARK_OPT_DEFAULT);
    std::string html = raw ? std::string(raw) : md;
    if(raw)
    {
        free(raw);
    }
    return html;
}
```

- [ ] **Step 5: Remove `#include <Wt/WTextArea.h>` from the .cpp top**

This include is no longer used. It will be re-introduced in the next task if still needed — but we're also replacing comment WTextAreas in Task 7, so remove it now.

- [ ] **Step 6: Build**

```bash
cd /home/altainia/code/altinf/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: clean build. If the compiler complains about `WTextArea` still being referenced (comment areas), those are fixed in Task 7 — temporarily re-add the include if needed just to unblock the build.

- [ ] **Step 7: Smoke test — open a task and verify description editing**

Run the app. Open an existing task. The description should render as formatted markdown. Click the description to enter edit mode — the WYSIWYG editor should appear. Edit content, click away — the viewer should update. Create a new task — description editor should appear immediately.

- [ ] **Step 8: Commit**

```bash
cd /home/altainia/code/altinf
git add src/org/widgets/task_editor_form_widget.hpp \
        src/org/widgets/task_editor_form_widget.cpp
git commit -m "feat: replace task description display/edit with markdown widgets"
```

---

## Task 7: Replace task comments; remove cmark entirely

**Files:**
- Modify: `src/org/widgets/task_editor_form_widget.cpp`
- Modify: `src/org/widgets/task_editor_form_widget.hpp` (remove `#include <Wt/WTextArea.h>` if still present)

The `rebuild_comments()` function creates comment display bodies (using cmark) and inline edit areas (using `WTextArea`). Replace all of them with the new widgets. Also replace the comment compose `WTextArea`. This is the last place cmark is used.

- [ ] **Step 1: Add the new widget headers at the top of `task_editor_form_widget.cpp`**

If not already present, ensure these lines are at the top of the .cpp:
```cpp
#include "widgets/markdown_editor_widget.hpp"
#include "widgets/markdown_viewer_widget.hpp"
```

And remove:
```cpp
#include <cmark.h>
#include <Wt/WTextArea.h>   // if it's still there from the previous task
```

- [ ] **Step 2: Replace comment display in `rebuild_comments()`**

Find the block that renders a comment body (around line 1182–1190 in the original):
```cpp
auto* bw = item->addNew<Wt::WContainerWidget>();
bw->setStyleClass("kb-comment-body");
char* raw2 = cmark_markdown_to_html(
  c.body.c_str(), c.body.size(), CMARK_OPT_DEFAULT);
bw->addNew<Wt::WText>(raw2 ? std::string(raw2) : "", Wt::TextFormat::UnsafeXHTML);
if(raw2)
{
    free(raw2);
}
```

Replace with:
```cpp
auto* bw = item->addNew<markdown_viewer_widget>(c.body);
bw->setStyleClass("kb-comment-body");
```

Note: `bw` is used below in `edit_btn->clicked()` and `cancel_eb->clicked()` to hide/show the body during editing. Since `markdown_viewer_widget` inherits from `WContainerWidget`, `bw->hide()` and `bw->show()` work unchanged.

- [ ] **Step 3: Replace comment edit `WTextArea` with `markdown_editor_widget`**

Find the comment edit area creation (around lines 1212–1222):
```cpp
auto* ea = item->addNew<Wt::WContainerWidget>();
ea->setStyleClass("kb-comment-edit-area");
ea->hide();
auto* eta = ea->addNew<Wt::WTextArea>();
eta->setText(c.body);
eta->setStyleClass("editor-field");
```

Replace with:
```cpp
auto* ea = item->addNew<Wt::WContainerWidget>();
ea->setStyleClass("kb-comment-edit-area");
ea->hide();
auto* eta = ea->addNew<markdown_editor_widget>(c.body);
```

Then update the save button click handler that reads `eta->text().toUTF8()`:
```cpp
// Remove:
const std::string nb = eta->text().toUTF8();

// Replace with:
const std::string nb = eta->value();
```

Also update the `edit_btn->clicked()` handler to call `eta->focus()` after showing:
```cpp
edit_btn->clicked().connect([this, cauthor, is_own, bw, ea, actions, eta] {
    if(is_own)
    {
        bw->hide();
        actions->hide();
        ea->show();
        eta->focus();
    }
    else
    {
        auto* d = new Wt::WDialog("Edit Another User's Comment");
        d->contents()->addNew<Wt::WText>(
          "This comment was written by " + cauthor +
            ". Are you sure you want to edit it?",
          Wt::TextFormat::Plain);
        auto* yes = d->footer()->addNew<Wt::WPushButton>("Edit Anyway");
        yes->setStyleClass("editor-btn");
        auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
        no->setStyleClass("editor-btn editor-btn-cancel");
        yes->clicked().connect([d, bw, ea, actions, eta] {
            d->accept(); bw->hide(); actions->hide(); ea->show(); eta->focus(); });
        no->clicked().connect([d] { d->reject(); });
        d->finished().connect([d](Wt::DialogCode) { delete d; });
        d->show();
    }
});
```

- [ ] **Step 4: Replace comment compose `WTextArea` with `markdown_editor_widget`**

Find the compose area creation at the bottom of `rebuild_comments()` (around lines 1299–1322):
```cpp
auto* ta = m_comment_compose->addNew<Wt::WTextArea>();
ta->setPlaceholderText("Write a comment (Markdown supported)");
ta->setStyleClass("editor-field");
auto* post = m_comment_compose->addNew<Wt::WPushButton>("Post Comment");
post->setStyleClass("editor-btn kb-comment-post-btn");
post->setDisabled(true);
ta->keyWentUp().connect([ta, post] { post->setDisabled(ta->text().empty()); });
post->clicked().connect([this, ta, post, alive = m_alive] {
    if(!*alive) { return; }
    const std::string body = ta->text().toUTF8();
    if(body.empty()) { return; }
    post->setDisabled(true);
    const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
    m_db.add_comment(tid, m_username, body);
    live_hub::instance().broadcast("task:" + std::to_string(tid) + ":comments");
    ta->setText(Wt::WString{});
    rebuild_comments();
});
```

Replace with:
```cpp
auto* ta = m_comment_compose->addNew<markdown_editor_widget>();
auto* post = m_comment_compose->addNew<Wt::WPushButton>("Post Comment");
post->setStyleClass("editor-btn kb-comment-post-btn");
post->setDisabled(true);
// Enable the Post button the first time the editor loses focus with content.
ta->changed().connect([ta, post](const std::string& v) {
    post->setDisabled(v.empty());
});
post->clicked().connect([this, ta, post, alive = m_alive] {
    if(!*alive) { return; }
    const std::string body = ta->value();
    if(body.empty()) { return; }
    post->setDisabled(true);
    const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
    m_db.add_comment(tid, m_username, body);
    live_hub::instance().broadcast("task:" + std::to_string(tid) + ":comments");
    rebuild_comments();
});
```

Note: `ta->setText(Wt::WString{})` is removed — `rebuild_comments()` clears and rebuilds the compose area, so a fresh editor is always created after posting.

- [ ] **Step 5: Verify no remaining cmark references**

```bash
grep -rn "cmark" /home/altainia/code/altinf/src/
```

Expected: no output. If any remain, remove them.

- [ ] **Step 6: Build**

```bash
cd /home/altainia/code/altinf/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: clean build with no cmark references.

- [ ] **Step 7: Smoke test — comments**

Run the app. Open an existing task with comments. Comments should render as formatted markdown. Click Edit on a comment — WYSIWYG editor appears with the comment content. Edit and save. Post a new comment — the compose editor appears; write content, click away (to enable the Post button), then click Post. The new comment should appear.

- [ ] **Step 8: Commit**

```bash
cd /home/altainia/code/altinf
git add src/org/widgets/task_editor_form_widget.hpp \
        src/org/widgets/task_editor_form_widget.cpp
git commit -m "feat: replace task comment display/edit/compose with markdown widgets; remove cmark"
```

---

## Task 8: Update Playwright E2E tests

**Files:**
- Modify: `e2e/specs/blog.spec.ts`
- Modify: `e2e/specs/comments.spec.ts`
- Modify: `e2e/specs/live-task-editor.spec.ts` (if it tests description editing)

The existing tests type into `<textarea>` elements for markdown fields. Toast UI Editor uses a `contenteditable` div (class `toastui-editor-contents`) as its editing surface. Playwright's `fill()` works on `contenteditable` elements. The blog edit test that clicks the "Preview" tab is removed.

- [ ] **Step 1: Check which specs interact with markdown editor fields**

```bash
grep -n "textarea\|editor-textarea\|editor-field\|Preview\|Write" \
     /home/altainia/code/altinf/e2e/specs/blog.spec.ts \
     /home/altainia/code/altinf/e2e/specs/comments.spec.ts \
     /home/altainia/code/altinf/e2e/specs/live-task-editor.spec.ts 2>/dev/null
```

Review which tests need updating.

- [ ] **Step 2: Update the blog edit spec**

Any test that types into `.editor-textarea` (the old `WTextArea` class) must now use `.toastui-editor-contents`. Any test that clicks a "Preview" tab must be removed or rewritten (the WYSIWYG tab in TUI is the preview; to check raw markdown, switch to TUI's "Markdown" tab).

Replace selectors like:
```typescript
// Old
await page.locator('.editor-textarea').fill('# Hello');
await page.locator('button', { hasText: 'Preview' }).click();

// New — fill the WYSIWYG surface
await page.locator('.toastui-editor-contents').fill('Hello');
// No preview tab needed — WYSIWYG shows rendered output directly.
// To switch to raw markdown tab:
// await page.locator('.toastui-tab-item', { hasText: 'Markdown' }).click();
```

- [ ] **Step 3: Update the comments spec**

The `postComment` helper currently fills `.kb-comment-compose textarea`:
```typescript
// Old
async function postComment(page: Page, body: string) {
  await page.locator('.kb-comment-compose textarea').pressSequentially(body);
  ...
}
```

Update to:
```typescript
async function postComment(page: Page, body: string) {
  await page.locator('.kb-comment-compose .toastui-editor-contents').click();
  await page.locator('.kb-comment-compose .toastui-editor-contents').fill(body);
  // Click outside the editor to trigger blur → enable the Post button
  await page.locator('.kb-comment-compose').press('Escape');
  const postBtn = page.locator('.kb-comment-post-btn');
  await expect(postBtn).toBeEnabled({ timeout: 3000 });
  await postBtn.click();
  await expect(page.locator('.kb-comment-item').last()).toBeVisible();
}
```

For the comment edit area (`.kb-comment-edit-area textarea`):
```typescript
// Old
await editItem.locator('.kb-comment-edit-area textarea').fill('Edited by B');

// New
await editItem.locator('.kb-comment-edit-area .toastui-editor-contents').fill('Edited by B');
```

- [ ] **Step 4: Run the full Playwright suite**

```bash
cd /home/altainia/code/altinf/e2e && npx playwright test
```

Expected: all tests pass. If tests fail due to selector or timing issues, adjust the affected locator or add a brief `waitFor`.

- [ ] **Step 5: Commit**

```bash
cd /home/altainia/code/altinf
git add e2e/specs/blog.spec.ts e2e/specs/comments.spec.ts
# Add any other modified spec files:
git add e2e/specs/live-task-editor.spec.ts 2>/dev/null || true
git commit -m "test: update E2E selectors for Toast UI Editor; remove server-preview test"
```

---

## Task 9: Final verification

- [ ] **Step 1: Confirm no remaining cmark references anywhere in the project**

```bash
grep -rn "cmark" /home/altainia/code/altinf/src/ /home/altainia/code/altinf/CMakeLists.txt
```

Expected: no output.

- [ ] **Step 2: Run the full test suite (all three suites)**

```bash
# Catch2
cd /home/altainia/code/altinf/build && ctest --output-on-failure

# JS unit tests
cd /home/altainia/code/altinf/tests/js && npm test

# Playwright E2E
cd /home/altainia/code/altinf/e2e && npx playwright test
```

Expected: all suites pass.

- [ ] **Step 3: Final commit if any last-minute fixes were made**

```bash
cd /home/altainia/code/altinf
git add -p   # review and stage any remaining changes
git commit -m "fix: post-migration cleanup"
```
