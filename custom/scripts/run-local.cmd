@echo off
setlocal
:: Starts the locally built QGroundControl-OI (build\ from custom\scripts\build-local.cmd).
:: Qt's DLLs are found through the same Qt folder the build used. A local build runs
:: as "QGroundControl-OI Daily" with its own settings file, so it does not disturb an
:: installed release.

set "ROOT=%~dp0..\.."
pushd "%ROOT%" || exit /b 1

for /f "usebackq delims=" %%v in (`python -c "import json;print(json.load(open('.github/build-config.json'))['qt']['version'])"`) do set "QT_VERSION=%%v"
set "PATH=%CD%\.qt\Qt\%QT_VERSION%\msvc2022_64\bin;%PATH%"

:: The GStreamer SDK that configure downloaded (its DLLs are not copied next to the exe until install).
for /f "tokens=2 delims==" %%g in ('findstr /b "GStreamer_ROOT_DIR" build\CMakeCache.txt 2^>nul') do set "GST_ROOT=%%g"
if defined GST_ROOT set "PATH=%GST_ROOT:/=\%\bin;%PATH%"

set "EXE=%CD%\build\Release\QGroundControl-OI.exe"
if not exist "%EXE%" set "EXE=%CD%\build\QGroundControl-OI.exe"
if not exist "%EXE%" (
    echo No local build found. Run custom\scripts\build-local.cmd first.
    popd & exit /b 1
)

start "" "%EXE%" %*
popd
endlocal
