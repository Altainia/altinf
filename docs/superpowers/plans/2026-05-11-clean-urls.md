# Clean URLs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate `/?_=/` from browser URLs by adding static-path declarations to `--docroot` so Wt uses the HTML5 History API for internal paths.

**Architecture:** Wt's `useUglyInternalPaths()` returns `true` when `--docroot` has no semicolon suffix, forcing `/?_=` URL encoding. Adding `;/css,/js` to `--docroot` sets `defaultStatic_ = false`, which lets Wt use `history.pushState` for clean paths. A secondary `wt_config.xml` change adds trusted-proxy-network support for Caddy. All E2E specs that hardcode `/?_=/path` URLs are updated to `/path`.

**Tech Stack:** Wt 4.13.1 (C++ web framework), Playwright (E2E tests), Caddy (reverse proxy), Docker

---

### Task 2 (revised): Update `--docroot` and `wt_config.xml`

**Files:**
- Modify: `Dockerfile`
- Modify: `e2e/playwright.config.ts`
- Modify: `wt_config.xml`

- [ ] **Step 1: Update the Dockerfile ENTRYPOINT**

In `Dockerfile`, change the `--docroot` line in the ENTRYPOINT from:
```
"--docroot",      "/app/resources", \
```
to:
```
"--docroot",      "/app/resources;/css,/js", \
```

The full updated ENTRYPOINT block:
```dockerfile
ENTRYPOINT ["/app/altinf", \
    "--config",       "/app/wt_config.xml", \
    "--docroot",      "/app/resources;/css,/js", \
    "--approot",      "/data", \
    "--http-address", "0.0.0.0", "--http-port", "8080"]
```

- [ ] **Step 2: Update playwright.config.ts**

In `e2e/playwright.config.ts`, change the `--docroot` line in the `webServer.command` array from:
```typescript
`--docroot ${DOCROOT}`,
```
to:
```typescript
`--docroot "${DOCROOT};/css,/js"`,
```

- [ ] **Step 3: Update wt_config.xml**

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
      <tracking>Auto</tracking>
    </session-management>
    <trusted-proxy-networks>172.16.0.0/12</trusted-proxy-networks>
    <max-request-size>1048576</max-request-size>
    <debug>false</debug>
  </application-settings>
</server>
```

Change from the original: adds `<trusted-proxy-networks>172.16.0.0/12</trusted-proxy-networks>`. The `<tracking>` stays `Auto`.

- [ ] **Step 4: Run the clean-URL test (from Task 1) to confirm it now passes**

```bash
cd e2e && npx playwright test specs/home.spec.ts --grep "clean URL" --reporter=list
```

Expected output: test PASSES — URL after clicking Blog is `/blog`.

- [ ] **Step 5: Commit**

```bash
git add Dockerfile e2e/playwright.config.ts wt_config.xml
git commit -m "config: use docroot static-path list to enable clean URLs via HTML5 History API"
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

Expected: all tests pass. If any fail, check whether the failure relates to URL navigation — the `/?_=/` pattern should no longer appear anywhere.

- [ ] **Step 3: If all tests pass, done — no further commit needed**

The Docker image rebuild on the next CI/CD run will bake the updated `Dockerfile` ENTRYPOINT and `wt_config.xml` into the image. Deploy by pushing the new image and running `docker compose pull && docker compose up -d` on the server.
