# Caddy Reverse Proxy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Caddy as a reverse proxy sidecar in docker-compose, removing Wt's SSL configuration so that Caddy handles TLS termination, HTTP→HTTPS redirect, and Let's Encrypt cert management for altinf.com.

**Architecture:** Caddy runs alongside the altinf container on the same Docker bridge network, listening on host ports 80 and 443. It proxies traffic to the Wt container over plain HTTP on port 8080 (internal Docker network only). Caddy auto-issues and renews a Let's Encrypt cert for altinf.com.

**Tech Stack:** Caddy 2 (`caddy:alpine` Docker image), Docker Compose, Wt (C++ web framework)

---

### Task 1: Create Caddyfile

**Files:**
- Create: `Caddyfile`

- [ ] **Step 1: Create `Caddyfile` at the project root**

```
altinf.com {
    reverse_proxy altinf:8080
}
```

`altinf` resolves to the altinf container by service name on the Docker bridge network. Caddy infers HTTPS for a bare domain, issues a Let's Encrypt cert via the ACME HTTP-01 challenge, and redirects HTTP→HTTPS automatically.

- [ ] **Step 2: Validate the Caddyfile syntax**

Run:
```bash
docker run --rm -v "$PWD/Caddyfile:/etc/caddy/Caddyfile" caddy:alpine caddy validate --config /etc/caddy/Caddyfile
```
Expected output contains: `Valid configuration`

- [ ] **Step 3: Commit**

```bash
git add Caddyfile
git commit -m "feat: add Caddyfile for Caddy reverse proxy"
```

---

### Task 2: Update docker-compose.yml

**Files:**
- Modify: `docker-compose.yml`

- [ ] **Step 1: Replace `docker-compose.yml` with the following**

```yaml
services:
  caddy:
    image: caddy:alpine
    restart: unless-stopped
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./Caddyfile:/etc/caddy/Caddyfile:ro
      - caddy_data:/data

  altinf:
    image: ghcr.io/altainia/altinf:latest
    restart: unless-stopped
    volumes:
      - ./data:/data
    environment:
      - ALTINF_ADMIN_PASSWORD=${ALTINF_ADMIN_PASSWORD:-}

volumes:
  caddy_data:
```

Changes from the previous version:
- Added `caddy` service with ports 80/443 and two volumes.
- Removed `ports` from `altinf` (no longer exposed to host).
- Removed `./certs:/certs:ro` volume from `altinf`.
- Added `caddy_data` named volume (persists Let's Encrypt certs across restarts — never delete with `-v`).

- [ ] **Step 2: Validate the compose file**

Run:
```bash
docker compose config
```
Expected: YAML output with no errors. Confirm `caddy` and `altinf` both appear as services and `caddy_data` appears under `volumes`.

- [ ] **Step 3: Commit**

```bash
git add docker-compose.yml
git commit -m "feat: add Caddy service to docker-compose, remove altinf port exposure"
```

---

### Task 3: Update Dockerfile

**Files:**
- Modify: `Dockerfile`

- [ ] **Step 1: Update `EXPOSE` in the runtime stage**

In the `# ── Stage 3: runtime` section, find:
```dockerfile
EXPOSE 8080 8443
```
Replace with:
```dockerfile
EXPOSE 8080
```

- [ ] **Step 2: Remove `/certs` from the VOLUME declaration**

Find:
```dockerfile
VOLUME ["/data", "/certs"]
```
Replace with:
```dockerfile
VOLUME ["/data"]
```

- [ ] **Step 3: Strip SSL flags from ENTRYPOINT**

Find:
```dockerfile
ENTRYPOINT ["/app/altinf", \
    "--config",        "/app/wt_config.xml", \
    "--docroot",       "/app/resources", \
    "--approot",       "/data", \
    "--http-address",  "0.0.0.0", "--http-port",  "8080", \
    "--https-address", "0.0.0.0", "--https-port", "8443", \
    "--ssl-certificate",  "/certs/cert.pem", \
    "--ssl-private-key",  "/certs/key.pem", \
    "--ssl-tmp-dh",       "/certs/dh.pem"]
```
Replace with:
```dockerfile
ENTRYPOINT ["/app/altinf", \
    "--config",       "/app/wt_config.xml", \
    "--docroot",      "/app/resources", \
    "--approot",      "/data", \
    "--http-address", "0.0.0.0", "--http-port", "8080"]
```

- [ ] **Step 4: Commit**

```bash
git add Dockerfile
git commit -m "feat: remove SSL config from Dockerfile, Caddy handles TLS"
```

---

### Task 4: Push and deploy

- [ ] **Step 1: Push to trigger CI build**

```bash
git push
```

Wait for the GitHub Actions workflow to build and push the new image to `ghcr.io/altainia/altinf:latest`. Confirm the workflow passes before continuing.

- [ ] **Step 2: Confirm DNS is ready**

Verify `altinf.com` resolves to the server's public IP:
```bash
dig +short altinf.com
```
Expected: the server's public IP. If not, update the DNS A record and wait for propagation. Caddy's ACME HTTP-01 challenge requires port 80 to be reachable from the internet under the correct hostname.

- [ ] **Step 3: Confirm ports 80 and 443 are open on the server firewall**

SSH to the production server and check:
```bash
sudo ufw status
```
If 80 and 443 are not listed as allowed:
```bash
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
```

- [ ] **Step 4: Deploy**

On the production server:
```bash
cd ~/altinf   # directory containing docker-compose.yml
./deploy.sh
```

`deploy.sh` runs `docker compose pull && docker compose up -d`. The first pull will fetch `caddy:alpine` and the new `altinf` image.

- [ ] **Step 5: Confirm cert was issued**

```bash
docker compose logs caddy
```
Look for a line containing `certificate obtained successfully` or `serving initial certificate` for `altinf.com`. If you see ACME errors instead, check DNS propagation and firewall rules, then restart Caddy:
```bash
docker compose restart caddy
```

- [ ] **Step 6: Smoke test — HTTP redirect**

```bash
curl -I http://altinf.com
```
Expected:
```
HTTP/1.1 301 Moved Permanently
Location: https://altinf.com/
```

- [ ] **Step 7: Smoke test — HTTPS**

```bash
curl -I https://altinf.com
```
Expected:
```
HTTP/2 200
```
No certificate errors.

- [ ] **Step 8: Browser test**

Visit `https://altinf.com` in a browser. Confirm:
- Padlock shows as valid (no warnings)
- App loads and you can log in
- Navigation works as expected

---

### Task 5: Clean up production server

- [ ] **Step 1: Remove the old certs directory**

Once everything is confirmed working:
```bash
rm -rf ~/altinf/certs
```
Caddy manages its own certs in the `caddy_data` Docker volume. The `certs/` directory is no longer used.

- [ ] **Step 2: Close old ports on the firewall**

If the firewall previously had 8080 or 8443 open, close them — the altinf container no longer exposes those ports:
```bash
sudo ufw delete allow 8080/tcp
sudo ufw delete allow 8443/tcp
```
