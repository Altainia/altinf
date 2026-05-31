export default async function globalSetup() {
  // App-root preparation (wipe, mkdir, copy posts) now happens in the
  // webServer command in playwright.config.ts, immediately before the server
  // launches. Doing it here would race with — and could wipe the DB created
  // by — a server that the app migrates at startup, since Playwright does not
  // guarantee globalSetup completes before the webServer starts.
}
