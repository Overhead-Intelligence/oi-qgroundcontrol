# OI QGroundControl

Overhead Intelligence's build of [QGroundControl](https://github.com/mavlink/qgroundcontrol), the ground station we fly the OI fleet with (ArduPlane QuadPlanes on Cube Orange). It is stock QGC plus what every OI laptop needs on day one:

- **OI branding**: logo, icons, installer, and the app name `QGroundControl-OI`. It installs beside stock QGC and keeps its own settings.
- **Fleet defaults on first launch**: metric units, the OI telemetry bar (8 columns, rangefinder included), guided-mode limits (50 m floor, 914 m ceiling, 3048 m go-to range, 200 m forward-flight loiter radius), the trimmed ArduPlane flight-mode list, gimbal on-screen control.
- **Your existing settings come along**: on its first start the build imports the telemetry bar, fleet links, units, video and Fly view settings from the previous OI build (or from stock QGC). Nobody rebuilds a telemetry bar after installing.
- **OI custom actions** in the Fly view: wingtip lights, gripper, EK3 PosXY source. Loaded automatically.
- **Hazard overlays**: import an FAA obstacle file (`.Dat`) or a KML and see the obstacles on the Fly view map, filtered by height and by distance from where you are flying. Settings -> Maps -> Map Overlays.

Current version: 1.0.1 (2026-09-17). Built on upstream QGroundControl **v5.1.4**.

## Download

Go to [Releases](https://github.com/Overhead-Intelligence/oi-qgroundcontrol/releases) and download `QGroundControl-OI-installer-AMD64.exe` (Windows 10/11, 64-bit). Run it, then start **QGroundControl-OI** from the Start menu.

Settings live in `%APPDATA%\Overhead Intelligence\QGroundControl-OI.ini`; missions, logs and custom actions in `Documents\QGroundControl-OI`. A stock QGroundControl install is not touched.

Every pull request and every push to `development` also produces an installer: open the run on the [Actions tab](https://github.com/Overhead-Intelligence/oi-qgroundcontrol/actions) and download the `QGroundControl-OI-installer-AMD64` artifact. Those are development builds: they run as "QGroundControl-OI Daily" with their own settings, so they never disturb a release install.

## What is in this fork

All OI code lives in [`custom/`](custom/README.md), QGC's supported custom-build overlay, so upstream releases merge cleanly. The single exception is 15 lines of declarative JSON in `src/AppSettings/pages/Maps.SettingsUI.json`, which registers the Map Overlays settings section; QGC generates its settings pages from those files and gives a custom build no other way in.

| Area | Where to look |
|---|---|
| Branding (name, icons, installer header, toolbar logo) | `custom/cmake/CustomOverrides.cmake`, `custom/res/`, `custom/deploy/windows/`, `custom/src/qml/QGCToolBarButton.qml` |
| Default settings on first launch | `custom/res/OI-defaults.ini` (change a value there, rebuild, done) |
| Telemetry bar layout | `custom/src/OIPlugin.cc`, `factValueGridCreateDefaultSettings` |
| Import of an operator's previous settings | `custom/src/OIPlugin.cc`, `_importLegacySettings` (runs once per settings file) |
| Custom actions | `custom/res/OI-*.json`, one file per capability (copied to `Documents\QGroundControl-OI\MavlinkActions` at startup; tick the ones a bird has under Fly View Settings) |
| Hazard overlays (FAA DOF + KML import, filters, map markers) | `custom/src/OIMapOverlays.{h,cc}`, `custom/src/qml/OIMapOverlay*.qml` |
| CI and releases | `.github/workflows/oi-windows.yml` |

Defaults are only defaults: an operator can still change any setting in the app, and "Reset to defaults" comes back to the OI values.

## Working on this repo

Same flow as `oi-raspi-toolkit`:

1. `main` is what has been released. `development` is where work lands. Nobody pushes to either directly.
2. Branch off `development` (`feat/...`, `fix/...`, `docs/...`, `chore/...`), commit with Conventional Commits, add a line to `CHANGELOG.md` under `[Unreleased]`, open a PR against `development`. CI builds the installer (about 25 minutes) and attaches it to the run. A human reviews and merges (merge commit).
3. Release: a `chore/vX.Y.Z-release-finalize` PR promotes `[Unreleased]` to `[X.Y.Z]` and writes `custom/VERSION`. After it merges, `main` is fast-forwarded to `development`, the `vX.Y.Z` tag is pushed, and CI attaches the installer to the GitHub release. The OI developer workspace tools (`start-release.cmd`, `finish-release.cmd`) expect the version file at the repo root, which this repo cannot have (a root `VERSION` shadows the C++ `<version>` header on Windows), so these steps are done by hand until the tools learn the new location.

No local Qt toolchain is needed to contribute: CI builds every PR. To iterate faster, build on your own Windows PC with the three scripts in `custom/scripts/` (`install-qt.cmd` once, then `build-local.cmd` and `run-local.cmd`); see [custom/README.md](custom/README.md#building-on-your-own-pc-minutes-instead-of-a-ci-run). CMake picks up the `custom/` directory automatically.

### Syncing with upstream

Upstream stable releases are tagged `v5.x.y` on `mavlink/qgroundcontrol`. To take one: `git fetch upstream --tags`, branch `chore/upstream-v5.x.y` off `development`, `git merge v5.x.y`, resolve (conflicts are expected in `README.md`, `CHANGELOG.md` and `.github/`; keep ours), open a PR. Do not merge upstream `master`: those are daily builds.

## License

QGroundControl is dual licensed under Apache 2.0 and GPLv3; see [COPYING.md](COPYING.md). OI's additions under `custom/` are offered under the same terms.
