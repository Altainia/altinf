# Clean URLs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate `/?_=/` from browser URLs by switching Wt session tracking to cookie-only mode and updating all E2E specs accordingly.

**Architecture:** Two changes to `wt_config.xml` (`<tracking>Cookie</tracking>` and `<trusted-proxy-networks>`) tell Wt to use cookies for sessions and trust Caddy's forwarded headers. With cookie tracking active, `setInternalPath` uses `history.pushState` to produce clean paths. All E2E specs that currently navigate to `/?_=/path` are updated to use `/path`.

**Tech Stack:** Wt 4.13.1 (C++ web framework), Playwright (E2E tests), Caddy (reverse proxy), Docker

---

### Task 1: Write a failing clean-URL test

**Files:**
- Modify: `e2e/specs/home.spec.ts`

- [ ] **Step 1: Add a test that asserts clean URL after nav-link click**

Append to `e2e/specs/home.spec.ts`:

```typescript
test('clicking Blog nav link produces a clean URL', async ({ page }) => {
  await page.goto('/');
  await page.locator('.nav-link', { hasText: 'Blog' }).click();
  await expect(page).toHaveURL('/blog');
});
```

- [ ] **Step 2: Run just this test to confirm it fails**

```bash
cd e2e && npx playwright test specs/home.spec.ts --grep "clean URL" --reporter=list
```

Expected output: test FAILS — the actual URL is `/?_=/blog`, not `/blog`.

- [ ] **Step 3: Commit the failing test**

```bash
git add e2e/specs/home.spec.ts
git commit -m "test(e2e): add failing clean-URL assertion"
```

---

### Task 2: Update wt_config.xml

**Files:**
- Modify: `wt_config.xml`

- [ ] **Step 1: Replace the config file with the updated content**

Replace the entire contents of `wt_config.xml` with:

```xml
<?xml version="1.0" encoding="utf-8"?>
<server>
  <application-settings location="*">
    <session-management>
      <shared-process>
        <num-processes>1</num-processes>
      </shared-process>
      <reload-is-new-session>true</reload-is-new-session>
      <tracking>Cookie</tracking>
    </session-management>
    <trusted-proxy-networks>172.16.0.0/12</trusted-proxy-networks>
    <max-request-size>1048576</max-request-size>
    <debug>false</debug>
  </application-settings>
</server>
```

Key changes vs. the original:
- `<tracking>Auto</tracking>` → `<tracking>Cookie</tracking>`
- Added `<trusted-proxy-networks>172.16.0.0/12</trusted-proxy-networks>`

- [ ] **Step 2: Run the new test to confirm it now passes**

```bash
cd e2e && npx playwright test specs/home.spec.ts --grep "clean URL" --reporter=list
```

Expected output: test PASSES — URL after clicking Blog is `/blog`.

- [ ] **Step 3: Commit**

```bash
git add wt_config.xml
git commit -m "config: switch Wt session tracking to cookie-only, trust Docker proxy network"
```

---

### Task 3: Update all E2E specs to use clean URLs

**Files:**
- Modify: `e2e/specs/blog.spec.ts`
- Modify: `e2e/specs/board.spec.ts`
- Modify: `e2e/specs/comments.spec.ts`
- Modify: `e2e/specs/home.spec.ts`
- Modify: `e2e/specs/links.spec.ts`
- Modify: `e2e/specs/live-board.spec.ts`
- Modify: `e2e/specs/live-nav-accounts.spec.ts`
- Modify: `e2e/specs/live-org-landing.spec.ts`
- Modify: `e2e/specs/live-org-manage.spec.ts`
- Modify: `e2e/specs/live-task-editor.spec.ts`
- Modify: `e2e/specs/live-task-types.spec.ts`
- Modify: `e2e/specs/login.spec.ts`
- Modify: `e2e/specs/notifications.spec.ts`
- Modify: `e2e/specs/task-history.spec.ts`
- Modify: `e2e/specs/task-permissions.spec.ts`

- [ ] **Step 1: Replace all `/?_=/` occurrences with `/` across all spec files**

From the repo root:

```bash
sed -i "s|'/?_=/|'/|g" e2e/specs/*.ts
```

This converts every `page.goto('/?_=/blog')` → `page.goto('/blog')`, `page.goto('/?_=/login')` → `page.goto('/login')`, etc.

- [ ] **Step 2: Verify the substitution looks correct**

```bash
grep -rn "_=/" e2e/specs/
```

Expected output: no output (all occurrences removed).

- [ ] **Step 3: Spot-check two files to confirm the replacements are sensible**

```bash
grep -n "page.goto" e2e/specs/blog.spec.ts
grep -n "page.goto" e2e/specs/login.spec.ts
```

Expected: every `goto` call now uses a clean path like `'/blog'`, `'/login'`, `'/links'`, `'/board/1'`.

- [ ] **Step 4: Commit**

```bash
git add e2e/specs/
git commit -m "test(e2e): update all specs to use clean URLs"
```

---

### Task 4: Run the full test suite and verify

- [ ] **Step 1: Run Catch2 unit tests**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass (no C++ code changed).

- [ ] **Step 2: Run all Playwright E2E tests**

```bash
cd e2e && npx playwright test --reporter=list
```

Expected: all tests pass. If any fail, check whether they relied on `/?_=/` URLs still being navigable — with cookie tracking active, Wt no longer routes on that query parameter.

- [ ] **Step 3: If all tests pass, done — no further commit needed**

The Docker image rebuild on the next CI/CD run will bake the updated `wt_config.xml` into the image (`COPY wt_config.xml /app/wt_config.xml` in `Dockerfile`). Deploy by pushing the new image and doing a `docker compose pull && docker compose up -d` on the server.
