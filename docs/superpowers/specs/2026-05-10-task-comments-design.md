# Task Comments — Design Spec

**Date:** 2026-05-10
**Status:** Approved

---

## Overview

Allow any team member who can view a board to post comments on tasks. Comments appear as a section at the bottom of the Details tab in the task editor. They support Markdown, soft edits, soft deletes, and live real-time updates to all connected viewers of the same task.

---

## 1. Data Layer

Two new tables, mirroring the existing `task_event` / `task_field_change` pattern.

### `task_comment`

Current state of each comment.

| Column | Type | Notes |
|---|---|---|
| `id` | INTEGER PK | auto |
| `task_id` | INTEGER | FK to `kanban_task` |
| `author` | TEXT | username of creator |
| `body` | TEXT | current body text |
| `created_at` | TEXT | ISO-8601 UTC |
| `is_deleted` | INTEGER | 0/1 soft-delete flag |

### `task_comment_event`

Immutable audit log, one row per action.

| Column | Type | Notes |
|---|---|---|
| `id` | INTEGER PK | auto |
| `comment_id` | INTEGER | FK to `task_comment` |
| `actor` | TEXT | username performing the action |
| `occurred_at` | TEXT | ISO-8601 UTC |
| `event_type` | TEXT | `"created"` \| `"edited"` \| `"deleted"` |
| `body_snapshot` | TEXT | body at the time of this event |

`body_snapshot` on a `created` event is the initial text; on `edited` it is the new text; on `deleted` it is the final body before deletion. This gives a complete replayable history.

### `task_comment_entry` (C++ struct)

Used for inter-layer communication. Populated by joining `task_comment` with its events.

```cpp
struct task_comment_entry {
    long long id;
    long long task_id;
    std::string author;
    std::string body;           // current body; empty string if deleted
    std::string created_at;
    bool is_deleted;
    std::string last_edited_by; // empty if never edited
    std::string last_edited_at; // empty if never edited
    std::string deleted_by;     // empty if not deleted
    std::string deleted_at;     // empty if not deleted
};
```

---

## 2. Database Access Layer

Four new methods on `kanban_db`.

### `long long add_comment(long long task_id, const std::string& author, const std::string& body)`

Inserts a `task_comment` row and a `task_comment_event` row with `event_type="created"` and `body_snapshot=body`. Returns the new comment id.

### `void edit_comment(long long comment_id, const std::string& editor, const std::string& new_body)`

Updates `task_comment.body` in place. Appends a `task_comment_event` row with `event_type="edited"` and `body_snapshot=new_body`. No-ops if the comment is already deleted.

### `void delete_comment(long long comment_id, const std::string& actor)`

Sets `task_comment.is_deleted=1`. Appends a `task_comment_event` row with `event_type="deleted"` and `body_snapshot` set to the body at time of deletion. No-ops if already deleted.

### `std::vector<task_comment_entry> comments_for_task(long long task_id)`

Returns all comments for a task in chronological order (including soft-deleted ones, so placeholders can be rendered). Each entry is populated with:

- `last_edited_by` / `last_edited_at` from the most recent `edited` event
- `deleted_by` / `deleted_at` from the `deleted` event

Deleted comment bodies are cleared to an empty string at this layer so content never reaches the UI.

### Permissions

Checked in the page layer (consistent with existing patterns):

| Action | Who |
|---|---|
| View comments | `can_view_board` — any team member |
| Add comment | `can_view_board` — any team member |
| Edit comment | `actor == comment.author` OR `is_team_lead` OR `is_org_lead` |
| Delete comment | `actor == comment.author` OR `is_team_lead` OR `is_org_lead` |

---

## 3. UI

### Placement

A **Comments** section is appended below the existing task fields in the Details tab of `kanban_task_editor_page`.

### Comment List

Rendered oldest-first (chronological). Each non-deleted comment shows:

- Author name and `created_at` timestamp
- Markdown-rendered body (cmark, server-side; output set as trusted HTML)
- "Edited by X at [timestamp]" in muted text below the body, if any edit events exist
- Edit and Delete buttons, visible only to the comment's author and to leads

Deleted comments render as a single muted line with no body and no buttons:

> *Comment deleted by [actor] at [timestamp]*

### Compose Area

Shown below the comment list; hidden for archived tasks.

- A `WTextArea` for the comment body
- A "Post" button, disabled while the textarea is empty
- On submit: clears the textarea and appends the new comment to the list

### Inline Editing

- Clicking Edit replaces the rendered body with a `WTextArea` pre-filled with the raw (unrendered) body text, plus Save and Cancel buttons
- The raw body is stored alongside the rendered HTML in the widget so Edit can populate the textarea without a DB round-trip
- Save calls `edit_comment`; Cancel restores the rendered view

### Confirmation Dialogs

**Delete (all users):** A `WMessageBox` is shown before any deletion.
- If the actor is the author: *"Are you sure you want to delete this comment?"*
- If the actor is a lead acting on another user's comment: *"This comment was written by [author]. Are you sure you want to delete it?"*

**Edit (leads acting on another user's comment only):** A `WMessageBox` is shown before opening the inline editor.
- *"This comment was written by [author]. Are you sure you want to edit it?"*
- Authors editing their own comments receive no confirmation — the inline editor opens directly.

### Live Updates

`kanban_task_editor_page` already subscribes to `live_hub` on `"task:{task_id}"`. On comment add, edit, or delete, the server broadcasts on this same channel. The receiving session re-fetches `comments_for_task` and re-renders the comment section only (not the full page).

### Archived Tasks

The compose area is hidden. Existing comments remain visible read-only.

---

## 4. Testing

### Playwright E2E

- Post a comment as a team member; verify it appears rendered (Markdown smoke test: bold text renders as `<strong>`)
- Edit own comment; verify "Edited by" metadata appears
- Delete own comment; verify deleted placeholder appears
- Lead edits another user's comment: confirm dialog names the author, proceed, verify edit is applied
- Lead deletes another user's comment: confirm dialog names the author, proceed, verify placeholder appears
- Archived task: verify compose area is hidden; existing comments still visible read-only
- **Live update — add:** Two sessions open on the same task; one posts a comment, the other sees it appear without reload
- **Live update — edit:** Two sessions open on the same task; one edits a comment, the other sees the updated body and "Edited by" metadata without reload
- **Live update — delete:** Two sessions open on the same task; one deletes a comment, the other sees the deleted placeholder without reload

### Catch2 (unit)

- `add_comment` → event log contains one `created` entry with correct `body_snapshot`
- `edit_comment` → body updated, event log gains `edited` entry; calling on a deleted comment is a no-op
- `delete_comment` → `is_deleted=1`, event log gains `deleted` entry with body snapshot; calling again is a no-op
- `comments_for_task` → deleted comment has empty body, `deleted_by`/`deleted_at` populated; edited comment has `last_edited_by`/`last_edited_at` from most recent edit event

### JS Unit

No new JS logic is introduced (all rendering is server-side Wt). No new JS unit tests are needed.
