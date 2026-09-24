# ============================================================================
# OI.Settings QML module
#
# The generated settings pages (src/AppSettings/pages/*.SettingsUI.json ->
# MapsSettings.qml) emit their section components as bare type names, resolved
# from the page's own module scope. A loose file in custom.qrc belongs to no
# module and cannot be named that way, so the Map Overlays section lives in a
# real module, which Maps.SettingsUI.json pulls in through its "imports" key.
#
# This module is declared here rather than in custom/CMakeLists.txt on purpose.
# The root CMakeLists calls qt_standard_project_setup(), which turns on
# CMAKE_INCLUDE_CURRENT_DIR, so every target gets its own source directory on
# the include path - and custom/ holds VERSION, which MSVC would then resolve
# for #include <version> (Windows filenames are case insensitive). Any target
# defined directly in custom/ breaks the build that way. custom/src/qml/ has no
# such file. See the VERSION note in CLAUDE.md.
# ============================================================================

qt_add_library(OISettingsModule STATIC)

qgc_set_qt_resource_alias(
    ${CMAKE_CURRENT_SOURCE_DIR}/OIMapOverlaySettings.qml
)

qt_add_qml_module(OISettingsModule
    URI OI.Settings
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/OIMapOverlaySettings.qml
    NO_PLUGIN
)
