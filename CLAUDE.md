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

Current version: 1.0.1 (released 2026-09-17). Upstream base: QGroundControl
**v5.1.4** (`main` and `development` both started from tag `v5.1.4`).
The QGC line is Roger's decision, not a sync: on 2026-09-17 he weighed going
back to the 5.0 line (he dislikes parts of the 5.1 interface) and chose to
stay on 5.1.x for its features, with the interface changed through `custom/`
overrides as he names things. A complete 5.0 port is parked on branch
`wip/base-5.0-fallback` (build unverified). Ask him before moving the base.

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
  `res/OI-Actions.json`), `createQmlApplicationEngine` (URL interceptor),
  `showInitialSetupVehiclePreferences` / `showInitialSetupMeasurementUnits`
  (both false: no first-run Preferences prompt), `customMapItems`
  (hazard overlay markers).
  The constructor runs `_importLegacySettings`: once per settings file, if the
  file is fresh, it copies the `TelemetryBarUserSettings-*`, `LinkConfigurations`,
  `Units`, `Video`, `FlyView` and `FlightMapPosition` groups from
  `%APPDATA%\QGroundControl\QGroundControl OI Build.ini` (else `QGroundControl.ini`)
  and records the source under `OI/importedSettingsFrom`.
- `custom/src/qml/QGCToolBarButton.qml` — the stock control with the logo tinted
  to the theme so the monochrome OI mark works in light and dark palettes.
- `custom/src/OIMapOverlays.{h,cc}` — hazard overlays in two formats.
  `OIMapOverlayLayer` is one imported file; the format is **sniffed, not taken from
  the extension** (`<kml` / `<?xml` in the first 4 KB means KML, otherwise FAA DOF),
  because a DOF arrives as `.Dat`, `.DAT` or `.dat`. KML is parsed with
  `QXmlStreamReader` rather than `KMLHelper`, which drops the Placemark `<name>`
  and errors out on a KML holding no `<Point>`. The DOF reader is fixed-width,
  latin-1, CRLF; the column slices are the `kDof*` constants, verified against
  `12-FL.Dat`. `OIMapOverlayItem` is one marker (a `QmlComponentInfo`, as
  `customMapItems()` documents). `OIMapOverlayManager` owns both models, the
  QSettings persistence under `OI/MapOverlays`, and is the `OIMapOverlays`
  singleton in `OI.Controls`. Markers render **Fly view only**: `CustomMapItems.qml`
  is instantiated once, in `FlyViewMap.qml`.
- `custom/src/qml/OIMapOverlayMarker.qml` — the marker, loaded by URL from
  `custom.qrc`, modelled on `ADSBVehicleMapItem.qml`.
  `custom/src/qml/OIMapOverlaySettings.qml` — the Settings → Maps section. It has
  to be a *type* in a QML module, because the generated settings page names its
  section components by bare type name; hence `custom/src/qml/CMakeLists.txt` and
  the `"imports": ["OI.Settings"]` key in `src/AppSettings/pages/Maps.SettingsUI.json`
  — the only `src/` change the overlay feature makes.
- **Why the overlay filters exist, and why the cap refuses rather than truncates.**
  One DOF is a whole state: Florida is 43,893 obstacles, 13,725 of them utility
  poles, and every marker costs a QObject plus a QML `MapQuickItem`. Each layer
  carries a minimum AGL height and a radius. The radius is anchored on the active
  vehicle's home, falling back to `QGroundControlQmlGlobal::flightMapPosition()`,
  and the manager rebuilds on `activeVehicleChanged` and `homePositionChanged`.
  Height alone does **not** work — Florida still has 4,734 obstacles over 200 ft,
  far past `kMaxMarkers`. Over that cap the manager draws **nothing** and says so.
  Do not "fix" that by truncating to the first N: a partial hazard overlay looks
  complete, which is more dangerous than an absent one.
- `custom/res/` — logo mark SVG, icons, `OI-defaults.ini`, `OI-Actions.json`.
  `custom/deploy/windows/` — installer icon and header.
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
- `custom/VERSION` + `CHANGELOG.md` drive releases. The version file cannot
  sit at the repo root: MSVC resolves `#include <version>` (C++20 standard
  header) to a root file named `VERSION` because the root is on the include
  path and Windows ignores case, which breaks the build. The
  `oi-developer-workspace` release tools expect a root `VERSION`, so until
  they learn this location (issue #9) releases are cut by hand as described
  in the README. CI attaches the installer to the GitHub release of the tag.
- CI (about 30 min cold, less once the shared ccache is warm) is the compile
  check for every PR; download the artifact to test. For iteration, build
  locally: `custom/scripts/install-qt.cmd` (once), `build-local.cmd`,
  `run-local.cmd` (VS 2022 Build Tools with the C++ workload, Python 3, Qt from
  `.github/build-config.json` into `.qt/`). Non-tag builds run as
  "QGroundControl-OI Daily" with separate settings; only tag builds set
  `QGC_STABLE_BUILD`.
- QGC's "new version available" check is disabled for custom builds; the
  download location shown in the app is the GitHub Releases page.

## Upstream sync

`git remote add upstream https://github.com/mavlink/qgroundcontrol.git`,
`git fetch upstream --tags`, branch `chore/upstream-vX.Y.Z` off `development`,
`git merge vX.Y.Z`, keep ours for `README.md`, `CHANGELOG.md` and `.github/`,
open a PR. Never merge `upstream/master` (daily builds).

## Gotchas

- The GCS system ID stays at QGC's default 255 (Roger's live setting on
  2026-09-16; the May export had 254). ArduPilot gates `MANUAL_CONTROL`, RC
  override and the GCS failsafe on `MAV_GCS_SYSID`; guided commands are not gated.
- `QGC_APP_NAME` with spaces breaks the NSIS `/D` defines; keep it hyphenated.
- The org ruleset protects the repository's *default* branch; `main` also has a
  repo-level ruleset (PR required, merge commits only, no deletion, no force
  push). The default branch must be `main`; only an org admin can switch it
  (`master` is the stale upstream default left over from forking).
- `git describe` decides the app version: a tag build reports `X.Y.Z`; anything
  else reports the nearest tag plus a suffix and a `0.0.0` fallback. Push tags
  with the release, never by hand on a topic branch.
- The repo deletes a PR's head branch on merge. A release PR's head is
  `development`, so `development` has its own ruleset (no deletion, no force
  push) to survive that; do not remove it. GitHub also closes any open PR
  whose base branch is deleted, so retarget stacked PRs to `development`
  before merging the PR they were stacked on.
- Do not put a file named `VERSION` (any case) at the repo root or in any
  include directory: MSVC resolves `#include <version>` to it. This also means
  **no CMake target may be declared in `custom/`** — the root calls
  `qt_standard_project_setup()`, which turns on `CMAKE_INCLUDE_CURRENT_DIR`, so a
  target there puts `custom/VERSION` on its own include path. Declare QML modules
  in a subdirectory (see `custom/src/qml/CMakeLists.txt`) and register them
  through `CUSTOM_LIBRARIES`, which `src/CMakeLists.txt` links once the main
  target exists.
- Local Qt installs need the aqtinstall commit pinned in
  `.github/workflows/oi-windows.yml` (`AQT_SOURCE`); the released aqtinstall
  does not know the Qt 6.11 repository layout. `custom/scripts/install-qt.cmd`
  does this.
