# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Versions are OI's own (`vX.Y.Z`). Each release notes the upstream QGroundControl version it is built on.

## [Unreleased]

### Added
- OI custom build under `custom/` (QGC's custom-build overlay, nothing under `src/` changed): app name `QGroundControl-OI` with its own settings file and Documents folder, "Overhead Intelligence" as the organisation, the OI logo mark (vector) in the toolbar and view menu tinted to the theme, OI window, taskbar and installer icons, and the OI installer header.
- Fleet defaults for a fresh install, read from `custom/res/OI-defaults.ini` by `OIPlugin::adjustSettingMetaData`: metric units, guided limits (914 m ceiling, 3048 m go-to range, 152 m forward-flight loiter radius), the Large Vertical instrument panel with a nose-up compass and extra indicators, the trimmed ArduPlane flight-mode list, gimbal on-screen control with an 80x65 degree camera FOV, GCS MAVLink system ID 254, and ArduPilot fixed wing at 21 m/s for offline plans. Operators keep any value they change; "Reset to defaults" returns to these.
- The OI telemetry bar as the default layout (7 columns x 2 rows: Alt (Rel), Distance to Home, Climb Rate, Ground Speed, AirSpd, Thr, Flight Time, Flight Distance, Alt (Above Terrain), Voltage, Wind Direction, Wind Spd, MGRS Position, Mission Item Index).
- OI custom actions (`custom/res/OI-Actions.json`: Wingtip Lights ON/OFF, Gripper Release/Grab, PosXY GPS Enable/Disable) copied into the MavlinkActions folder at every start and selected as the Fly view actions file.
- Repository bootstrap on upstream QGroundControl v5.1.4: OI `README.md` and `CLAUDE.md`, PR template, `CODEOWNERS`, and the `oi-windows.yml` workflow that builds the Windows x64 installer on every PR and push and attaches it to tagged GitHub releases.

### Removed
- Upstream QGroundControl CI workflows (Android, iOS, macOS, Linux, docs, CodeQL, ...) and bot configuration (Dependabot, Renovate, labeler, Copilot). OI ships Windows only; the reusable actions under `.github/actions` are kept because the OI workflow uses them.
