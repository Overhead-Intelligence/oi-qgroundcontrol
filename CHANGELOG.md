# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Versions are OI's own (`vX.Y.Z`). Each release notes the upstream QGroundControl version it is built on.

## [Unreleased]

### Added
- "Onboard Files" page in the Analyze view: a MAVLink FTP browser for the vehicle's onboard storage. Navigate folders, upload a file into the folder you are looking at, download or delete a selected file, and cancel a transfer in progress. Useful for checking Lua scripts, terrain data and the log folder without a second tool. The page requires a connected vehicle and says so when there is none. All transfer work is upstream's `FTPController`; the OI part is the browser UI and its registration through `QGCCorePlugin::analyzePages()`.
- The Onboard Files page asks before starting a download of 1 MB or more, naming the size and warning that cancelling will not stop the vehicle sending. Over a telemetry link MAVLink FTP moves a few KB/s, so a 2 MB log is a ten-minute transfer.
- The page now says what it is doing while it is busy - listing, downloading with a percentage, uploading, deleting, or cancelling - instead of disabling its controls with no explanation, which read as a hang. A cancelled download is reported as cancelled rather than as a failure, with a warning that the vehicle keeps sending for a few seconds afterwards: ArduPilot streams a burst read from a blocking loop (`GCS_FTP.cpp`) and will not service a TerminateSession until that loop ends, so the transfer cannot actually be called off mid-burst.

### Changed
- PX4 is no longer offered as a firmware choice. `QGC_DISABLE_PX4_PLUGIN_FACTORY` drops PX4 from the firmware set QGC advertises, which hides the PX4 log transfer settings page, the PX4 entries in the firmware upgrade picker and the PX4 options in MockLink. The fleet is ArduPilot only, so this is UI clutter removal; the PX4 setup pages under `src/AutoPilotPlugins/PX4/` still compile in but were already unreachable without a PX4 vehicle.

### Removed
- Keyboard guided control, in full: the Fly view panel and its `FlyViewCustomLayer.qml` override, `OIKeyboardController`, `OIKeyboardSettings`, the `OIKeyboard` settings group, the `OI.Controls` QML singleton, the `mavlinkMessage` hook that fed it `NAV_CONTROLLER_OUTPUT`, and the aircraft-side watchdog `custom/ardupilot-scripts/heading_hold_timeout.lua`. The feature was never flown or run against SITL (issue #5), and its panel rendered mid-screen the moment any vehicle connected, because it positioned itself from `bottomEdgeCenterInset`, which upstream aliases to the bottom-*right* inset — the full height of the Large Vertical instrument panel that the OI defaults select. That made the build inadvisable for flight operations and blocked further Fly view work. Shipped in 1.0.0 and 1.0.1; the history is kept on `development` so the feature can be reworked and brought back deliberately.

### Notes
- Removing the watchdog from this repo does not uninstall it from aircraft that already have it. It is harmless there: it only arms on receiving `MAV_CMD_GUIDED_CHANGE_HEADING` (43002), which this build no longer sends. Delete it from `APM/scripts/` at the next bird maintenance. Its Lua parameter table key 107 is retired and must not be reused.

## [1.0.1] - 2026-09-17

Built on upstream QGroundControl v5.1.4.

### Added
- Local build scripts for contributors under `custom/scripts/`: `install-qt.cmd` installs the Qt version from `.github/build-config.json` into `.qt/` once, `build-local.cmd` configures and builds with the VS 2022 Build Tools, `run-local.cmd` starts the result. A rebuild after a change in `custom/` takes about 2 minutes instead of a CI round trip.

### Changed
- No first-run "Preferences" prompt (vehicle type, measurement units) on a fresh install: the OI defaults already answer it.
- The hidden ArduPlane flight-mode list carries both QGC's mixed-case names and the upper-case names an ArduPilot 4.6+ aircraft reports, so the trimmed mode list holds whether or not a vehicle is connected.
- Decision record: the fork stays on the QGC 5.1 line (Roger, 2026-09-17) for its features; UI complaints are handled as overrides in `custom/`. A complete port to the 5.0 line is parked on branch `wip/base-5.0-fallback` (build unverified).

## [1.0.0] - 2026-09-17

First OI release. Built on upstream QGroundControl v5.1.4.

### Added
- Keyboard guided control for ArduPlane in GUIDED mode, behind a "Keyboard" switch at the bottom of the Fly view (off at every start, off on Esc, off when the vehicle changes). W/S bump the target altitude by a step (default 15 m, clamped to the guided min/max altitude settings), A/D fly a standard-rate turn (3 deg/s) for as long as the key is held and the aircraft then holds the new heading, arrow keys pan and tilt the gimbal. A "Release" button clears the heading hold. Works without GPS: heading commands use the compass-heading type and altitude changes are relative offsets in the guided target's existing frame; the panel shows the autopilot's reported altitude target and clamps bumps against it. Step and rates are settings in the panel. Keys are ignored while a text field has focus. The aircraft-side watchdog `custom/ardupilot-scripts/heading_hold_timeout.lua` is shipped alongside.
- OI custom build under `custom/` (QGC's custom-build overlay, nothing under `src/` changed): app name `QGroundControl-OI` with its own settings file and Documents folder, "Overhead Intelligence" as the organisation, the OI logo mark (vector) in the toolbar and view menu tinted to the theme, OI window, taskbar and installer icons, and the OI installer header.
- Fleet defaults for a fresh install, read from `custom/res/OI-defaults.ini` by `OIPlugin::adjustSettingMetaData` (taken from Roger's live operator settings, 2026-09-16): metric units, guided limits (50 m floor, 914 m ceiling, 3048 m go-to range, 200 m forward-flight loiter radius), the Large Vertical instrument panel with a nose-up compass and extra indicators, the OI hidden-mode list (FBW A, FBW B, Auto, RTL, Loiter, Guided and the QuadPlane Hover, Loiter, Land and RTL modes stay visible), gimbal on-screen control with an 80x65 degree camera FOV, and ArduPilot fixed wing at 21 m/s for offline plans. Operators keep any value they change; "Reset to defaults" returns to these.
- The OI telemetry bar as the default layout (8 columns x 2 rows: Alt (Rel), Distance to Home, Climb Rate, Ground Speed, AirSpd, Thr, Flight Time, Flight Distance, Alt (Above Terrain), Voltage, Wind Direction, Wind Spd, MGRS Position, Mission Item Index, rangefinder Down and Forward).
- One-time import of an operator's existing settings into a fresh settings file: telemetry bar, link list, units, video, Fly view and map position are copied from the previous OI build's settings (`%APPDATA%\QGroundControl\QGroundControl OI Build.ini`), or from stock QGC's file, on the first start. Nobody rebuilds their telemetry bar or fleet links after installing this build.
- OI custom actions (`custom/res/OI-Actions.json`: Wingtip Lights ON/OFF, Gripper Release/Grab, PosXY GPS Enable/Disable) copied into the MavlinkActions folder at every start and selected as the Fly view actions file.
- Repository bootstrap on upstream QGroundControl v5.1.4: OI `README.md` and `CLAUDE.md`, PR template, `CODEOWNERS`, and the `oi-windows.yml` workflow that builds the Windows x64 installer on every PR and push and attaches it to tagged GitHub releases.

### Removed
- Upstream QGroundControl CI workflows (Android, iOS, macOS, Linux, docs, CodeQL, ...) and bot configuration (Dependabot, Renovate, labeler, Copilot). OI ships Windows only; the reusable actions under `.github/actions` are kept because the OI workflow uses them.
