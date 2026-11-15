# Agent instructions

## Project Overview

This is a Pebble smartwatch watchapp (not a watchface) for Jira Cloud. It's written in C, with PebbleKit JS on the
phone, using the Pebble SDK. `NOTES.md` has the goals, the Jira API notes, the constraints and the UI design. Read it
before changing behavior.

## SDK Version and Platforms

- The SDK is 4.44 (`pebble sdk list` shows the installed versions).
- The app targets **emery (Pebble Time 2)** only: 200x228, 64 colors, rectangular. It's set in `targetPlatforms` in
  `package.json`. Add other platforms only when asked.
- Alloy runs only on emery and gabbro. This project uses C.
- The timeline web API is gone. The Pebble mobile app doesn't sync pins from a server, so use local pins instead.

## Commands

```bash
# Build the app for all target platforms
pebble build

# Clean build artifacts
pebble clean

# Install the app on the emulator
pebble install --emulator emery

# Screenshot the running emulator
pebble screenshot --no-open --emulator emery screenshot.png

# Show APP_LOG() output from the running app
pebble logs --emulator emery
```

To learn more about `pebble` or one of its subcommands, add `--help`.

`pebble screenshot` doesn't create directories, so create the output directory before you take a screenshot into it.

If the emulator gets into a strange state, such as a screenshot that shows a different app or an install that times
out, run `pebble kill` to stop it and `pebble wipe` to reset its storage. Then install again.

### Headless Environments

In an environment without a window server (such as headless Linux, Docker or CI), add `--vnc` to **every command that
uses the emulator**. That includes app installs, screenshots, button presses and any `emu-*` commands:

```bash
pebble install --emulator emery --vnc
pebble screenshot --vnc --no-open --emulator emery screenshot.png
pebble emu-button --emulator emery --vnc click select
```

The `--vnc` flag uses a VNC display backend, which doesn't need X11.

## Project Structure

```
src/c/main.c         - The watch app: UI and input only, with no network access
src/pkjs/index.js    - PebbleKit JS entry point: AppMessage handling and Clay config events
src/pkjs/jira.js     - The Jira Cloud REST client (XMLHttpRequest, basic auth, its own timeout)
src/pkjs/settings.js - Loads the Clay settings (site URL, email, API token) from localStorage
src/pkjs/config.js   - The Clay config page definition
package.json         - App metadata, targetPlatforms, messageKeys, capabilities
wscript              - The build script
```

## Architecture

```
Watch (C app)  <-Bluetooth/AppMessage->  Phone (PebbleKit JS)  <-HTTPS->  Jira Cloud REST API
```

- The watch app never talks to the network. PebbleKit JS calls Jira and sends small AppMessage dictionaries to the
  watch.
- Each AppMessage key must be listed in `messageKeys` in `package.json`. C code refers to it as `MESSAGE_KEY_<Name>`.
- The credentials stay on the phone. `index.js` handles Clay's events itself (`autoHandleEvents: false`) so they're
  never sent to the watch.
- PebbleKit JS runs only while the app is open, so refresh when the app opens.

## SDK Documentation

The full Pebble SDK documentation is at https://developer.repebble.com.

https://developer.repebble.com/llms.txt indexes every page. Each page also has a Markdown version: replace the
trailing `/` of a page URL with `.md` (for example `https://developer.repebble.com/guides/events-and-services/buttons.md`).
Fetch the `.md` version of a page before you use an API.

https://developer.repebble.com/api-index.json lists every C and PebbleKit JS symbol, with its signature, the
platforms it supports and the page that documents it. Look a function up there instead of searching the reference
pages.

Larger files, for reading a whole section at once:

- https://developer.repebble.com/docs/c/llms-full.txt - the C API reference
- https://developer.repebble.com/docs/pebblekit-js/llms-full.txt - the PebbleKit JS reference
- https://developer.repebble.com/guides/llms-full.txt - all guides

Main categories:

- Tutorials: step-by-step lessons (a C watchface tutorial in 5 parts, plus advanced topics)
- Developer Guides: reference material, organized by topic

Key sections:

- App Resources: images, fonts, vector graphics, the limit of 256 resources
- User Interfaces: the layer hierarchy, TextLayer, MenuLayer, round and rectangular displays
- Events & Services: buttons, accelerometer, compass, health data, background workers
- Communication: Bluetooth AppMessage, PebbleKit JS, Android and iOS integration
- Graphics & Animations: the drawing APIs, property animations, vector graphics
- Debugging: app logs, GDB, common errors and how to fix them
- Best Practices: support for many platforms, saving battery, modular architecture
- Design & Interaction: glance-first design, one-click actions, platform guidelines
- App Store Publishing: submission requirements, assets, analytics

## Development Best Practices

- After every change, build, install it on the emery emulator, run
  `pebble screenshot --no-open --emulator emery screenshot.png` and look at the screenshot to make sure it's what the
  user asked for. If it isn't, make more changes until it is.
- To build a new watchface or watchapp from a description, use the `pebble-watchface` skill in
  `.claude/skills/pebble-watchface/`.

## Emulator Button Control

Use `pebble emu-button` to press emulator buttons from the command line:

```bash
# Click a button (press and release)
pebble emu-button click select --emulator emery

# Long press (for example, 2 seconds to exit the app)
pebble emu-button click back --duration 2000 --emulator emery

# Click again and again (for example, scroll down 5 times)
pebble emu-button click down --repeat 5 --emulator emery

# Click again and again, faster
pebble emu-button click up --repeat 3 --interval 100 --emulator emery
```

**Actions:**

- `click`: press, then release. Add `--duration` for a long press.
- `push`: hold the button down. Use `release` to let go.
- `release`: release all buttons.

**Buttons:** `back`, `up`, `select`, `down`

**Best practices:**

- Use `click` for normal navigation and selection.
- Use `click --duration 2000` for a long press, for example to exit with the back button.
- To scroll through a menu, use `--repeat` instead of running several commands.
- After you change the UI, take a screenshot to check the result.

## AI Interaction Guidelines

- If you're given an image of a screen to copy, describe it in precise detail. Note every visual element, with its
  size, alignment, font weight, spacing and position.

## AI Code Review Guidelines

- When you think you've done what the user asked, check the latest screenshot for problems and for differences from
  the reference image or the user's description. Fix any you find.
