# SCSS Unification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Consolidate fragmented button and form styles into two new SCSS partials, remove rounded corners from surfaces (preserving circular badges and pill chips), and mute the magenta accent color.

**Architecture:** New `_buttons.scss` and `_forms.scss` partials define SCSS mixins for each variant; component files replace duplicated rule blocks with `@include` calls. The `--radius` CSS custom property is set to `0` to eliminate surface rounding globally. Accent color inputs are updated in `_variables.scss`; the existing contrast-checking machinery verifies WCAG compliance automatically.

**Tech Stack:** SCSS (Dart Sass), CSS custom properties, cmake build system (runs `sass` as a custom target)

---

## File Map

| Action | File | Purpose |
|---|---|---|
| Modify | `resources/scss/_variables.scss` | Accent color + `--radius` |
| Create | `resources/scss/_buttons.scss` | All button mixins |
| Create | `resources/scss/_forms.scss` | Input/select/textarea/section-heading mixins |
| Modify | `resources/scss/altinf.scss` | Add new imports |
| Modify | `resources/scss/_login.scss` | Use btn-primary + form-input |
| Modify | `resources/scss/_blog.scss` | Use btn-primary, btn-ghost, form-input, form-textarea, section-heading |
| Modify | `resources/scss/_kanban.scss` | Use button/form mixins, fix hardcoded border-radius |
| Modify | `resources/scss/_links.scss` | Use btn-ghost-xs, btn-ghost-danger-xs |
| Modify | `resources/scss/_wt.scss` | Use form-select mixin, fix hardcoded border-radius |
| Modify | `resources/scss/_org.scss` | Use section-heading mixin |

---

## Compile verification command

Run this after every task to check for SCSS errors (zero output = success):

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

---

## Task 1: Update `_variables.scss` — accent color and radius

**Files:**
- Modify: `resources/scss/_variables.scss`

- [ ] **Step 1: Update accent color inputs and radius**

Replace the two accent entries in `$text-intents` and `--radius` in `:root`:

```scss
// Before:
$text-intents: (
  'text':     #d4d4d4,
  'muted':    #6b6b6b,
  'accent':   #d279d2,
  'accent-h': #db94db,
  'error':    #e06c6c,
);
```

```scss
// After:
$text-intents: (
  'text':     #d4d4d4,
  'muted':    #6b6b6b,
  'accent':   #a673a6,
  'accent-h': #b58cb5,
  'error':    #e06c6c,
);
```

And in `:root`:

```scss
// Before:
  --radius:    4px;

// After:
  --radius:    0;
```

- [ ] **Step 2: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0. The contrast system may lighten `#a673a6` slightly if needed — that is expected behavior.

- [ ] **Step 3: Full build and visual check**

```bash
cmake --build build --parallel $(nproc)
```

Then start the server and open the app. Verify:
- Cards, dialogs, inputs, and buttons all have square corners
- The accent color on links, active borders, and the nav-bell badge is a muted dusty mauve (not vivid magenta)
- Tag pills, count badges, and type-chip dots remain circular/rounded

- [ ] **Step 4: Commit**

```bash
git add resources/scss/_variables.scss resources/css/altinf.css
git commit -m "style: mute accent color and remove border radius"
```

---

## Task 2: Create `_buttons.scss`

**Files:**
- Create: `resources/scss/_buttons.scss`

- [ ] **Step 1: Write the file**

Create `resources/scss/_buttons.scss` with the following content exactly:

```scss
// ── Base ──────────────────────────────────────────────────────────────────────

@mixin btn-base {
  border-radius: var(--radius);
  cursor:        pointer;
  font-family:   var(--font-sans);
  transition:    background 0.15s, color 0.15s;

  &:disabled { opacity: 0.35; cursor: default; }
}

// ── Role variants ─────────────────────────────────────────────────────────────

@mixin btn-primary {
  @include btn-base;
  background:  var(--color-accent);
  border:      none;
  color:       var(--color-bg);
  font-size:   0.95rem;
  font-weight: 600;
  padding:     0.5rem 1rem;

  &:hover { background: var(--color-accent-h); color: var(--color-bg); text-decoration: none; }
}

@mixin btn-ghost {
  @include btn-base;
  background: transparent;
  border:     1px solid var(--color-border);
  color:      var(--color-muted);
  font-size:  0.95rem;
  padding:    0.5rem 1rem;

  &:hover { background: var(--color-surface); color: var(--color-text); text-decoration: none; }
}

@mixin btn-danger {
  @include btn-base;
  background:  #7a2424;
  border:      1px solid #a33;
  color:       var(--color-text);
  font-size:   0.95rem;
  font-weight: 600;
  padding:     0.5rem 1rem;

  &:hover { background: #922; }
}

@mixin btn-ghost-danger {
  @include btn-ghost;
  border-color: #e06c6c33;
  color:        var(--color-error);

  &:hover { background: #e06c6c22; color: var(--color-error); }
}

// ── Size variants (compose with any role) ─────────────────────────────────────

@mixin btn-sm {
  font-size: 0.85rem;
  padding:   0.35rem 0.85rem;
}

@mixin btn-xs {
  font-size:   0.78rem;
  font-weight: 400;
  padding:     0.2rem 0.65rem;
}
```

- [ ] **Step 2: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0. (The file is not yet imported so no visual change.)

---

## Task 3: Create `_forms.scss`

**Files:**
- Create: `resources/scss/_forms.scss`

- [ ] **Step 1: Write the file**

Create `resources/scss/_forms.scss` with the following content exactly:

```scss
// ── Input fields ──────────────────────────────────────────────────────────────

@mixin form-input {
  background:    var(--color-bg);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  color:         var(--color-text);
  font-family:   var(--font-sans);
  font-size:     0.95rem;
  padding:       0.5rem 0.75rem;
  width:         100%;

  &:focus { border-color: var(--color-accent); outline: none; }
}

// ── Select dropdowns ──────────────────────────────────────────────────────────

@mixin form-select {
  background:    var(--color-surface);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  color:         var(--color-text);
  cursor:        pointer;
  font-family:   var(--font-sans);
  font-size:     0.85rem;
}

// ── Textareas ─────────────────────────────────────────────────────────────────

@mixin form-textarea {
  background:    var(--color-bg);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  color:         var(--color-text);
  font-family:   var(--font-mono);
  font-size:     0.88rem;
  resize:        vertical;
  width:         100%;

  &:focus { border-color: var(--color-accent); outline: none; }
}

// ── Section sub-headings ──────────────────────────────────────────────────────
// Uppercase h2 pattern: used in kanban editor, team forms, comment sections,
// links page, and org landing page.

@mixin section-heading {
  border-bottom:  1px solid var(--color-border);
  color:          var(--color-muted);
  font-size:      0.8rem;
  letter-spacing: 0.06em;
  padding-bottom: 0.3rem;
  text-transform: uppercase;
}
```

- [ ] **Step 2: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0.

---

## Task 4: Update `altinf.scss` — import new partials

**Files:**
- Modify: `resources/scss/altinf.scss`

- [ ] **Step 1: Add imports**

```scss
// Before:
@use 'variables';
@use 'base';
@use 'wt';

// After:
@use 'variables';
@use 'base';
@use 'buttons';
@use 'forms';
@use 'wt';
```

- [ ] **Step 2: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0. No visual change yet.

- [ ] **Step 3: Commit**

```bash
git add resources/scss/_buttons.scss resources/scss/_forms.scss resources/scss/altinf.scss resources/css/altinf.css
git commit -m "style: add _buttons.scss and _forms.scss mixin libraries"
```

---

## Task 5: Update `_login.scss`

**Files:**
- Modify: `resources/scss/_login.scss`

- [ ] **Step 1: Replace the file contents**

```scss
@use 'buttons';
@use 'forms';

.login-page {
  display:         flex;
  justify-content: center;
  padding-top:     4rem;
}

.login-form {
  background:     var(--color-surface);
  border:         1px solid var(--color-border);
  border-radius:  var(--radius);
  padding:        2rem;
  width:          100%;
  max-width:      360px;
  display:        flex;
  flex-direction: column;
  gap:            0.75rem;

  h2 { font-size: 1.25rem; margin-bottom: 0.25rem; }
}

.login-field { @include forms.form-input; }

.login-btn { @include buttons.btn-primary; }

.login-error {
  font-size:  0.85rem;
  color:      var(--color-error);
  min-height: 1.2em;
}
```

- [ ] **Step 2: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0.

- [ ] **Step 3: Visual check**

Build and start the server. Navigate to the login page. Verify:
- The username/password fields have square corners and match the dark background styling
- The "Log in" button is muted mauve, square, and has the same proportions as before
- Focus ring on fields is muted mauve (not vivid magenta)

- [ ] **Step 4: Commit**

```bash
git add resources/scss/_login.scss resources/css/altinf.css
git commit -m "style: unify login page button and field styles via mixins"
```

---

## Task 6: Update `_blog.scss`

**Files:**
- Modify: `resources/scss/_blog.scss`

- [ ] **Step 1: Replace the file contents**

```scss
@use 'buttons';
@use 'forms';

.blog-page {
  h1 {
    font-size:      2rem;
    margin-bottom:  1.5rem;
    border-bottom:  1px solid var(--color-border);
    padding-bottom: 0.5rem;
  }
}

.post-list { display: flex; flex-direction: column; gap: 1.5rem; }

.post-item {
  padding:       1rem;
  background:    var(--color-surface);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
}

.post-title {
  font-size:   1.1rem;
  font-weight: 600;
}

.post-date {
  font-size:   0.82rem;
  color:       var(--color-muted);
  margin-left: 0.5rem;
}

.tag-row { display: flex; flex-wrap: wrap; gap: 0.4rem; margin-top: 0.5rem; }

.tag-chip {
  display:       inline-block;
  padding:       0.15rem 0.55rem;
  font-size:     0.75rem;
  background:    var(--color-bg);
  color:         var(--color-accent);
  border:        1px solid var(--color-accent);
  border-radius: 999px;
  cursor:        pointer;
  transition:    background 0.15s, color 0.15s;
  font-family:   var(--font-sans);

  &:hover,
  &:focus {
    background:      var(--color-accent);
    color:           var(--color-bg);
    text-decoration: none;
  }
}

.clear-chip {
  border-color: var(--color-muted);
  color:        var(--color-muted);

  &:hover { background: var(--color-muted); color: var(--color-bg); }
}

.filter-bar {
  display:       flex;
  align-items:   center;
  gap:           0.75rem;
  margin-bottom: 1rem;
  font-size:     0.85rem;
  color:         var(--color-muted);
}

.post-header {
  margin-bottom:  2rem;
  padding-bottom: 1rem;
  border-bottom:  1px solid var(--color-border);

  h1 { font-size: 2rem; margin-bottom: 0.4rem; }
}

.post-content {
  max-width:   var(--max-width);
  line-height: 1.8;

  h1, h2, h3 {
    margin:      1.5rem 0 0.6rem;
    line-height: 1.3;
  }

  p          { margin-bottom: 1rem; }
  pre        { background: var(--color-surface); padding: 1rem; border-radius: var(--radius); overflow-x: auto; margin-bottom: 1rem; }
  code       { font-family: var(--font-mono); font-size: 0.88em; }
  ul, ol     { padding-left: 1.5rem; margin-bottom: 1rem; }

  blockquote {
    border-left:   3px solid var(--color-accent);
    padding-left:  1rem;
    color:         var(--color-muted);
    margin-bottom: 1rem;
  }
}

.post-editor-page { padding-top: 2rem; }

.post-editor-form {
  background:     var(--color-surface);
  border:         1px solid var(--color-border);
  border-radius:  var(--radius);
  padding:        2rem;
  display:        flex;
  flex-direction: column;
  gap:            0.85rem;

  h2 { font-size: 1.25rem; }
}

.editor-field { @include forms.form-input; }

.editor-tab-bar { display: flex; gap: 0; margin-bottom: -1px; }

.editor-tab {
  @include buttons.btn-ghost;
  @include buttons.btn-sm;
  border-bottom: none;

  & + & { margin-left: 2px; }
}

.editor-tab-active {
  background:   var(--color-bg);
  color:        var(--color-text);
  border-color: var(--color-accent);
}

.editor-textarea {
  @include forms.form-textarea;
  min-height:    420px;
  padding:       0.75rem;
  line-height:   1.6;
  border:        1px solid var(--color-accent);
  border-radius: 0 var(--radius) var(--radius) var(--radius);
}

.editor-preview {
  min-height:    420px;
  padding:       0.75rem;
  background:    var(--color-bg);
  border:        1px solid var(--color-accent);
  border-radius: 0 var(--radius) var(--radius) var(--radius);
}

.editor-btn-row { display: flex; gap: 0.75rem; margin-top: 2rem; }

.editor-btn        { @include buttons.btn-primary; }
.editor-btn-cancel { @include buttons.btn-ghost; }

.editor-status { font-size: 0.85rem; color: var(--color-error); min-height: 1.2em; }

.post-edit-link {
  font-size:   0.8rem;
  color:       var(--color-muted);
  margin-left: 0.75rem;

  &:hover { color: var(--color-text); text-decoration: none; }
}
```

- [ ] **Step 2: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0.

- [ ] **Step 3: Visual check**

Build and navigate to a blog post editor. Verify:
- "Save" button is muted mauve fill, square corners
- "Cancel" button is transparent with border, square corners
- The title/content input fields are styled consistently with login fields
- The textarea and preview panel retain their accent-colored border
- The "Write"/"Preview" tab switcher buttons look correct (ghost-sm style, no bottom border)

- [ ] **Step 4: Commit**

```bash
git add resources/scss/_blog.scss resources/css/altinf.css
git commit -m "style: unify blog editor buttons and fields via mixins"
```

---

## Task 7: Update `_kanban.scss`

This file has the most changes: four button classes, one form-select, the `editor-btn-danger`, the hardcoded `border-radius: 4px` on calendar cells, and three section headings. Take it one section at a time.

**Files:**
- Modify: `resources/scss/_kanban.scss`

- [ ] **Step 1: Add `@use` declarations at the top of `_kanban.scss`**

Add these two lines as the very first lines of the file, before all existing rules:

```scss
@use 'buttons';
@use 'forms';
```

- [ ] **Step 2: Replace Gantt button rules**

Find and replace the `.gv-btn` and `.gv-btn--today` blocks:

```scss
// Before:
.gv-btn {
  padding:       0.35rem 0.85rem;
  background:    transparent;
  color:         var(--color-text);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  font-size:     0.85rem;
  font-family:   var(--font-sans);
  cursor:        pointer;
  transition:    background 0.15s, color 0.15s;

  &:hover:not(:disabled) {
    background: var(--color-surface);
  }

  &:disabled {
    opacity: 0.35;
    cursor:  default;
  }
}

.gv-btn--today {
  background:   var(--color-accent);
  color:        var(--color-bg);
  border-color: var(--color-accent);

  &:hover:not(:disabled) {
    background: var(--color-accent-h);
    color:      var(--color-bg);
  }
}
```

```scss
// After:
.gv-btn {
  @include buttons.btn-ghost;
  @include buttons.btn-sm;
}

.gv-btn--today {
  @include buttons.btn-primary;
  @include buttons.btn-sm;
}
```

- [ ] **Step 3: Replace `.gv-range-select` rules**

```scss
// Before:
.gv-range-select {
  background:    var(--color-surface);
  color:         var(--color-text);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  padding:       0.25rem 0.5rem;
  font-size:     0.85rem;
  font-family:   var(--font-sans);
  cursor:        pointer;
}
```

```scss
// After:
.gv-range-select {
  @include forms.form-select;
  padding: 0.25rem 0.5rem;
}
```

- [ ] **Step 4: Replace `.kb-date-clear` rules**

```scss
// Before:
.kb-date-clear {
  flex-shrink:   0;
  padding:       0.4rem 0.7rem;
  font-size:     0.78rem;
  background:    transparent;
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  color:         var(--color-muted);
  cursor:        pointer;
  white-space:   nowrap;

  &:hover {
    color:        var(--color-text);
    border-color: var(--color-accent);
  }
}
```

```scss
// After:
.kb-date-clear {
  @include buttons.btn-ghost;
  @include buttons.btn-xs;
  flex-shrink: 0;
  white-space: nowrap;

  &:hover { border-color: var(--color-accent); }
}
```

- [ ] **Step 5: Replace `.editor-btn-danger` rules**

```scss
// Before:
.editor-btn-danger {
  background:   #7a2424;
  border-color: #a33;

  &:hover { background: #922; }
}
```

```scss
// After:
.editor-btn-danger { @include buttons.btn-danger; }
```

- [ ] **Step 6: Replace `.kb-comment-actions button` rules**

```scss
// Before:
.kb-comment-actions {
  display:     flex;
  gap:         0.5rem;
  margin-top:  0.5rem;

  button {
    font-size:     0.72rem;
    padding:       0.2rem 0.5rem;
    background:    transparent;
    border:        1px solid var(--color-border);
    border-radius: var(--radius);
    color:         var(--color-muted);
    cursor:        pointer;

    &:hover { color: var(--color-text); border-color: var(--color-accent); }
  }

  .kb-comment-del-btn:hover {
    color:        var(--color-error);
    border-color: var(--color-error);
  }
}
```

```scss
// After:
.kb-comment-actions {
  display:    flex;
  gap:        0.5rem;
  margin-top: 0.5rem;

  button {
    @include buttons.btn-ghost;
    @include buttons.btn-xs;

    &:hover { border-color: var(--color-accent); }
  }

  .kb-comment-del-btn:hover {
    color:        var(--color-error);
    border-color: var(--color-error);
  }
}
```

- [ ] **Step 7: Fix hardcoded `border-radius` on calendar cells**

In the `.Wt-calendar` block (in `_wt.scss`, NOT `_kanban.scss` — see Task 9). 

In `_kanban.scss`, find the `.kb-tab` block and apply mixins:

```scss
// Before:
.kb-tab {
  padding:         0.3rem 0.8rem;
  border:          1px solid var(--color-border);
  border-radius:   var(--radius);
  font-size:       0.85rem;
  color:           var(--color-muted);
  text-decoration: none;

  &:hover { color: var(--color-text); text-decoration: none; }

  &--active {
    background:    var(--color-surface);
    color:         var(--color-text);
    border-color:  var(--color-accent);
  }
}
```

```scss
// After:
.kb-tab {
  @include buttons.btn-ghost;
  @include buttons.btn-sm;
  text-decoration: none;

  &--active {
    background:   var(--color-surface);
    color:        var(--color-text);
    border-color: var(--color-accent);
  }
}
```

- [ ] **Step 8: Apply section-heading mixin to the three `h2` blocks**

Three places in `_kanban.scss` have identical h2 rules. Each looks like:

```scss
// Before (in .kb-editor-form, .kb-team-form, .kb-comment-section):
h2 {
  font-size:      0.8rem;
  color:          var(--color-muted);
  text-transform: uppercase;
  letter-spacing: 0.06em;
  border-bottom:  1px solid var(--color-border);
  padding-bottom: 0.3rem;
  margin-top:     0.5rem;
}
```

```scss
// After:
h2 {
  @include forms.section-heading;
  margin-top: 0.5rem;
}
```

Apply this replacement in all three blocks: `.kb-editor-form`, `.kb-team-form`, and `.kb-comment-section`.

- [ ] **Step 9: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0.

- [ ] **Step 10: Visual check**

Build and navigate to the kanban board. Verify:
- "Today" Gantt button is filled muted mauve, square
- Nav/prev/next Gantt buttons are ghost (transparent + border), square
- Date clear button ("×") in the task editor is a small ghost button
- Comment edit/delete buttons are small ghost buttons
- The Kanban/Gantt tab switcher looks correct
- Section headings in task editor, team form, comment section are uppercase muted text with bottom border

- [ ] **Step 11: Commit**

```bash
git add resources/scss/_kanban.scss resources/css/altinf.css
git commit -m "style: unify kanban/gantt buttons and form elements via mixins"
```

---

## Task 8: Update `_links.scss`

**Files:**
- Modify: `resources/scss/_links.scss`

- [ ] **Step 1: Add `@use` and replace button rules**

Add `@use 'buttons';` at the top. Then replace `.link-action-btn` and `.link-delete-btn`:

```scss
// Before:
.link-action-btn {
  padding:       0.2rem 0.65rem;
  font-size:     0.78rem;
  font-family:   var(--font-sans);
  background:    transparent;
  color:         var(--color-muted);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  cursor:        pointer;
  transition:    color 0.15s, background 0.15s;

  &:hover { color: var(--color-text); background: var(--color-surface); }
}

.link-delete-btn {
  color:        var(--color-error);
  border-color: #e06c6c33;

  &:hover { background: #e06c6c22; color: var(--color-error); }
}
```

```scss
// After:
.link-action-btn {
  @include buttons.btn-ghost;
  @include buttons.btn-xs;
}

.link-delete-btn {
  @include buttons.btn-ghost-danger;
  @include buttons.btn-xs;
}
```

The full `_links.scss` after edits:

```scss
@use 'buttons';

.links-page {
  h1 {
    font-size:      2rem;
    margin-bottom:  1.5rem;
    border-bottom:  1px solid var(--color-border);
    padding-bottom: 0.5rem;
  }
}

.link-add-btn { margin-bottom: 1.5rem; }

.link-section {
  margin-bottom: 2.5rem;

  h2 {
    font-size:      0.8rem;
    margin-bottom:  1rem;
    color:          var(--color-muted);
    text-transform: uppercase;
    letter-spacing: 0.07em;
    border-bottom:  1px solid var(--color-border);
    padding-bottom: 0.3rem;
  }
}

.link-list {
  display:        flex;
  flex-direction: column;
  gap:            0.75rem;
}

.link-item {
  padding:       0.85rem 1rem;
  background:    var(--color-surface);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
}

.link-title {
  font-size:   1rem;
  font-weight: 600;
  display:     block;
}

.link-desc {
  display:    block;
  font-size:  0.88rem;
  color:      var(--color-muted);
  margin-top: 0.25rem;
}

.links-empty { color: var(--color-muted); margin-top: 1rem; }

.link-ctrl {
  display:     flex;
  align-items: center;
  gap:         0.6rem;
  margin-top:  0.5rem;
}

.link-action-link {
  font-size: 0.78rem;
  color:     var(--color-muted);

  &:hover { color: var(--color-text); text-decoration: none; }
}

.link-action-btn {
  @include buttons.btn-ghost;
  @include buttons.btn-xs;
}

.link-delete-btn {
  @include buttons.btn-ghost-danger;
  @include buttons.btn-xs;
}

.link-confirm {
  display:     flex;
  align-items: center;
  gap:         0.5rem;
  font-size:   0.82rem;
  color:       var(--color-muted);
}

.link-editor-page { padding-top: 2rem; }

.link-desc-field {
  min-height: 80px;
  resize:     vertical;
}
```

- [ ] **Step 2: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0.

- [ ] **Step 3: Visual check**

Navigate to the Links page. Verify:
- Edit and Delete buttons are small, ghost-style with square corners
- Delete button shows error color (not muted grey)
- Link items (cards) have square corners

- [ ] **Step 4: Commit**

```bash
git add resources/scss/_links.scss resources/css/altinf.css
git commit -m "style: unify links page button styles via mixins"
```

---

## Task 9: Update `_wt.scss` and `_org.scss`

**Files:**
- Modify: `resources/scss/_wt.scss`
- Modify: `resources/scss/_org.scss`

- [ ] **Step 1: Update `_wt.scss`**

Add `@use 'forms';` at the top. Replace the `.Wt-calendar select` block and fix the hardcoded `border-radius: 4px` on `td`:

```scss
// Before:
.Wt-calendar {
  background-color: var(--color-bg);
  
  select {
    background-color: var(--color-bg);
    color:            var(--color-text);
    border-radius:    var(--radius);
  }

  th { ... }

  td {
    border-radius: 4px;    // ← hardcoded
    font-family:   var(--font-mono);

    &:hover { background-color: var(--color-border); }
  }

  .Wt-cal-oom { color: var(--color-muted); }
}
```

```scss
// After:
.Wt-calendar {
  background-color: var(--color-bg);
  
  select {
    @include forms.form-select;
    background: var(--color-bg);  // calendar bg, not surface
  }

  th {
    padding-left:  0.3rem;
    padding-right: 0.3rem;
    font-family:   var(--font-mono);
  }

  td {
    border-radius: var(--radius);  // was hardcoded 4px
    font-family:   var(--font-mono);

    &:hover { background-color: var(--color-border); }
  }

  .Wt-cal-oom { color: var(--color-muted); }
}
```

The complete replacement for `_wt.scss`:

```scss
@use 'forms';

.Wt-popup {
  background: var(--color-bg);
}

.Wt-dialogcover {
  position:   fixed;
  left:       0;
  top:        0;
  width:      100%;
  height:     100%;
  background: rgba(0, 0, 0, 0.55);
}

.Wt-dialog {
  background:    var(--color-surface);
  border:        1px solid var(--color-border);
  border-radius: var(--radius);
  color:         var(--color-text);
  min-width:     320px;
  max-width:     480px;

  .titlebar {
    padding:       0.7rem 1rem;
    background:    var(--color-bg);
    border-bottom: 1px solid var(--color-border);
    border-radius: var(--radius) var(--radius) 0 0;
    font-weight:   600;
    font-size:     0.95rem;
  }

  .body {
    padding: 1.1rem 1rem 0.85rem;
  }

  .footer {
    display:         flex;
    justify-content: flex-end;
    gap:             0.5rem;
    padding:         0.75rem 1rem;
    border-top:      1px solid var(--color-border);
  }
}

.Wt-dateedit.editor-field {
  background-image:    url("data:image/svg+xml;charset=utf-8,%3Csvg xmlns='http://www.w3.org/2000/svg' width='1em' height='1em' viewBox='0 0 16 16' fill='%236c757d'%3E%3Cpath fill-rule='evenodd' d='M3.5 0a.5.5 0 0 1 .5.5V1h8V.5a.5.5 0 0 1 1 0V1h1a2 2 0 0 1 2 2v11a2 2 0 0 1-2 2H2a2 2 0 0 1-2-2V3a2 2 0 0 1 2-2h1V.5a.5.5 0 0 1 .5-.5zM1 4v10a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V4H1z'/%3E%3Cpath d='M11 6.5a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1zm-3 0a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1zm-5 3a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1zm3 0a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1z'/%3E%3C/svg%3E");
  background-position: right .73em center;
  background-repeat:   no-repeat;
  background-size:     1.125em 1.125em;

  &.hover {
    background-image: url("data:image/svg+xml;charset=utf-8,%3Csvg xmlns='http://www.w3.org/2000/svg' width='1em' height='1em' viewBox='0 0 16 16' fill='%23343a40'%3E%3Cpath fill-rule='evenodd' d='M3.5 0a.5.5 0 0 1 .5.5V1h8V.5a.5.5 0 0 1 1 0V1h1a2 2 0 0 1 2 2v11a2 2 0 0 1-2 2H2a2 2 0 0 1-2-2V3a2 2 0 0 1 2-2h1V.5a.5.5 0 0 1 .5-.5zM1 4v10a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V4H1z'/%3E%3Cpath d='M11 6.5a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1zm-3 0a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1zm-5 3a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1zm3 0a.5.5 0 0 1 .5-.5h1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-1a.5.5 0 0 1-.5-.5v-1z'/%3E%3C/svg%3E");
    cursor:           default;
  }
}

.Wt-calendar {
  background-color: var(--color-bg);

  select {
    @include forms.form-select;
    background: var(--color-bg);
  }

  th {
    padding-left:  0.3rem;
    padding-right: 0.3rem;
    font-family:   var(--font-mono);
  }

  td {
    border-radius: var(--radius);
    font-family:   var(--font-mono);

    &:hover { background-color: var(--color-border); }
  }

  .Wt-cal-oom { color: var(--color-muted); }
}

.Wt-dialog.kb-task-popup {
  min-width: 560px;
  max-width: 720px;

  .body {
    overflow-y: auto;
    max-height: calc(85vh - 130px);
    padding:    1rem;
  }
}
```

- [ ] **Step 2: Update `_org.scss`**

Add `@use 'forms';` at the top. Apply `section-heading` mixin to `.org-landing-page h2`:

```scss
// Before:
.org-landing-page {
  h1 { font-size: 2rem; margin-bottom: 0.5rem; }
  h2 {
    font-size:      0.78rem;
    font-weight:    700;
    text-transform: uppercase;
    letter-spacing: 0.07em;
    color:          var(--color-muted);
    border-bottom:  1px solid var(--color-border);
    padding-bottom: 0.3rem;
    margin-top:     1.75rem;
    margin-bottom:  0.75rem;
  }
}
```

```scss
// After:
.org-landing-page {
  h1 { font-size: 2rem; margin-bottom: 0.5rem; }
  h2 {
    @include forms.section-heading;
    font-size:      0.78rem;
    font-weight:    700;
    letter-spacing: 0.07em;
    margin-top:     1.75rem;
    margin-bottom:  0.75rem;
  }
}
```

Add `@use 'forms';` as the very first line of `_org.scss`.

- [ ] **Step 3: Compile**

```bash
sass --no-source-map resources/scss/altinf.scss resources/css/altinf.css
```

Expected: no output, exit 0.

- [ ] **Step 4: Visual check**

Build and navigate to a task with a date field (opens the Wt date picker). Verify:
- The calendar popup's select dropdowns (month/year) are styled consistently
- Calendar day cells have no rounded corners on hover highlight
- Navigate to the org landing page and verify section headings are uppercase muted with border-bottom

- [ ] **Step 5: Commit**

```bash
git add resources/scss/_wt.scss resources/scss/_org.scss resources/css/altinf.css
git commit -m "style: unify wt calendar select and org section heading via mixins"
```

---

## Task 10: Final integration build and verify

- [ ] **Step 1: Full clean build**

```bash
cmake --build build --parallel $(nproc)
```

Expected: build succeeds, no SCSS errors in output.

- [ ] **Step 2: Cross-page visual sweep**

Start the server. Visit each of these pages and confirm no regressions:

| Page | What to check |
|---|---|
| Login | Square input/button, muted mauve button |
| Blog list | Tag pills still round, post cards square |
| Blog post editor | Write/Preview tabs, Save/Cancel buttons, textarea |
| Links | Edit/Delete buttons styled correctly |
| Kanban board | Column cards square, count badges round, drag-over accent border |
| Task editor | Date clear button, Gantt Today button, section headings |
| Notifications | Unread left-accent border, bell badge still round |
| Org landing | Section headings |
| Account manager | Table cells, token section card |
| Date picker popup | Calendar select dropdowns, day cell hover |

- [ ] **Step 3: Final commit**

```bash
git add resources/css/altinf.css
git commit -m "style: final integration build — scss unification complete"
```
