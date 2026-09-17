# custom/ — everything Overhead Intelligence adds to QGroundControl

QGroundControl looks for a `custom/` directory when it configures and, when one
exists, produces a "custom build": this directory can rename the app, swap
images, override QML files and hook C++ behaviour without touching `src/`.
Upstream documents the mechanism in `custom-example/`; this directory is OI's
version of it.

| Path | What it does | Change it when |
|---|---|---|
| `cmake/CustomOverrides.cmake` | App name (`QGroundControl-OI`), org name, description, icon paths | Renaming the app or swapping icons |
| `CMakeLists.txt` | Lists the OI C++ sources and resources | Adding a C++ file |
| `custom.qrc` | Resources compiled into the exe: images, `OI-defaults.ini`, `OI-Actions.json`, QML overrides | Adding an image, a data file or a QML override |
| `res/OI-defaults.ini` | Default settings for a fresh install | Changing a default (units, guided limits, hidden modes, ...) |
| `res/OI-Actions.json` | Fly view custom action buttons | Adding or changing a MAVLink action |
| `res/Images/OILogoMark.svg` | The OI logo mark as a vector, used for the toolbar logo and menus | Rebranding |
| `res/icons/`, `deploy/windows/` | App icon, installer icon and installer header | Rebranding |
| `src/OIPlugin.{h,cc}` | The QGC core plugin: applies the defaults, builds the telemetry bar, deploys the actions file, installs the override interceptor | Adding a new hook |
| `src/qml/QGCToolBarButton.qml`, `src/qml/SelectViewDropdown.qml` | Stock controls with the logo swapped for the OI mark and tinted to the theme | Rarely |
| `src/OIKeyboardController.{h,cc}` | Keyboard guided control: key handling, GUIDED commands, safety gates | Changing what a key does |
| `src/OIKeyboardSettings.{h,cc}`, `res/json/OIKeyboard.SettingsGroup.json` | Its tunables (altitude step, turn rate, bank limit, gimbal rate) and their defaults | Changing a default step or rate |
| `src/qml/FlyViewCustomLayer.qml` | The Fly view overlay: keyboard switch, status line, settings | Changing the panel |
| `ardupilot-scripts/` | Lua scripts that belong on the aircraft, kept next to the GCS feature that needs them | Changing aircraft-side behaviour |
| `VERSION` | The OI release version (kept here, not at the repo root, because a root `VERSION` shadows the C++ `<version>` header on Windows) | Cutting a release |

## First start: your previous settings are imported

The first time a fresh settings file is used, `OIPlugin::_importLegacySettings`
copies the telemetry bar, the link list, units, video, Fly view and map position
from the previous OI build's file (`%APPDATA%\QGroundControl\QGroundControl OI Build.ini`),
or from stock QGC's `QGroundControl.ini` if that is all there is. It runs once
(the source is recorded under `OI/importedSettingsFrom` in the new file) and never
overwrites settings that already exist. Flight-mode and gimbal settings are not
imported because their keys changed in QGC 5.1; the OI defaults cover them.

Roger's reference settings (the source of the defaults below) are backed up at
`G:\Shared drives\OI-Engineering\Software & Firmware\QGC\settings-backups\`.

## Changing a default setting

1. Find the setting name in `src/Settings/<Group>.SettingsGroup.json` (for example
   `guidedMaximumAltitude` in `FlyView.SettingsGroup.json`).
2. Put `name=value` under the `[<Group>]` section of `res/OI-defaults.ini`.
   App and MAVLink settings have an empty group name in QGC and go under
   `[General]`. Quote values that contain commas.
3. Rebuild. Only the *default* changes: an operator who already saved a
   different value keeps it, and "Reset to defaults" in the app returns to
   the OI value.

Values are raw QGC units (metres, metres per second) regardless of the units
shown in the app.

## Adding a custom action

Edit `res/OI-Actions.json`. The format is QGC's MavlinkActions file: `label`,
`description`, `mavCmd`, optional `compId` and `param1` to `param7`. At every
start the plugin copies the file to `Documents\QGroundControl-OI\MavlinkActions\OI-Actions.json`
and overwrites the copy, so local edits to that copy are lost; keep local
experiments in a file with a different name and select it under
Application Settings > Fly View > Custom actions.

## Overriding a stock QML file

1. Copy `src/<Module>/<File>.qml` to `custom/src/qml/<File>.qml` and edit it.
2. Add it to `custom.qrc` under the `/Custom/qml` prefix with the alias
   `QGroundControl/<Module>/<File>.qml`, where `<Module>` is the QML module URI
   without the `QGroundControl.` prefix (`QGroundControl.Controls` becomes
   `QGroundControl/Controls`, `QGroundControl.FlyView` becomes `QGroundControl/FlyView`).
3. The interceptor in `OIPlugin.cc` redirects the stock URL to the override at
   runtime. Images work the same way under the `/Custom/res` prefix.

## Brand assets

The mark, icons and installer header were generated from the OI brand kit
(`G:\Shared drives\OI-Marketing\Logo\OI.FinalLogos`). `OILogoMark.svg` is a
vector reconstruction of the icon lockup (ring radii, bar width and the four
slots measured from the master PNG) so it stays crisp at every size.
