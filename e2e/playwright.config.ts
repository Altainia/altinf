import { defineConfig } from '@playwright/test';
import * as path from 'path';

const PROJECT_ROOT = path.resolve(__dirname, '..');
const TEST_DIR = '/tmp/altinf-e2e-test';
const PORT = 9090;
const BINARY = path.join(PROJECT_ROOT, 'build', 'altinf');
const DOCROOT = path.join(PROJECT_ROOT, 'build', 'resources');
// Tests use the committed template, not the (untracked) live wt_config.xml, so
// local config edits never change test behavior and a fresh clone always works.
const WT_CONFIG = path.join(PROJECT_ROOT, 'wt_config.template.xml');

export default defineConfig({
  testDir: './specs',
  timeout: 30_000,
  retries: 1,
  workers: 3,
  expect: { timeout: 10_000 },
  use: {
    baseURL: `http://localhost:${PORT}`,
    // Wt bootstraps over WebSocket; give it a moment after navigation
    actionTimeout: 10_000,
  },
  globalSetup: './global-setup.ts',
  globalTeardown: './global-teardown.ts',
  reporter: [['list'], ['html', { open: 'never' }]],
  webServer: {
    command: [
      // Prepare a fresh app-root immediately before launching the server, so it
      // exists when the app migrates the DB at startup (Playwright does not
      // guarantee globalSetup finishes before the webServer starts).
      `rm -rf ${TEST_DIR} &&`,
      `mkdir -p ${TEST_DIR} &&`,
      `(cp -r ${PROJECT_ROOT}/posts ${TEST_DIR}/posts 2>/dev/null || mkdir -p ${TEST_DIR}/posts) &&`,
      `ALTINF_ADMIN_PASSWORD=testpass`,
      BINARY,
      `--approot ${TEST_DIR}`,
      `--docroot "${DOCROOT};/css,/js"`,
      `--http-address 0.0.0.0`,
      `--http-port ${PORT}`,
      `--config ${WT_CONFIG}`,
    ].join(' '),
    url: `http://localhost:${PORT}`,
    reuseExistingServer: false,
    timeout: 15_000,
  },
});
