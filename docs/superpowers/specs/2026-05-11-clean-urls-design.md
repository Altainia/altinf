# Clean URLs — Design Spec

**Date:** 2026-05-11  
**Status:** Approved (revised)

## Problem

When navigating within the app, the browser address bar shows URLs like `/?_=/blog` instead of `/blog`.

### Root cause (from Wt 4.13.1 source)

`/?_=` appears when `WebSession::useUglyInternalPaths()` returns `true`. Tracing the call chain:

1. `WServer.C:176` — `setUseSlashExceptionForInternalPaths(defaultStatic_)`
2. `defaultStatic_` starts `true` (line 241 in `http/Configuration.C`)
3. `defaultStatic_` is only set to `false` (line 374) when `--docroot` uses the semicolon format: `path;/p1,/p2`
4. The app currently passes `--docroot /app/resources` (no semicolon) → `defaultStatic_` stays `true` → `useUglyInternalPaths()` = `true` → `/?_=`

**The session tracking mode (`<tracking>`) is unrelated.** It controls whether session IDs appear in URLs, not internal path encoding.

Additionally, `WEnvironment.C:197–198` shows that when a modern browser sends `htmlHistory=true` (via `Boot.js`, whenever `window.history.pushState` exists) and `useUglyInternalPaths()` is `false`, Wt uses the HTML5 History API — producing clean paths.

## Solution

### 1. Add static path declarations to `--docroot` (primary fix)

Change the `--docroot` flag from:
```
--docroot /app/resources
```
to:
```
--docroot /app/resources;/css,/js
```

The semicolon format tells Wt which URL paths to serve as static files (`/css` and `/js` map into the docroot directory). Everything else (`/blog`, `/links`, `/board/3`, etc.) is routed to the Wt app. This sets `defaultStatic_ = false` → `useUglyInternalPaths() = false` → HTML5 History API → clean URLs.

This must be updated in **two places**:
- `Dockerfile` ENTRYPOINT (production)
- `e2e/playwright.config.ts` `webServer.command` (E2E tests)

### 2. Trusted proxy network in `wt_config.xml` (secondary improvement)

```xml
<trusted-proxy-networks>172.16.0.0/12</trusted-proxy-networks>
```

Covers Docker's default bridge and overlay subnets. Tells Wt to trust `X-Forwarded-For` and `X-Forwarded-Proto` from Caddy, so Wt correctly knows the public scheme and host. Not required for clean URLs, but correct for a reverse-proxied deployment.

## Scope

- **Files changed:** `Dockerfile`, `e2e/playwright.config.ts`, `wt_config.xml`
- **No changes to:** `Caddyfile`, `docker-compose.yml`, or any C++ source
- **App routing logic:** unchanged — `setInternalPath` calls work identically

## Behavior after change

| Action | Before | After |
|--------|--------|-------|
| Navigate to blog | `/?_=/blog` | `/blog` |
| Navigate to board | `/?_=/board/3` | `/board/3` |
| Login redirect | `/?_=/login` | `/login` |
| Direct link to `/blog` | Works (Caddy forwards to Wt) | Same |
| No-JS fallback | `/?_=/blog` | `#/blog` (hash-based) |

## Rollout

Rebuild and redeploy the Docker image. Both `wt_config.xml` and the `Dockerfile` ENTRYPOINT change are baked into the image.

## Risks

- **Static path list must stay in sync:** If new static asset directories are added (e.g., `/fonts`), they must be added to the `--docroot` semicolon list or Wt will route them to the app instead of serving them from disk. This is low-risk since the app currently only has `/css` and `/js`.
- **Docker subnet:** `172.16.0.0/12` covers standard Docker bridge/overlay ranges. Non-standard networks may need updating.
