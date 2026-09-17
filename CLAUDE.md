# CLAUDE.md — oi-qgroundcontrol

> OI-wide context (system map, cross-repo contracts, naming) is the
> `oi-system-architecture` skill and the company `CLAUDE.md` in
> `oi-developer-workspace`; branch, commit and release conventions are the
> `oi-git-workflow` skill. This file is self-sufficient for work inside this
> repository.

## What this repo is

Overhead Intelligence's fork of QGroundControl (upstream `mavlink/qgroundcontrol`),
built as a QGC **custom build**: everything OI-specific is in `custom/`, upstream
sources under `src/` are untouched. It ships a Windows x64 installer through
GitHub Releases. Operators fly ArduPlane QuadPlanes (Cube Orange) with it; the
SITL bench is `ardupilot-SITL-environment`.

Current version: 0.0.0 (not released yet). Upstream base: QGroundControl
**v5.1.4** (`main` and `development` both started from tag `v5.1.4`).

## Layout

- `custom/cmake/CustomOverrides.cmake` — app name `QGroundControl-OI`, org name,
  description, icon paths. `QGC_APP_NAME` drives the exe name, the installer
  name, the settings file and the Documents folder; the CI workflow reads it
  back from the CMake cache, so nothing else needs updating.
- `custom/CMakeLists.txt` — registers the OI sources, resources and the plugin
  class (`CUSTOMCLASS=OIPlugin`). `custom/custom.qrc` — resources, including the
  QML overrides under `/Custom/qml/...`: QGC's URL interceptor swaps a stock
  `qrc:/qml/X` for `:/Custom/qml/X` whenever the latter exists.
- `custom/src/OIPlugin.{h,cc}` — the `QGCCorePlugin` subclass. Hooks used:
  `adjustSettingMetaData` (defaults read from `res/OI-defaults.ini`),
  `factValueGridCreateDefaultSettings` (telemetry bar), `init` (deploys
  `res/OI-Actions.json`, creates the keyboard controller, registers the
  `OI.Controls` QML singleton), `createQmlApplicationEngine` (URL interceptor).
- `custom/src/OIKeyboardController.{h,cc}` — application-wide key event filter
  that turns W/S/A/D and the arrow keys into GUIDED commands.
  `custom/src/OIKeyboardSettings.{h,cc}` + `res/json/OIKeyboard.SettingsGroup.json`
  — its tunables (altitude step, turn rate, bank limit, gimbal rate).
- `custom/src/qml/FlyViewCustomLayer.qml` — Fly view overlay (keyboard panel).
  `custom/src/qml/QGCToolBarButton.qml` — the stock control with the logo tinted
  to the theme so the monochrome OI mark works in light and dark palettes.
- `custom/res/` — logo mark SVG, icons, `OI-defaults.ini`, `OI-Actions.json`,
  settings JSON. `custom/deploy/windows/` — installer icon and header.
  `custom/ardupilot-scripts/` — Lua that belongs on the aircraft, not the GCS.
- `.github/workflows/oi-windows.yml` — the only workflow. `.github/actions/*`,
  `.github/scripts/*` and `.github/build-config.json` are upstream's, reused
  unchanged (Qt version, GStreamer version, build steps).

## Conventions

- Keep changes inside `custom/` and `.github/`. Touching `src/` is acceptable
  only when no plugin hook exists; say so in the PR and the CHANGELOG entry,
  because that is what conflicts on the next upstream sync.
- A default setting goes in `custom/res/OI-defaults.ini`: section = QGC settings
  group (`[General]` for the App and MAVLink groups, which have an empty group
  name), key = the fact name from `src/Settings/*.SettingsGroup.json`. Quote
  values that contain commas. Units enums: 0 = feet / ft/s / sq ft, 1 = metres
  / m/s / sq m. The plugin only changes the *default*; a value the operator has
  saved wins.
- Branches `feat/`, `fix/`, `docs/`, `chore/` off `development`; Conventional
  Commits; PR against `development`; a `CHANGELOG.md` `[Unreleased]` entry in
  every PR; humans merge with a merge commit (the ruleset allows nothing else).
  `main` fast-forwards at release time; tags are OI versions `vX.Y.Z`, not QGC's.
- `VERSION` + `CHANGELOG.md` drive releases; the `oi-developer-workspace`
  tools `start-release.cmd` / `finish-release.cmd` work on this repo. CI
  attaches the installer to the GitHub release of the tag.
- No local toolchain is assumed. CI (about 25 min) is the compile check;
  download the artifact to test. Non-tag builds run as "QGroundControl-OI Daily"
  with separate settings; only tag builds set `QGC_STABLE_BUILD`.
- QGC's "new version available" check is disabled for custom builds; the
  download location shown in the app is the GitHub Releases page.

## Keyboard guided control (safety contract)

- Off at every start. Turns itself off when the active vehicle changes or
  disconnects and on Esc. Window deactivation releases every held key.
- Acts only when the active vehicle is ArduPlane (fixed-wing or VTOL) in flight
  mode `Guided`. It never changes the flight mode.
- W/S: `Vehicle::guidedModeChangeAltitude(±altitudeStep)`, clamped by the
  FlyView `guidedMinimumAltitude` / `guidedMaximumAltitude` settings.
- A/D: `MAV_CMD_GUIDED_CHANGE_HEADING` (43002) every 200 ms while held.
  param1 = 1 (HEADING), param2 = target heading slewed at `turnRate` deg/s from
  the heading at key-down, param3 = g·tan(`turnBankLimit`) (ArduPlane turns it
  into a bank ceiling). Releasing the key stops sending; the aircraft holds the
  last target. "Release" sends param1 = 2 (`HEADING_TYPE_DEFAULT`), which clears
  the hold on ArduPlane 4.5 and later.
- Arrow keys: `GimbalController::sendPitchBodyYaw`, integrating at `gimbalRate`
  deg/s from the gimbal's reported angles at key-down.
- Keys are ignored while a `TextInput` / `TextEdit` has focus.
- Aircraft watchdog: `custom/ardupilot-scripts/heading_hold_timeout.lua`
  (parameter `HHT_TIMEOUT`, default 120 s, 0 disables).
- The GCS system ID defaults to 254 (OI convention). ArduPilot gates
  `MANUAL_CONTROL`, RC override and the GCS failsafe on `MAV_GCS_SYSID`; guided
  commands are not gated.

## Upstream sync

`git remote add upstream https://github.com/mavlink/qgroundcontrol.git`,
`git fetch upstream --tags`, branch `chore/upstream-vX.Y.Z` off `development`,
`git merge vX.Y.Z`, keep ours for `README.md`, `CHANGELOG.md` and `.github/`,
open a PR. Never merge `upstream/master` (daily builds).

## Gotchas

- `QGC_APP_NAME` with spaces breaks the NSIS `/D` defines; keep it hyphenated.
- The org ruleset protects the repository's *default* branch; `main` also has a
  repo-level ruleset (PR required, merge commits only, no deletion, no force
  push). The default branch must be `main`; only an org admin can switch it
  (`master` is the stale upstream default left over from forking).
- `git describe` decides the app version: a tag build reports `X.Y.Z`; anything
  else reports the nearest tag plus a suffix and a `0.0.0` fallback. Push tags
  with the release, never by hand on a topic branch.
