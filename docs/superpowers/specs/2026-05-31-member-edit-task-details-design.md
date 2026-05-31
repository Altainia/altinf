# Lead-configurable "members can edit task details"

**Date:** 2026-05-31
**Status:** Approved (design)

## Problem

A team lead should be able to control, per team, whether non-lead members can
edit a task's details (title, description, start/end dates, type). Today this is
governed by the static `team_cap::edit_task_details` capability, which commit
`aab6878` baked into `team_member_caps` — so every member can edit details with
no way for a lead to restrict it. (That commit also left the `team_cap` unit
test asserting the opposite, so the suite has a pre-existing failure.)

## Decisions

- **Default:** members *allowed* to edit details. The new per-team setting
  defaults to `true`/`1`, preserving current live behavior for existing teams.
- **Done status:** changing a task's status to/from "Done" stays **leads-only**,
  regardless of the new toggle. The toggle covers descriptive fields only.
- **Granularity:** one per-team toggle for all four descriptive fields together.
  No per-member overrides and no per-field toggles (YAGNI).

## Approach

Model the toggle exactly like the existing `allow_member_move_columns` team
setting: a per-team DB flag resolved at the use-site by combining a base
capability with the setting. This keeps `team_cap::edit_task_details` meaning
strictly "is a lead" and confines the new policy to a single setting.

## Changes

### 1. Capability — `src/org/team_cap.hpp`

- Remove `edit_task_details` from `team_member_caps` (leads retain it via
  `team_lead_caps`). Remove the now-duplicate `edit_task_details` term in the
  `team_lead_caps` initializer.
- Effect: `caps.has_any(team_cap::edit_task_details)` once again means "is a
  lead." The existing assertion
  `CHECK(!team_member_caps.has_any(edit_task_details))`
  ([tests/test_team_cap.cpp:67](../../../tests/test_team_cap.cpp)) passes with no
  edit.

### 2. Data layer — `src/org/kanban.hpp`, `src/org/kanban_db.cpp`

- `team_settings_record`: add `int allow_member_edit_details{1}` + its
  `Wt::Dbo::field(a, allow_member_edit_details, "allow_member_edit_details")`.
- `team_settings_entry`: add `bool allow_member_edit_details{true}`.
- Schema: add `allow_member_edit_details integer not null default 1` to the
  `CREATE TABLE team_settings` statement, and add an idempotent migration:
  `migrate("ALTER TABLE team_settings ADD COLUMN allow_member_edit_details integer not null default 1")`.
- `settings_for_team`: map the new column in both the team-specific and the
  org-default branches.
- `set_team_settings`: persist the new field and record an audit change in both
  the new-row and existing-row branches (mirroring the existing four fields;
  new-row default value is `"1"`).

### 3. Editor gating — `src/org/widgets/task_editor_form_widget.cpp`

- Introduce a local:
  `const bool can_edit_details = is_lead || (caps.has_any(team_cap::edit_task_fields) && m_settings.allow_member_edit_details);`
  where `is_lead == caps.has_any(team_cap::edit_task_details)` (now leads-only).
- Drive the descriptive fields (title, description, start/end date, type) off
  `can_edit_details` instead of the lead-only `can_edit` (~line 101).
- Gate the field-revert guard in `save()` (~line 997) on `!can_edit_details`
  instead of the raw cap, so allowed members' edits persist.
- Leave the to/from-Done restriction (~lines 969–980) gated on actual lead
  status — unchanged.
- The "can save at all" guard (~lines 939–944) is unaffected: members retain
  `edit_task_fields`.

### 4. Board page — `src/org/pages/team_kanban_page.cpp`

- Expected no change: `is_lead`/`can_move_done` keep their lead-only meaning, and
  detail editing happens only in the task editor. Verify during implementation.

### 5. Settings UI — `src/org/pages/team_settings_page.cpp`

- Add a "Members can edit task details" checkbox to the existing "Member
  permissions" section, initialized from `settings.allow_member_edit_details`,
  with a `changed()` handler that loads settings, sets the field,
  `set_team_settings`, and broadcasts `team:<id>` — identical in shape to the
  existing four checkboxes.

## Testing

- **Catch2:** the existing `team_cap` test goes green. Add a `kanban_db` test
  that round-trips `allow_member_edit_details` through `set_team_settings` /
  `settings_for_team` and confirms an audit event is recorded.
- **JS unit:** unaffected.
- **E2E (Playwright):** new test modeled on `task-permissions.spec.ts` — a lead
  turns the toggle off; a non-lead member opens a task and finds the title field
  non-editable and edits do not persist; the lead turns it on and the member can
  edit. Verify the Done-status restriction still holds for members in both
  states.

## Out of scope

Per-member overrides, per-field toggles, retroactive effects on existing tasks.
