import * as fs from 'fs';

export default async function globalTeardown() {
  fs.rmSync('/tmp/altinf-e2e-test', { recursive: true, force: true });
}
