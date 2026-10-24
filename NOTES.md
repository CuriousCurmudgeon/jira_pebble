# Pebble Jira app: context

**Goal:** A Jira Cloud app for my **Pebble Time 2**, built mostly for fun. The project lives in `~/code/jira_pebble`.

## Hardware: Pebble Time 2

- A 200×228 color e-paper screen (64 colors) and a touchscreen.
- Four buttons: Back, Up, Select, Down.
- A microphone for dictation.
- Build only for the Time 2 platform at first. Find its platform name in Core's SDK docs.

## Architecture

```
Watch (C app)  ←Bluetooth/AppMessage→  Phone (PebbleKit JS)  ←HTTPS→  Jira Cloud REST API
```

- **Watch app (C):** The UI and the input only. It has no network access.
- **PebbleKit JS (on the phone):** Calls Jira with `XMLHttpRequest`, so it uses the phone's TLS stack and needs no proxy.
  It keeps only the key, a short summary, the status and the priority, then sends small AppMessage dictionaries to the
  watch, one issue or a few fields at a time.
- **Config page (Clay):** The site URL, the email and the API token. They're stored in PebbleKit JS `localStorage`.
- **Auth:** Basic auth with `email:api_token`. Atlassian API tokens now expire, so the token must be easy to update in
  the config page.

## Jira API notes

- **My queue:** `/rest/api/3/search/jql` with the JQL `assignee = currentUser() AND statusCategory != Done`. The old
  `/search` endpoint is deprecated. Pass `fields=` and a small `maxResults`.
- **Transitions:** `GET` and `POST /rest/api/3/issue/{key}/transitions`. Fetch them per issue, because workflows differ.
- **Comments:** In v3, comment bodies use Atlassian Document Format (ADF), so wrap plain text in a minimal ADF document.
  The v2 endpoints take plain strings instead.

## Constraints

- A small app heap: hold about 20–25 issues on the watch and keep descriptions on the phone.
- Small AppMessage buffers.
- PebbleKit JS runs only while the app is open, so there's no background polling. Refresh when the app opens.
- Text entry is limited to dictation and canned replies.

## UI design

- **List:** A `MenuLayer` with about 5–6 rows. Each row has a stripe in the color of its status category, with red for
  blocked or highest-priority issues. Below that come the key in bold and a summary of up to two lines. The header reads
  "My queue · N".
- **Issue view:** A colored status banner, then the summary, priority and assignee. An `ActionBarLayer` maps the
  buttons:
  - **Up:** Move to the next status.
  - **Select:** Open a menu of all the transitions.
  - **Down:** Add a comment, by dictation first and canned replies second ("On it", "Blocked", "Ready for review").
- **Touch:** Swiping and tapping are a supplement only, until I confirm the SDK supports touch.
- **Glance:** "3 in progress · 1 blocked".
- **Vibration:** A tap when the list holds an issue it didn't hold the last time the app opened.
- **Optional watchface:** The time, the number of issues in progress and the days left in the sprint, with an accent
  color that turns red near the end of the sprint.

## Still to confirm in the current Core/Rebble docs

- How to install the SDK and use the emulator.
- Whether the Core phone app supports PebbleKit JS and dictation.
- Whether the Time 2 SDK exposes the touchscreen.

## First milestone

In the emulator, have PebbleKit JS call `GET /rest/api/3/myself` with the token, then show my display name on the
watch. That proves the whole path from the watch through the phone to Jira. After that, scaffold the app, the JS client
and the Clay config.
