# Member-Edit-Task-Details Toggle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a team lead control, per team, whether non-lead members can edit a task's descriptive details (title, description, start/end dates, type).

**Architecture:** Model the control as a per-team DB setting (`allow_member_edit_details`, default on) resolved at the editor use-site — exactly like the existing `allow_member_move_columns` toggle. `team_cap::edit_task_details` reverts to meaning strictly "is a lead." Changing status to/from Done stays leads-only.

**Tech Stack:** C++20, Wt 4.13 (widgets + Wt::Dbo over SQLite), Catch2 (unit), Playwright (e2e).

**Build/run reference:**
- Build: `cmake --build /home/altainia/code/altinf/build --parallel $(nproc)`
- Catch2 (single binary): `/home/altainia/code/altinf/build/tests/test_team_cap` / `.../test_kanban_db`
- E2E: from `/home/altainia/code/altinf/e2e`, `npx playwright test <spec>` (its `webServer` launches the freshly-built binary).

---

## File Structure

- `src/org/team_cap.hpp` — remove `edit_task_details` from `team_member_caps`; de-duplicate in `team_lead_caps`.
- `src/org/kanban.hpp` — add `allow_member_edit_details` to `team_settings_record` and `team_settings_entry`.
- `src/org/kanban_db.cpp` — schema column + migration; map in `settings_for_team`; persist + audit in `set_team_settings`.
- `src/org/widgets/task_editor_form_widget.hpp` / `.cpp` — `can_edit_details()` helper; gate descriptive fields and the `save()` revert guard on it.
- `src/org/pages/team_settings_page.cpp` — add the "Members can edit task details" checkbox.
- `tests/test_kanban_db.cpp` — round-trip + audit unit test.
- `e2e/specs/member-edit-details.spec.ts` — behavioral e2e test.

---

## Task 1: Capability — leads-only `edit_task_details`

**Files:**
- Modify: `src/org/team_cap.hpp:25-26`
- Test: `tests/test_team_cap.cpp:65-71` (already exists; currently failing)

- [ ] **Step 1: Confirm the existing test fails (red)**

Run: `/home/altainia/code/altinf/build/tests/test_team_cap "team_cap - edit_task_details*"`
Expected: FAIL at `tests/test_team_cap.cpp:67` — `CHECK(!team_member_caps.has_any(edit_task_details))`, because `team_member_caps` currently includes it.

- [ ] **Step 2: Remove the capability from member caps and de-duplicate in lead caps**

In `src/org/team_cap.hpp`, replace lines 25-26:

```cpp
	inline constexpr flags team_member_caps = view_board | edit_task_fields | self_assign | comment | edit_task_details;
	inline constexpr flags team_lead_caps   = team_member_caps | view_archived | reassign_task | create_task | archive_task | manage_team | edit_task_details;
```

with:

```cpp
	inline constexpr flags team_member_caps = view_board | edit_task_fields | self_assign | comment;
	inline constexpr flags team_lead_caps   = team_member_caps | view_archived | reassign_task | create_task | archive_task | manage_team | edit_task_details;
```

(`team_member_caps` loses `edit_task_details`; `team_lead_caps` keeps the single explicit `edit_task_details` term.)

- [ ] **Step 3: Rebuild the test binary**

Run: `cmake --build /home/altainia/code/altinf/build --parallel $(nproc) --target test_team_cap`
Expected: builds clean.

- [ ] **Step 4: Verify the test passes (green)**

Run: `/home/altainia/code/altinf/build/tests/test_team_cap`
Expected: all assertions pass (the `edit_task_details` case included).

- [ ] **Step 5: Commit**

```bash
git add src/org/team_cap.hpp
git commit -m "fix(caps): make edit_task_details leads-only again"
```

---

## Task 2: Data layer — `allow_member_edit_details` setting

**Files:**
- Modify: `src/org/kanban.hpp:132-161`
- Modify: `src/org/kanban_db.cpp` (schema ~73-82; `settings_for_team` ~888-944; `set_team_settings` ~985-1023)
- Test: `tests/test_kanban_db.cpp` (append new TEST_CASE)

- [ ] **Step 1: Write the failing round-trip + audit test**

Append to `tests/test_kanban_db.cpp` (the file already includes `<algorithm>`, `<catch2/catch_test_macros.hpp>`, and `"org/kanban_db.hpp"`):

```cpp
TEST_CASE("kanban_db - allow_member_edit_details round-trips and is audited")
{
	kanban_db       db{":memory:"};
	const long long tid = db.create_team("Engineering", 1);

	// Defaults to true (members allowed) when no row exists.
	CHECK(db.settings_for_team(tid).allow_member_edit_details);

	// Turn it off and persist.
	auto s                        = db.settings_for_team(tid);
	s.org_id                      = 1;
	s.team_id                     = tid;
	s.allow_member_edit_details   = false;
	db.set_team_settings(s, "lead");

	CHECK_FALSE(db.settings_for_team(tid).allow_member_edit_details);

	// The change is recorded in the audit log.
	const auto events  = db.settings_events_for_team(tid);
	const bool audited = std::ranges::any_of(events, [](const auto& e) {
		return e.field_name == "allow_member_edit_details" && e.new_value == "0";
	});
	CHECK(audited);
}
```

- [ ] **Step 2: Run the test to verify it fails (red)**

Run: `cmake --build /home/altainia/code/altinf/build --parallel $(nproc) --target test_kanban_db`
Expected: COMPILE ERROR — `team_settings_entry` has no member `allow_member_edit_details`.

- [ ] **Step 3: Add the field to the record and entry structs**

In `src/org/kanban.hpp`, in `struct team_settings_record` add the field after `allow_abandon` (line 139) and its `persist` mapping after the `allow_abandon` field (line 149):

```cpp
	int       allow_abandon{1};
	int       allow_member_edit_details{1};
```

```cpp
		Wt::Dbo::field(a, allow_abandon, "allow_abandon");
		Wt::Dbo::field(a, allow_member_edit_details, "allow_member_edit_details");
```

In `struct team_settings_entry` add after `allow_abandon` (line 160):

```cpp
	bool      allow_abandon{true};
	bool      allow_member_edit_details{true};
```

- [ ] **Step 4: Add the schema column and migration**

In `src/org/kanban_db.cpp`, change the `CREATE TABLE team_settings` statement (ends at line 82) so the last column line reads:

```cpp
	  " allow_abandon integer not null default 1,"
	  " allow_member_edit_details integer not null default 1)");
```

Immediately after that `migrate(...)` block (after line 82), add an idempotent column migration for pre-existing databases:

```cpp
	migrate("ALTER TABLE team_settings ADD COLUMN allow_member_edit_details integer not null default 1");
```

- [ ] **Step 5: Map the column in `settings_for_team`**

In `src/org/kanban_db.cpp`, in the team-specific branch add after line 907 and in the org-default branch add after line 935:

```cpp
		e.allow_member_edit_details    = r->allow_member_edit_details != 0;
```

(The hard-coded-defaults branch at lines 939-943 needs no change — the entry's member default is already `true`.)

- [ ] **Step 6: Persist + audit the field in `set_team_settings`**

In `src/org/kanban_db.cpp`, in the new-row branch (after line 993) add the audit line, and after line 1001 add the assignment:

```cpp
		record_change("allow_abandon", "1", to_s(s.allow_abandon));
		record_change("allow_member_edit_details", "1", to_s(s.allow_member_edit_details));
```

```cpp
		r.modify()->allow_abandon                = s.allow_abandon ? 1 : 0;
		r.modify()->allow_member_edit_details    = s.allow_member_edit_details ? 1 : 0;
```

In the existing-row branch, add the audit after line 1017 and the assignment after line 1022:

```cpp
		record_change("allow_abandon",
		              to_s(r->allow_abandon != 0),
		              to_s(s.allow_abandon));
		record_change("allow_member_edit_details",
		              to_s(r->allow_member_edit_details != 0),
		              to_s(s.allow_member_edit_details));
```

```cpp
		r.modify()->allow_abandon                = s.allow_abandon ? 1 : 0;
		r.modify()->allow_member_edit_details    = s.allow_member_edit_details ? 1 : 0;
```

- [ ] **Step 7: Run the test to verify it passes (green)**

Run: `cmake --build /home/altainia/code/altinf/build --parallel $(nproc) --target test_kanban_db && /home/altainia/code/altinf/build/tests/test_kanban_db "kanban_db - allow_member_edit_details*"`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add src/org/kanban.hpp src/org/kanban_db.cpp tests/test_kanban_db.cpp
git commit -m "feat(settings): add allow_member_edit_details team setting (default on)"
```

---

## Task 3: Editor gating — `can_edit_details()`

**Files:**
- Modify: `src/org/widgets/task_editor_form_widget.hpp` (private section, near line 52-53)
- Modify: `src/org/widgets/task_editor_form_widget.cpp:99-101` and `:997`

No standalone unit test (Wt widget); behavior is covered by the e2e test in Task 5. This task is verified by a clean build.

- [ ] **Step 1: Declare the helper in the header**

In `src/org/widgets/task_editor_form_widget.hpp`, add to the private section (just below the `m_settings` member at line 53) a declaration:

```cpp
	// True when the current user may edit descriptive task fields (title,
	// description, dates, type): always for leads, and for members when the
	// team setting allows it.
	bool can_edit_details() const;
```

- [ ] **Step 2: Define the helper in the .cpp**

In `src/org/widgets/task_editor_form_widget.cpp`, add the definition immediately above the constructor (before line 68, after the `k_status_labels` definition at line 64):

```cpp
bool task_editor_form_widget::can_edit_details() const
{
	return m_caps.has_any(team_cap::edit_task_details) ||
	       (m_caps.has_any(team_cap::edit_task_fields) &&
	        m_settings.allow_member_edit_details);
}
```

- [ ] **Step 3: Use the helper for descriptive-field editing**

In `src/org/widgets/task_editor_form_widget.cpp`, replace line 101:

```cpp
	const bool can_edit        = is_lead && not_archived;
```

with:

```cpp
	const bool can_edit        = can_edit_details() && not_archived;
```

Leave `is_lead` (line 99) and `can_edit_status` (lines 102-104) unchanged — `can_edit_status` and the Done logic must keep using `is_lead`.

- [ ] **Step 4: Gate the `save()` revert guard on the helper**

In `src/org/widgets/task_editor_form_widget.cpp`, replace line 997:

```cpp
	if(!m_caps.has_any(team_cap::edit_task_details) && !creating)
```

with:

```cpp
	if(!can_edit_details() && !creating)
```

Leave the to/from-Done guard (lines 969-980, keyed on `team_cap::edit_task_details`) unchanged so Done stays leads-only.

- [ ] **Step 5: Build to verify it compiles**

Run: `cmake --build /home/altainia/code/altinf/build --parallel $(nproc)`
Expected: builds clean.

- [ ] **Step 6: Commit**

```bash
git add src/org/widgets/task_editor_form_widget.hpp src/org/widgets/task_editor_form_widget.cpp
git commit -m "feat(editor): gate detail editing on can_edit_details()"
```

---

## Task 4: Settings UI — the checkbox

**Files:**
- Modify: `src/org/pages/team_settings_page.cpp:73-117`

Verified by build + the e2e test in Task 5.

- [ ] **Step 1: Add the checkbox widget and initialize it**

In `src/org/pages/team_settings_page.cpp`, after the `abandon_cb` line (line 76) add:

```cpp
	auto* edit_details_cb = permissions_list->addNew<Wt::WCheckBox>("Members can edit task details");
```

After `abandon_cb->setChecked(settings.allow_abandon);` (line 81) add:

```cpp
	edit_details_cb->setChecked(settings.allow_member_edit_details);
```

In the styling loop (line 83), include the new checkbox:

```cpp
	for(auto* cb: {move_cb, self_un_cb, self_as_cb, abandon_cb, edit_details_cb})
```

- [ ] **Step 2: Wire the change handler**

In `src/org/pages/team_settings_page.cpp`, after the `abandon_cb->changed().connect(...)` block (ends line 117) add:

```cpp
	edit_details_cb->changed().connect(
	  [&db, team_id, actor, edit_details_cb] {
		  auto s                       = db.settings_for_team(team_id);
		  s.allow_member_edit_details  = edit_details_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
```

- [ ] **Step 3: Build to verify it compiles**

Run: `cmake --build /home/altainia/code/altinf/build --parallel $(nproc)`
Expected: builds clean.

- [ ] **Step 4: Commit**

```bash
git add src/org/pages/team_settings_page.cpp
git commit -m "feat(settings-ui): add 'Members can edit task details' checkbox"
```

---

## Task 5: End-to-end behavioral test

**Files:**
- Create: `e2e/specs/member-edit-details.spec.ts`

This is the behavioral test for Tasks 2-4. It mirrors the proven setup in
`e2e/specs/task-permissions.spec.ts`: `createUser` via the Accounts admin page,
org invite + `acceptInvite` via the notification bell, and add-to-team via the
team block's `.gv-range-select`. The shared SQLite DB is reset by global-setup,
so user/org/team creation is unconditional (matching the house style).

**DOM contract being verified** (from `task_editor_form_widget.cpp` title field):
for an existing task, a member who cannot edit details sees a **visible
`readonly`** title input; a member who can edit sees the input hidden behind a
click-to-edit display, i.e. there is **no `readonly` title input**. The
assertions therefore target `input[placeholder="Task title"][readonly]`.

- [ ] **Step 1: Write the spec**

Create `e2e/specs/member-edit-details.spec.ts`:

```ts
import { test, expect, type Page, type Browser } from '@playwright/test';
import { loginAs } from './helpers';

test.describe.configure({ mode: 'serial' });

const ORG  = 'EditOrg';
const TEAM = 'EditTeam';

async function createUser(page: Page, username: string, password: string) {
  await page.locator('.nav-link', { hasText: 'Accounts' }).click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
  await page.locator('.account-new-btn').click();
  await expect(page.locator('.account-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Username (required)"]').fill(username);
  await page.locator('input[placeholder="Password (required)"]').fill(password);
  await page.locator('input[placeholder="Confirm password"]').fill(password);
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel)').click();
  await expect(page.locator('.account-manager-page')).toBeVisible();
}

async function acceptInvite(browser: Browser, username: string, password: string) {
  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, username, password);
  await page.locator('.nav-bell-link').click();
  await expect(page.locator('.notifications-page')).toBeVisible();
  await page.getByRole('button', { name: 'Accept' }).click();
  await expect(page.locator('.nav-bell-badge')).not.toBeVisible();
  await ctx.close();
}

async function gotoTeamBoard(page: Page) {
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).first().click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();
}

test.beforeAll(async ({ browser }) => {
  const page = await browser.newPage();
  await loginAs(page, 'admin', 'testpass');

  // A non-lead member.
  await createUser(page, 'edit_member', 'editpass');

  // Create EditOrg and open its manage page.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('input[placeholder="Organization name"]').fill(ORG);
  await page.locator('.org-create-form .editor-btn').click();
  await expect(page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) })).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  // Invite edit_member to the org and create the team.
  await page.locator('.kb-member-input').fill('edit_member');
  await page.getByRole('button', { name: 'Send invite' }).click();
  await expect(page.locator('.editor-status')).toContainText('Invite sent to edit_member');

  await page.locator('input[placeholder="Team name"]').fill(TEAM);
  await page.getByRole('button', { name: 'Create' }).click();
  await expect(page.locator('.kb-team-block:has(.kb-team-name-label:text-is("' + TEAM + '"))')).toBeVisible();

  // Member accepts the org invite.
  await acceptInvite(browser, 'edit_member', 'editpass');

  // Re-open the manage page (refresh) and add edit_member to the team as a non-lead.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await page.getByRole('link', { name: 'Manage organization' }).click();
  await expect(page.locator('.kb-team-page')).toBeVisible();

  const teamBlock = page.locator('.kb-team-block:has(.kb-team-name-label:text-is("' + TEAM + '"))');
  await teamBlock.locator('.gv-range-select').selectOption('edit_member');
  await teamBlock.getByRole('button', { name: 'Add to team' }).click();
  await expect(teamBlock.locator('.kb-member-row', { hasText: 'edit_member' })).toBeVisible();

  // Navigate to the team board and seed a task.
  await page.locator('.nav-link', { hasText: 'Orgs' }).click();
  await expect(page.locator('.org-admin-page')).toBeVisible();
  await page.locator('.org-list-link', { hasText: new RegExp(`^${ORG}$`) }).click();
  await expect(page.locator('.org-landing-page')).toBeVisible();
  await page.locator('.org-team-link', { hasText: TEAM }).click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.locator('.kb-new-btn').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
  await page.locator('input[placeholder="Task title"]').fill('DetailTask');
  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.locator('.editor-btn-row .editor-btn:not(.editor-btn-cancel):not(.editor-btn-danger)').click();
  await expect(page.locator('.kb-board')).toBeVisible();

  await page.close();
});

// Lead flips the per-team setting via the team Settings page.
async function setEditDetails(page: Page, on: boolean) {
  await gotoTeamBoard(page);
  await page.locator('.kb-manage-link', { hasText: 'Settings' }).click();
  await expect(page.locator('.team-settings-page')).toBeVisible();
  const cb = page.getByRole('checkbox', { name: 'Members can edit task details' });
  if ((await cb.isChecked()) !== on) await cb.click();
  await expect(cb).toBeChecked({ checked: on });
}

// Open DetailTask's full editor as the currently-logged-in user.
async function openDetailTaskEditor(page: Page) {
  await gotoTeamBoard(page);
  await page.locator('.kb-card', { hasText: 'DetailTask' }).first().click();
  await expect(page.locator('.Wt-dialog.kb-task-popup')).toBeVisible();
  await page.locator('.kb-popup-full-link').click();
  await expect(page.locator('.kb-editor-page')).toBeVisible();
}

test('member cannot edit task title when the setting is off', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const admin    = await adminCtx.newPage();
  await loginAs(admin, 'admin', 'testpass');
  await setEditDetails(admin, false);
  await adminCtx.close();

  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'edit_member', 'editpass');
  await openDetailTaskEditor(page);

  // Read-only members see a visible readonly title input.
  await expect(page.locator('input[placeholder="Task title"][readonly]')).toBeVisible();
  await ctx.close();
});

test('member can edit task title when the setting is on', async ({ browser }) => {
  const adminCtx = await browser.newContext();
  const admin    = await adminCtx.newPage();
  await loginAs(admin, 'admin', 'testpass');
  await setEditDetails(admin, true);
  await adminCtx.close();

  const ctx  = await browser.newContext();
  const page = await ctx.newPage();
  await loginAs(page, 'edit_member', 'editpass');
  await openDetailTaskEditor(page);

  // Editable members have no readonly title input (it is hidden behind the
  // click-to-edit display instead).
  await expect(page.locator('input[placeholder="Task title"][readonly]')).toHaveCount(0);
  await ctx.close();
});
```

- [ ] **Step 2: Run the spec to verify it passes**

Run: from `/home/altainia/code/altinf/e2e`, `npx playwright test member-edit-details.spec.ts`
Expected: 2 passed.

- [ ] **Step 3: Commit**

```bash
git add e2e/specs/member-edit-details.spec.ts
git commit -m "test(e2e): member detail-edit gated by team setting"
```

---

## Task 6: Full verification across all three suites

**Files:** none (verification + final state).

- [ ] **Step 1: Build everything**

Run: `cmake --build /home/altainia/code/altinf/build --parallel $(nproc)`
Expected: builds clean.

- [ ] **Step 2: Run the full Catch2 suite**

Run: `ctest --test-dir /home/altainia/code/altinf/build --output-on-failure`
Expected: 100% pass (the previously-failing `team_cap` case is now green; new `kanban_db` case passes).

- [ ] **Step 3: Run the JS unit tests**

Run: from `/home/altainia/code/altinf/tests/js`, `npm test`
Expected: all pass (unaffected).

- [ ] **Step 4: Run the full e2e suite**

Run: from `/home/altainia/code/altinf/e2e`, `npx playwright test`
Expected: all pass, including the new spec; no regression in `board.spec.ts` / `task-permissions.spec.ts`.

- [ ] **Step 5: Final commit (if any verification fixups were needed)**

```bash
git add -A
git commit -m "chore: verification fixups for member-edit-details" || true
```

---

## Notes for the implementer

- Default is **on**: `allow_member_edit_details` defaults to `1`/`true` everywhere (schema, record, entry, migration), so existing teams keep today's behavior.
- `team_cap::edit_task_details` now means strictly "is a lead." Do not reintroduce it into `team_member_caps`.
- Keep the to/from-Done restriction leads-only — it intentionally stays keyed on `edit_task_details`, not on `can_edit_details()`.
- `can_edit` in the editor is the single switch for title/description/date/type UI affordances; changing only its definition (Task 3, Step 3) propagates to all of them.
