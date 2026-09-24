# ============================================================================
# Overhead Intelligence custom build
#
# Added as a subdirectory by the root CMakeLists.txt when custom/ exists.
# Registers the OI plugin sources and resources with the main target. The
# variables below are read by src/CMakeLists.txt (see "QGC_CUSTOM_BUILD").
# ============================================================================

message(STATUS "QGC: Adding Overhead Intelligence custom build (custom/)")

# ----------------------------------------------------------------------------
# Resources compiled into the executable (images, defaults, actions, QML overrides)
# ----------------------------------------------------------------------------
list(APPEND CUSTOM_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc
)
set(QGC_RESOURCES ${QGC_RESOURCES} ${CUSTOM_RESOURCES} CACHE STRING "Paths to .qrc Resources" FORCE)

# ----------------------------------------------------------------------------
# C++ sources. Add new files here (headers too, so moc runs on Q_OBJECT classes).
# ----------------------------------------------------------------------------
set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIMapOverlays.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIMapOverlays.h
    CACHE INTERNAL "" FORCE
)

# src/CMakeLists.txt links these into the main target once it exists; custom/ is
# added as a subdirectory before that, so linking directly here would fail.
set(CUSTOM_LIBRARIES
    OISettingsModule
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    CACHE INTERNAL "" FORCE
)

# CUSTOMCLASS / CUSTOMHEADER tell QGCCorePlugin::instance() which class is the plugin.
set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="OIPlugin.h"
    CUSTOMCLASS=OIPlugin
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_QT_COMPONENTS
    Core
    CACHE INTERNAL "" FORCE
)

# The OI.Settings QML module is declared in its own subdirectory: a target defined
# here would put custom/ (and so custom/VERSION) on its include path, which breaks
# MSVC's #include <version>. See custom/src/qml/CMakeLists.txt.
add_subdirectory(src/qml)
