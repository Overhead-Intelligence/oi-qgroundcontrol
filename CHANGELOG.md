# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Versions are OI's own (`vX.Y.Z`). Each release notes the upstream QGroundControl version it is built on.

## [Unreleased]

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
