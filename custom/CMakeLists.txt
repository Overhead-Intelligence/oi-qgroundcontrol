# ============================================================================
# Overhead Intelligence custom build (QGC 5.0 line)
#
# Added as a subdirectory by the root CMakeLists.txt when custom/ exists.
# Registers the OI plugin sources and resources with the main target.
# ============================================================================

message(STATUS "QGC: Adding Overhead Intelligence custom build (custom/)")

# ----------------------------------------------------------------------------
# Tell QGCCorePlugin::instance() which class is the plugin.
# ----------------------------------------------------------------------------
set_property(DIRECTORY ${CMAKE_SOURCE_DIR}
    APPEND PROPERTY COMPILE_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="OIPlugin.h"
    CUSTOMCLASS=OIPlugin
)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIKeyboardController.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIKeyboardController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIKeyboardSettings.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OIKeyboardSettings.h
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_LIBRARIES
    ""
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    CACHE INTERNAL "" FORCE
)
