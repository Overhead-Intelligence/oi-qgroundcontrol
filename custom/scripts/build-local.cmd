@echo off
setlocal
:: Builds QGroundControl-OI on this machine. First run configures (a few minutes,
:: it downloads the CMake dependencies); later runs only rebuild what changed,
:: usually a minute or two.
::
::   custom\scripts\build-local.cmd                 Release build into build\
::   custom\scripts\build-local.cmd -DQGC_ENABLE_GST_VIDEOSTREAMING=ON    (extra CMake args on first configure)
::
:: Video is OFF by default for local builds: the GStreamer SDK is an MSI install
:: (see custom\README.md); CI builds with video ON. Delete build\ to reconfigure.
::
:: Needs: Visual Studio 2022 Build Tools with the C++ workload (cl, CMake, Ninja),
:: Python 3, and Qt under .qt\ (custom\scripts\install-qt.cmd).

set "ROOT=%~dp0..\.."
pushd "%ROOT%" || exit /b 1

for /f "usebackq delims=" %%v in (`python -c "import json;print(json.load(open('custom/build-config.json'))['qt']['version'])"`) do set "QT_VERSION=%%v"
set "QT_ROOT_DIR=%CD%\.qt\Qt\%QT_VERSION%\msvc2022_64"
if not exist "%QT_ROOT_DIR%\bin\qt-cmake.bat" (
    echo Qt %QT_VERSION% not found at %QT_ROOT_DIR%. Run custom\scripts\install-qt.cmd first.
    popd & exit /b 1
)

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo Visual Studio 2022 with the "Desktop development with C++" workload was not found.
    popd & exit /b 1
)
:: VsDevCmd looks for vswhere.exe on PATH; keep its warning quiet.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VCVARS%" >nul || (popd & exit /b 1)

set "BUILD_DIR=build"
:: build.ninja only exists after a successful configure (CMakeCache.txt is left behind by a failed one).
if not exist "%BUILD_DIR%\build.ninja" (
    echo Configuring %BUILD_DIR% for the first time. This downloads the CMake dependencies...
    call "%QT_ROOT_DIR%\bin\qt-cmake.bat" -S . -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF %* || (popd & exit /b 1)
)

echo Building...
cmake --build "%BUILD_DIR%" --config Release || (popd & exit /b 1)

echo.
if exist "%BUILD_DIR%\Release\QGroundControl-OI.exe" (
    echo Built: %CD%\%BUILD_DIR%\Release\QGroundControl-OI.exe
) else (
    echo Built: %CD%\%BUILD_DIR%\QGroundControl-OI.exe
)
echo Run it with custom\scripts\run-local.cmd
popd
endlocal
