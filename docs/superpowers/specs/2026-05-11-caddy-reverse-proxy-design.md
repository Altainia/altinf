# Caddy Reverse Proxy — Design Spec

**Date:** 2026-05-11
**Status:** Approved

## Goal

Replace direct SSL handling in the Wt app with a Caddy reverse proxy sidecar. Caddy terminates SSL, issues and renews a Let's Encrypt cert for `altinf.com`, redirects HTTP→HTTPS, and proxies traffic to the Wt container over plain HTTP on the Docker internal network. Wt's SSL configuration is removed entirely.

## Architecture

```
Browser
  │
  ├─ HTTP :80 ──► Caddy ──► 301 redirect to HTTPS
  │
  └─ HTTPS :443 ─► Caddy (terminates SSL, Let's Encrypt cert for altinf.com)
                       │
                       └─ HTTP :8080 ──► altinf container (Docker internal network)
```

- Caddy and altinf run on the same Docker bridge network; Caddy resolves the Wt container as `altinf:8080`.
- The altinf container exposes no ports to the host. All external traffic enters through Caddy.
- Let's Encrypt cert issuance and renewal is fully automatic; no manual cert management is required.

## File Changes

### New: `Caddyfile` (project root)

```
altinf.com {
    reverse_proxy altinf:8080
}
```

Caddy infers HTTPS for a bare domain, issues the cert via ACME HTTP-01 challenge, and automatically redirects port 80 to HTTPS.

### Modified: `docker-compose.yml`

- Add `caddy` service:
  - Image: `caddy:alpine`
  - Ports: `80:80`, `443:443`
  - Volumes: `./Caddyfile:/etc/caddy/Caddyfile:ro`, `caddy_data:/data`
- `altinf` service:
  - Remove `ports` mapping entirely (Docker-internal only)
  - Remove `./certs:/certs:ro` volume mount
- Add `caddy_data` named volume declaration at the top level.

### Modified: `Dockerfile`

- `EXPOSE`: remove `8443`, keep `8080`.
- `ENTRYPOINT`: remove the five SSL flags:
  - `--https-address`, `--https-port`
  - `--ssl-certificate`, `--ssl-private-key`, `--ssl-tmp-dh`

### Unchanged

- `wt_config.xml` — no SSL settings present, no changes needed.
- `deploy.sh` — no changes.
- All application source code and build system.

## Error Handling

**Cert issuance failure:** Caddy uses the ACME HTTP-01 challenge, which requires port 80 to be publicly reachable. If the `altinf.com` DNS A record is not pointing at the server, Caddy logs the error and retries. Fix: update DNS, then restart Caddy (`docker compose restart caddy`).

**Cert renewal:** Caddy auto-renews before expiry. The `caddy_data` named volume must persist across deployments. `docker compose down` (without `-v`) preserves it. `docker compose down -v` deletes it and forces re-issuance — avoid in production.

**App behavior:** Wt continues to receive plain HTTP on port 8080. `wApp->environment().urlScheme()` will now always return `"https"` because Caddy sets the `X-Forwarded-Proto: https` header on every proxied request.

## Testing Plan

1. `docker compose up` on the server — confirm Caddy logs show cert issued for `altinf.com`.
2. `curl -I http://altinf.com` — expect `301` redirect to `https://altinf.com`.
3. `curl -I https://altinf.com` — expect `200` from the Wt app.
4. Browser visit to `https://altinf.com` — confirm no cert warnings and the app loads normally.

## Production Prerequisites

- DNS A record for `altinf.com` must point to the server's public IP before first startup.
- Port 80 and 443 must be open in the server's firewall.
- The `certs/` directory on the production machine can be removed after migration.
