# Client-Side Markdown Rendering Design

**Date:** 2026-05-28
**Status:** Approved

## Overview

Replace all server-side cmark markdown rendering in AltInf with two new Wt widgets backed by Toast UI Editor v3. The server continues to store raw markdown text; rendering moves entirely to the browser.

## Library: Toast UI Editor v3

**Source:** npm package `@toast-ui/editor@3` (MIT license)  
**Self-hosted** in `resources/js/` and `resources/css/` — no CDN dependency.

Two bundles are used:

| Bundle | Size (min) | Purpose |
|---|---|---|
| `toastui-editor.min.js` | ~420 KB | Full WYSIWYG editor + markdown tab |
| `toastui-editor-viewer.min.js` | ~180 KB | Viewer only — same parser as editor |
| `toastui-editor.min.css` | — | Shared CSS for both |

The viewer bundle uses the exact same markdown parser as the full editor, guaranteeing that stored content renders identically in both edit and display contexts.

## New Widgets

### `markdown_editor_widget`

Located at `src/widgets/markdown_editor_widget.hpp/.cpp`.

Wraps the full Toast UI Editor. Presents WYSIWYG mode by default; the editor's built-in "Markdown" tab exposes the raw markdown source.

**C++ API:**

```cpp
class markdown_editor_widget : public Wt::WContainerWidget {
public:
    explicit markdown_editor_widget(const std::string& initial = "");

    // Last synced markdown value (updated on blur)
    const std::string& value() const;

    // Fires when the editor loses focus, carrying the new markdown string
    Wt::Signal<std::string>& changed();
};
```

**JS bridge (content out — JS → C++):**

- A hidden `WLineEdit` is added to the widget (using the standard hidden-input callback pattern).
- Toast UI's `onChange` event updates `hiddenEl.value = editor.getMarkdown()` silently (no round-trip).
- Toast UI's `onBlur` event additionally calls `hiddenEl.dispatchEvent(new Event('change'))`, triggering Wt's `changed()` signal on the hidden input.
- The C++ slot stores the value in `m_value` and fires `markdown_editor_widget::changed()`.

**JS bridge (content in — C++ → JS):**

- The constructor calls `doJavaScript()` to initialize the editor with the JSON-escaped markdown string as `initialValue`.
- Wt's `doJavaScript()` is queued and runs after the DOM is ready, so the mount element is guaranteed to exist.

**Save button interaction:**

In all major browsers, clicking a button causes the focused editor to blur first. The blur fires `dispatchEvent('change')`, so C++ has the current value before any button-click handler runs. No special save-path handling is needed.

### `markdown_viewer_widget`

Located at `src/widgets/markdown_viewer_widget.hpp/.cpp`.

Wraps the Toast UI Viewer. Renders stored markdown to HTML client-side. No editing controls.

**C++ API:**

```cpp
class markdown_viewer_widget : public Wt::WContainerWidget {
public:
    explicit markdown_viewer_widget(const std::string& markdown = "");

    // Replace displayed content without a page reload
    void set_content(const std::string& markdown);
};
```

**JS bridge:**

- Constructor: `doJavaScript()` calls `new toastui.Viewer({ el, initialValue: md })` and stores the instance as `el._viewer`.
- `set_content()`: calls `doJavaScript()` → `el._viewer.setMarkdown(md)`.
- No JS → C++ communication needed.

**JS loading:**

Both widgets load their respective JS bundle once per application session using `WApplication::instance()->require(...)`. Wt deduplicates `require()` calls for the same URL. CSS is loaded via `WApplication::instance()->useStyleSheet(WLink("css/toastui-editor.min.css"))`, also deduplicated.

## Usage Mapping

| Location | Before | After |
|---|---|---|
| `blog_view_page` | `cmark_markdown_to_html` → `WText(UnsafeXHTML)` | `markdown_viewer_widget` |
| `blog_edit_page` | `WTextArea` + `WStackedWidget` + server preview tab | `markdown_editor_widget` |
| task description (display) | `render_markdown()` → `WText(UnsafeXHTML)` | `markdown_viewer_widget` |
| task description (edit) | `WTextArea` | `markdown_editor_widget` |
| task comments (display) | `cmark_markdown_to_html` → `WText(UnsafeXHTML)` | `markdown_viewer_widget` |
| task comments (edit) | `WTextArea` | `markdown_editor_widget` |

## Task Editor Click-to-Edit Pattern

The existing click-to-edit UX is preserved. The display widget and edit widget are swapped on click/blur as before:

1. Display: `markdown_viewer_widget` shown, `markdown_editor_widget` hidden.
2. User clicks → `markdown_viewer_widget` hides, `markdown_editor_widget` shows.
3. Editor blur → `changed()` fires with new markdown → `markdown_viewer_widget::set_content(v)` → viewer shown, editor hidden.

The `enter_edit_mode` helper in `task_editor_form_widget` is updated to accept the new widget types in place of `Wt::WText*` / `Wt::WTextArea*`.

## Blog Edit Page Simplification

The Write/Preview tab bar (`WPushButton` pair), `WStackedWidget`, server-side preview container, and `preview_tab->clicked()` handler are all removed. The `markdown_editor_widget` replaces all of them — Toast UI's built-in WYSIWYG/Markdown tabs serve the same purpose without a server round-trip.

## Files Added

| File | Purpose |
|---|---|
| `src/widgets/markdown_editor_widget.hpp/.cpp` | Editor widget |
| `src/widgets/markdown_viewer_widget.hpp/.cpp` | Viewer widget |
| `resources/js/toastui-editor.min.js` | Editor JS bundle |
| `resources/js/toastui-editor-viewer.min.js` | Viewer JS bundle |
| `resources/css/toastui-editor.min.css` | Shared CSS |

The three bundle files are sourced from `node_modules/@toast-ui/editor/dist/` (run `npm install @toast-ui/editor@3` once to obtain them) and committed directly to the repository alongside the existing `resources/js/kanban.js` and `resources/js/gantt.js`. The CMake post-build copy step that already mirrors `resources/` into `build/resources/` requires no changes.

## Files Modified

| File | Change |
|---|---|
| `src/blog/pages/blog_view_page.cpp` | Replace cmark + `WText` with `markdown_viewer_widget` |
| `src/blog/pages/blog_edit_page.cpp` | Replace `WTextArea` + preview infrastructure with `markdown_editor_widget` |
| `src/org/widgets/task_editor_form_widget.cpp` | Replace all cmark display and `WTextArea` edit widgets |
| `CMakeLists.txt` | Add widget sources; remove cmark linkage; add bundle-copy step |

## Removed

- `#include <cmark.h>` from `blog_view_page.cpp`, `blog_edit_page.cpp`, `task_editor_form_widget.cpp`
- All `cmark_markdown_to_html()` call sites (five total) and associated `free()` calls
- `render_markdown()` private helper in `task_editor_form_widget`
- Write/Preview tab bar and `WStackedWidget` in `blog_edit_page`
- cmark `target_link_libraries` entry in `CMakeLists.txt`

## Testing

Existing Playwright specs that interact with markdown fields (`blog.spec.ts`, `comments.spec.ts`, `live-task-editor.spec.ts`) currently type into `<textarea>` elements. Toast UI Editor uses a `contenteditable` div as its editing surface (class `toastui-editor-contents`). Playwright's `fill()` works on contenteditable elements; affected tests need selector updates. Tests that click the old "Preview" tab in blog editing are removed.

No changes are required to Catch2 unit tests or JS unit tests — those do not cover markdown rendering.
