import { defineConfig } from '@playwright/test';
import * as path from 'path';

const PROJECT_ROOT = path.resolve(__dirname, '..');
const TEST_DIR = '/tmp/altinf-e2e-test';
const PORT = 9090;
const BINARY = path.join(PROJECT_ROOT, 'build', 'altinf');
const DOCROOT = path.join(PROJECT_ROOT, 'build', 'resources');
const WT_CONFIG = path.join(PROJECT_ROOT, 'wt_config.xml');

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
      `ALTINF_ADMIN_PASSWORD=testpass`,
      BINARY,
      `--approot ${TEST_DIR}`,
      `--docroot ${DOCROOT}`,
      `--http-address 0.0.0.0`,
      `--http-port ${PORT}`,
      `--config ${WT_CONFIG}`,
    ].join(' '),
    url: `http://localhost:${PORT}`,
    reuseExistingServer: false,
    timeout: 15_000,
  },
});
