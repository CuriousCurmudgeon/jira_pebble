# Pebble Jira app: context

**Goal:** A Jira Cloud app for my **Pebble Time 2**, built mostly for fun, to review the current sprint's work from my
wrist. The project lives in `~/code/jira_pebble`.

## Hardware: Pebble Time 2

- A 200×228 color e-paper screen (64 colors) and a touchscreen.
- Four buttons: Back, Up, Select, Down.
- A microphone for dictation.
- The SDK calls this platform `emery`. Build only for it for now.

## Architecture

```
Watch (C app)  ←Bluetooth/AppMessage→  Phone (PebbleKit JS)  ←HTTPS→  Jira Cloud REST API
```

- **Watch app (C):** The UI and the input only. It has no network access.
- **PebbleKit JS (on the phone):** Calls Jira with `XMLHttpRequest`, so it uses the phone's TLS stack and needs no proxy.
  It loads the whole sprint, keeps it in memory, and sends the watch small AppMessage dictionaries: the sprint and its
  columns first, then one column's issues when the watch asks for them, one issue per message.
- **Config page (Clay):** The site URL, the email, the API token and the board. They're stored in PebbleKit JS
  `localStorage` and never sent to the watch.
- **Auth:** Basic auth with `email:api_token`, using a scoped API token. Atlassian API tokens expire, so the token must
  be easy to update in the config page.
- **Permissions:** SDK 4.44 builds apps that must declare their network access, so `package.json` lists `Internet` for
  `atlassian.net` and `api.atlassian.com`.

## Jira API notes

- **Scoped tokens:** These only work through Atlassian's API gateway, at
  `https://api.atlassian.com/ex/jira/{cloudId}/...`, not at `yourcompany.atlassian.net`. The phone gets the cloud ID
  from `https://yourcompany.atlassian.net/_edge/tenant_info` (no auth needed) and caches it per site.
- **Scopes:** `read:jira-work`, `read:jira-user`, `read:board-scope:jira-software`,
  `read:board-scope.admin:jira-software`, `read:project:jira` and `read:sprint:jira-software`. The `.admin` one is for
  the board configuration; the API docs ask only for permission to view the board.
- **Board:** `/rest/agile/1.0/board?name=…`, falling back to `?projectKeyOrId=…&type=scrum`. The config page takes a
  board name, a board ID or a project key. The board ID is cached.
- **Columns:** `/rest/agile/1.0/board/{id}/configuration` lists the columns in board order, each with the IDs of the
  statuses mapped to it. An issue whose status is in no column doesn't show on the board, so the app leaves it out too.
- **Active sprint:** `/rest/agile/1.0/board/{id}/sprint?state=active`. The sprint's `endDate` gives the days left.
- **Sprint issues:** `/rest/api/3/search/jql` with `sprint = {id} ORDER BY Rank ASC`. The old `/search` endpoint is
  deprecated. Pass `fields=` and page with `nextPageToken`.
- **Transitions:** `GET` and `POST /rest/api/3/issue/{key}/transitions`. Fetch them per issue, because workflows differ.
  Jira doesn't say which transition is the "next" one.
- **Comments:** In v3, comment bodies use Atlassian Document Format (ADF), so wrap plain text in a minimal ADF document.
  The v2 endpoints take plain strings instead.

## Constraints

- A small app heap: the watch holds at most 12 columns and 50 issues per column. The phone loads up to 500 sprint
  issues and keeps descriptions to itself.
- Small AppMessage buffers. The phone cuts each string to fit the watch's buffer, by UTF-8 bytes, with an ellipsis.
- PebbleKit JS runs only while the app is open, so there's no background polling. Refresh when the app opens.
- Text entry is limited to dictation and canned replies.

## UI design

- **Home:** The header shows the sprint's done and total story points on the left (such as "9/82 pts done"), or the
  sprint name if the board has no points, and the days left on the right. Only one sprint is active at a time, so the
  name matters less than the points. The header turns red with two days or fewer to go. Below it, a `MenuLayer` lists
  the board's columns in board order, each with its story points, its issue count and a stripe in its status
  category's color: gray for To Do, blue for In Progress, green for Done.
- **Column:** The header shows the column name and its count, in the column's color. Each row has a stripe, red for
  blocked or highest-priority issues, then a badge in the issue type's color, the key in bold, the days in status
  (such as "6d") and the assignee on the right, and a summary of up to two lines. An assignee whose name doesn't fit
  is shortened to a first name and last initial, such as "Ada L.".
- **Issue types:** Colored as Jira colors them: green for Story, red for Bug, blue for Task, light blue for Subtask,
  purple for Epic and gray for any other type.
- **Issue view:** A colored status banner stays at the top. Below it, Up and Down scroll the key with the issue type in
  its color, the summary, the epic in purple, the story points and days in the current status, the priority and the
  assignee.
- **Story points:** From the field the board's configuration names for estimation.
- **Time in status:** The latest status change in the issue's changelog, or its status category change date if that's
  later, because the search may leave out older changelog entries.
- **Epic:** The issue's parent, when the parent is an epic. A subtask's parent is a story, so a subtask shows no epic.
- **Touch:** Swiping and tapping are a supplement only, until I confirm the SDK supports touch.

## Ideas, not built

- **Acting on issues:** An `ActionBarLayer` in the issue view, with Up moving to the next status, Select opening a menu
  of transitions, and Down adding a comment by dictation or canned reply ("On it", "Blocked", "Ready for review"). I'm
  not sure these fit a sprint review, which is mostly reading.
- **Glance:** A summary of the sprint, such as "3 in progress · 1 blocked".
- **Vibration:** A tap when the sprint holds an issue it didn't hold the last time the app opened.
- **Optional watchface:** The time, the number of issues in progress and the days left in the sprint, with an accent
  color that turns red near the end of the sprint.

## Still to confirm

- Whether dictation works. PebbleKit JS runs on a real phone with the Core app; dictation is untried.
- Whether the Time 2 SDK exposes the touchscreen.

## Emulator tips

- `pebble emu-app-config --emulator emery` opens the config page. The app must be installed and open first, and the
  command must keep running until you press Save.
- A fresh emulator has no apps, so run `pebble install --emulator emery` after starting one.

## Installing on the watch

- In the Pebble app on the phone, open the watch's ⋯ menu in the *Devices* tab and turn on *Dev Connection*.
- Run `pebble login` once, then `pebble install --cloudpebble`. Or, on the same Wi-Fi, turn on *Use LAN developer
  connection* under *Settings → Phone → Connectivity* and run `pebble install --phone <IP>`. The LAN connection has no
  authentication, so use it only on a trusted network.
- Allow the Internet permissions when the phone asks.
- The phone keeps its own settings, apart from the emulator's. Enter them through the app's gear in the Pebble app.
