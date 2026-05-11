# Clean URLs — Design Spec

**Date:** 2026-05-11  
**Status:** Approved

## Problem

When navigating within the app, the browser address bar shows URLs like `/?_=/blog` instead of `/blog`. This is Wt's fallback behavior when session tracking is set to `Auto`: it encodes the session ID and internal path into the query string (`?_=`) rather than using the browser History API with clean paths.

## Solution

Change `wt_config.xml` only. Two additions:

### 1. Cookie-only session tracking

```xml
<tracking>Cookie</tracking>
```

Replaces `<tracking>Auto</tracking>`. Wt stops using URL-based session/path encoding entirely. The session is tracked exclusively via the `altinf_session` HTTP-only cookie already set at login. Internal navigation via `setInternalPath` uses `history.pushState`, producing clean paths like `/blog`.

### 2. Trusted proxy network

```xml
<trusted-proxy-networks>172.16.0.0/12</trusted-proxy-networks>
```

Covers Docker's default bridge and overlay subnets. Tells Wt to trust `X-Forwarded-For` and `X-Forwarded-Proto` headers forwarded by Caddy, so Wt correctly knows the public scheme (`https`) and host when generating any internal links or redirects.

## Scope

- **File changed:** `wt_config.xml` only
- **No changes to:** `Caddyfile`, `docker-compose.yml`, `Dockerfile`, or any C++ source
- **App routing logic:** unchanged — `setInternalPath` calls work identically

## Behavior after change

| Action | Before | After |
|--------|--------|-------|
| Navigate to blog | `/?_=/blog` | `/blog` |
| Navigate to board | `/?_=/board/3` | `/board/3` |
| Login redirect | `/?_=/login` | `/login` |
| Direct link to `/blog` | Works (Caddy forwards to Wt) | Same |

## Rollout

Rebuild and redeploy the Docker image. `wt_config.xml` is baked into the image via `COPY wt_config.xml /app/wt_config.xml` in the Dockerfile, so no volume or runtime changes are needed.

## Risks

- **Cookie dependency:** If a client has cookies disabled, there is no session fallback. Acceptable — the app already requires cookies for authentication.
- **Docker subnet:** `172.16.0.0/12` covers all standard Docker bridge/overlay ranges. If the deployment moves to a non-standard network, this may need updating.
