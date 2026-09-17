# ============================================================================
# Overhead Intelligence build configuration overrides
#
# The root CMakeLists.txt includes this file (before project()) whenever the
# custom/ directory exists. Everything here only *renames and rebrands*; the
# feature set stays stock QGC with every firmware plugin enabled (OI flies
# ArduPilot, so the APM plugin must stay on).
# ============================================================================

# ----------------------------------------------------------------------------
# Application identity
#
# QGC_APP_NAME is used for the executable (QGroundControl-OI.exe), the NSIS
# installer (QGroundControl-OI-installer-AMD64.exe), the QSettings file and the
# Documents folder. Keep it free of spaces: the installer passes it through
# NSIS /D defines. The CI workflow reads the name back from the CMake cache,
# so renaming here is enough.
# ----------------------------------------------------------------------------
set(QGC_APP_NAME "QGroundControl-OI" CACHE STRING "App Name" FORCE)
set(QGC_ORG_NAME "Overhead Intelligence" CACHE STRING "Organization name" FORCE)
set(QGC_ORG_DOMAIN "overheadintel.com" CACHE STRING "Organization domain" FORCE)
set(QGC_APP_DESCRIPTION "Overhead Intelligence build of QGroundControl" CACHE STRING "Application description" FORCE)

string(TIMESTAMP _oi_copyright_year "%Y")
set(QGC_APP_COPYRIGHT "Copyright (c) ${_oi_copyright_year} Overhead Intelligence. QGroundControl is Copyright (c) QGroundControl Project. All rights reserved." CACHE STRING "Copyright notice" FORCE)

# ----------------------------------------------------------------------------
# Icons and installer artwork (generated from the OI brand kit, see custom/README.md)
# ----------------------------------------------------------------------------

# Linux AppImage icon (not shipped by OI, kept so a Linux build still brands correctly)
if(EXISTS "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/res/icons/oi-qgroundcontrol.svg")
    set(QGC_APPIMAGE_ICON_SCALABLE_PATH "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/res/icons/oi-qgroundcontrol.svg" CACHE FILEPATH "AppImage Icon SVG Path" FORCE)
endif()

# Windows installer header (150x57 BMP shown at the top of every installer page)
if(EXISTS "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/deploy/windows/installheader.bmp")
    set(QGC_WINDOWS_INSTALL_HEADER_PATH "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/deploy/windows/installheader.bmp" CACHE FILEPATH "Windows Install Header Path" FORCE)
endif()

# Windows application icon (exe, taskbar, installer)
if(EXISTS "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/deploy/windows/WindowsQGC.ico")
    set(QGC_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/deploy/windows/WindowsQGC.ico" CACHE FILEPATH "Windows Icon Path" FORCE)
endif()
