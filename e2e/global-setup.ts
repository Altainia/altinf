import * as fs from 'fs';
import * as path from 'path';

const TEST_DIR = '/tmp/altinf-e2e-test';
const PROJECT_ROOT = path.resolve(__dirname, '..');

export default async function globalSetup() {
  // Fresh test app-root on each run so ALTINF_ADMIN_PASSWORD creates a clean admin
  fs.rmSync(TEST_DIR, { recursive: true, force: true });
  fs.mkdirSync(TEST_DIR, { recursive: true });

  // Copy posts into the test app-root so the blog page has content
  const postsDir = path.join(PROJECT_ROOT, 'posts');
  if (fs.existsSync(postsDir)) {
    fs.cpSync(postsDir, path.join(TEST_DIR, 'posts'), { recursive: true });
  } else {
    fs.mkdirSync(path.join(TEST_DIR, 'posts'));
  }
}
