# ============================================================================
# Overhead Intelligence build configuration overrides (QGC 5.0 line)
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
# installer, the QSettings file and the Documents folder. Keep it free of
# spaces: the installer passes it through NSIS /D defines. The CI workflow
# reads the name back from the CMake cache, so renaming here is enough.
# ----------------------------------------------------------------------------
set(QGC_APP_NAME "QGroundControl-OI" CACHE STRING "App Name" FORCE)
set(QGC_ORG_NAME "Overhead Intelligence" CACHE STRING "Org Name" FORCE)
set(QGC_ORG_DOMAIN "overheadintel.com" CACHE STRING "Domain" FORCE)
set(QGC_APP_DESCRIPTION "Overhead Intelligence build of QGroundControl" CACHE STRING "Description" FORCE)

string(TIMESTAMP _oi_copyright_year "%Y")
set(QGC_APP_COPYRIGHT "Copyright (c) ${_oi_copyright_year} Overhead Intelligence. QGroundControl is Copyright (c) QGroundControl Project. All rights reserved." CACHE STRING "Copyright" FORCE)

# ----------------------------------------------------------------------------
# Icons and installer artwork (generated from the OI brand kit, see custom/README.md)
# ----------------------------------------------------------------------------

# Linux AppImage icon (not shipped by OI, kept so a Linux build still brands correctly)
if(EXISTS "${CMAKE_SOURCE_DIR}/custom/res/icons/oi-qgroundcontrol.png")
    set(QGC_APPIMAGE_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/res/icons/oi-qgroundcontrol.png" CACHE FILEPATH "AppImage Icon Path" FORCE)
endif()

# Windows installer header (150x57 BMP shown at the top of every installer page)
if(EXISTS "${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp")
    set(QGC_WINDOWS_INSTALL_HEADER_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp" CACHE FILEPATH "Windows Install Header Path" FORCE)
endif()

# Windows application icon (exe, taskbar, installer)
if(EXISTS "${CMAKE_SOURCE_DIR}/custom/deploy/windows/WindowsQGC.ico")
    set(QGC_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/WindowsQGC.ico" CACHE FILEPATH "Windows Icon Path" FORCE)
endif()
