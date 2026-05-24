# SCSS Unification — Design Spec

**Date:** 2026-05-24
**Branch:** refactoring
**Scope:** Stylesheet consolidation — button system, form elements, accent color, corner rounding

---

## Goals

1. Unify fragmented button and form styles into single authoritative files.
2. Remove rounded corners from surfaces and interactive elements (preserve circular badges/pills).
3. Mute the magenta accent color.

---

## 1. Variables (`_variables.scss`)

### Accent color

| Token | Old value | New value |
|---|---|---|
| `'accent'` | `#d279d2` | `#a673a6` |
| `'accent-h'` | `#db94db` | `#b58cb5` |

Both values feed through the existing `contrast.ensure-contrast` calls, which auto-adjust toward WCAG compliance. No other contrast machinery changes.

### Corner radius

```scss
--radius: 0;   // was 4px
```

Every `var(--radius)` reference across all files automatically becomes sharp. One hardcoded exception to fix manually: `.Wt-calendar td { border-radius: 4px }` → `border-radius: var(--radius)`.

### Preserved circular/pill radii

These five values are **not changed**:

| Value | Used by |
|---|---|
| `50%` | `.org-type-dot`, `.kb-type-chip__dot` (color indicator circles) |
| `999px` | `.tag-chip` (blog tag pills) |
| `14px` | `.kb-type-chip` (type selector chips) |
| `10px` | `.kb-col-count` (kanban column count badge) |
| `8px` | `.nav-bell-badge` (notification bell badge) |

---

## 2. Button system (`_buttons.scss`, new file)

All button rules move into a single new partial. The file defines SCSS mixins; every existing class in the component files becomes a one-line `@include`. C++ class names are preserved — no server-side changes needed.

### Base mixin

`btn-base` is included by every role variant. Contains:
- `border-radius: 0`
- `cursor: pointer`
- `font-family: var(--font-sans)`
- `transition: background 0.15s, color 0.15s`
- `&:disabled { opacity: 0.35; cursor: default; }` — universal disabled state

### Role variants

| Mixin | Description | Applied to |
|---|---|---|
| `btn-primary` | Accent fill, dark text | `.login-btn`, `.editor-btn`, `.gv-btn--today` |
| `btn-ghost` | Transparent bg, muted text, border | `.gv-btn`, `.editor-btn-cancel`, `.link-action-btn`, `.kb-date-clear`, `.kb-comment-actions button` |
| `btn-danger` | Dark red fill, light text | `.editor-btn-danger` |
| `btn-ghost-danger` | Ghost base, error-colored text and border | `.link-delete-btn` |

### Size variants (compose with any role)

| Mixin | Font size | Padding | Applied to |
|---|---|---|---|
| *(default)* | `0.95rem` | `0.5rem 1rem` | `.login-btn`, `.editor-btn` |
| `btn-sm` | `0.85rem` | `0.35rem 0.85rem` | `.gv-btn`, `.gv-btn--today` |
| `btn-xs` | `0.78rem` | `0.2rem 0.65rem` | `.link-action-btn`, `.kb-date-clear`, `.kb-comment-actions button` |

### Tab-style buttons (`.kb-tab`, `.editor-tab`)

These use `btn-ghost-sm` as their base but keep their own active-state rules locally in `_kanban.scss` and `_blog.scss`. They are view switchers, not actions, and their selected/active highlight logic is component-specific.

---

## 3. Form elements (`_forms.scss`, new file)

Three input mixins and one typography mixin.

### `form-input`

The shared text input style. `.login-field` (`_login.scss`) and `.editor-field` (`_blog.scss`) are currently identical rules duplicated verbatim. Both become `@include forms.form-input`. Covers: background (`--color-bg`), color, border, font size/family, padding, and focus ring (`border-color: var(--color-accent); outline: none`).

### `form-select`

For `<select>` elements. `.gv-range-select` (`_kanban.scss`) and `.Wt-calendar select` (`_wt.scss`) share the same border, font, and color pattern. The mixin captures shared rules; each class retains its own padding override.

### `form-textarea`

For `<textarea>` elements. `.editor-textarea`, `.kb-comment-compose textarea`, and `.kb-comment-edit-area textarea` share background, border, mono font, and `resize: vertical`. Each class keeps its own `min-height`.

### `section-heading`

The `h2` pattern — uppercase, `letter-spacing`, muted color, border-bottom — appears six times across `_kanban.scss`, `_blog.scss`, `_org.scss`, and `_links.scss`. Component files keep their `h2` selectors locally but replace the repeated declarations with `@include forms.section-heading`.

---

## 4. Import order (`altinf.scss`)

New partials inserted after `base`, before component files:

```scss
@use 'variables';
@use 'base';
@use 'buttons';   // new
@use 'forms';     // new
@use 'wt';
@use 'layout';
@use 'login';
@use 'blog';
@use 'links';
@use 'kanban';
@use 'accounts';
@use 'org';
```

---

## 5. Per-file changes

### `_login.scss`
- `.login-btn` body → `@include buttons.btn-primary`
- `.login-field` body → `@include forms.form-input`

### `_blog.scss`
- `.editor-btn` body → `@include buttons.btn-primary`
- `.editor-btn-cancel` body → `@include buttons.btn-ghost`
- `.editor-field` body → `@include forms.form-input`
- `.editor-textarea` body → `@include forms.form-textarea` (retains `min-height: 420px`)
- Six `h2` section heading blocks → `@include forms.section-heading`

### `_kanban.scss`
- `.editor-btn-danger` body → `@include buttons.btn-danger`
- `.gv-btn` body → `@include buttons.btn-ghost` + `@include buttons.btn-sm`
- `.gv-btn--today` body → `@include buttons.btn-primary` + `@include buttons.btn-sm`
- `.kb-date-clear` body → `@include buttons.btn-ghost` + `@include buttons.btn-xs`
- `.kb-comment-actions button` body → `@include buttons.btn-ghost` + `@include buttons.btn-xs`
- `.gv-range-select` body → `@include forms.form-select`
- `.Wt-calendar td` `border-radius: 4px` → `border-radius: var(--radius)`
- Section heading `h2` blocks → `@include forms.section-heading`

### `_links.scss`
- `.link-action-btn` body → `@include buttons.btn-ghost` + `@include buttons.btn-xs`
- `.link-delete-btn` body → `@include buttons.btn-ghost-danger` + `@include buttons.btn-xs`

### `_wt.scss`
- `.Wt-calendar select` body → `@include forms.form-select`

### `_accounts.scss`
- No changes (`.account-new-btn` is positioning only).

### `_org.scss`
- No changes (no button rules).

---

## Non-goals

- No C++ server-side class name changes.
- No changes to layout, Kanban card, or navigation styles beyond what is listed above.
- No new visual features or component additions.
