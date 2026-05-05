# Design: Navbar Login Link, Login Autofocus, Org Form Enter Key

**Date:** 2026-05-05

## Overview

Three small, independent UI polish changes:

1. Show a "Login" link in the navbar when not logged in.
2. Auto-focus the username field when the login page loads.
3. Pressing Enter in the "Username" invite field or "Team name" field in Org management triggers the respective form action.

---

## Change 1: Navbar Login Link

**File:** `src/widgets/nav_bar.cpp`

**Current behavior:** `nav_bar::update()` returns early when `!m_session.logged_in`, leaving `m_auth_area` empty — no feedback that a login path exists.

**Change:** Before returning early, add a `WAnchor` pointing to `/login` with text "Login" and style class `nav-link nav-login`. This matches the position and base styling of the "Logout" link shown when logged in.

```cpp
if(!m_session.logged_in)
{
    m_auth_area->addNew<Wt::WAnchor>(
        Wt::WLink{Wt::LinkType::InternalPath, "/login"}, "Login")
      ->setStyleClass("nav-link nav-login");
    return;
}
```

`nav-login` is an additive class alongside `nav-link` so SCSS can target the login link independently if needed in future.

**No SCSS changes required** — `nav-link` base styles already provide correct appearance.

---

## Change 2: Login Page Username Autofocus

**File:** `src/pages/login_page.cpp`

**Current behavior:** The login page renders with no focused field; the user must click the username input before typing.

**Change:** Call `m_username->setFocus()` immediately after creating the username `WLineEdit`. Wt emits a JavaScript `focus()` call on the next render pass.

```cpp
m_username = form->addNew<Wt::WLineEdit>();
m_username->setPlaceholderText("Username");
m_username->setStyleClass("login-field");
m_username->setFocus();   // <-- add this
```

---

## Change 3: Enter Key in Org Management Forms

**File:** `src/pages/kanban_team_page.cpp`

**Current behavior:** The "Username" invite field and the "Team name" field do not respond to Enter. The user must click the button to submit.

**Change:** Hoist each button's inline lambda into a named local variable, then connect it to both the button's `clicked()` signal and the input's `enterPressed()` signal.

### Invite form (around line 73)

```cpp
auto do_invite = [this] {
    // ... existing invite logic unchanged ...
};
invite_btn->clicked().connect(do_invite);
m_invite_input->enterPressed().connect(do_invite);
```

### Create team form (around line 116)

```cpp
auto do_create = [this] {
    // ... existing create logic unchanged ...
};
new_team_btn->clicked().connect(do_create);
m_new_team_input->enterPressed().connect(do_create);
```

No business logic changes — only the connection pattern changes.

---

## Architecture

All three changes are isolated to their respective files with no cross-file dependencies.

| Change | File | Lines affected |
|--------|------|---------------|
| Navbar login link | `src/widgets/nav_bar.cpp` | 45–48 |
| Login autofocus | `src/pages/login_page.cpp` | 21–24 |
| Invite Enter key | `src/pages/kanban_team_page.cpp` | 73–93 |
| Create Enter key | `src/pages/kanban_team_page.cpp` | 116–127 |

---

## Error Handling

No new error paths. The existing invite and create callbacks already validate empty input and invalid usernames — Enter key reuses that logic unchanged.

---

## Testing

- **Playwright E2E:** Navigate to the site while logged out and verify the navbar shows a "Login" link that navigates to `/login`. Verify the login page focuses the username field on load. On the org management page, verify Enter in the username invite field sends the invite, and Enter in the team name field creates the team.
- **JS unit / Catch2:** No new logic; no new unit tests required.
