@echo off
setlocal
:: Installs the Qt SDK this repo builds with (version and modules from
:: custom\build-config.json, the same ones CI uses) into the git-ignored .qt\
:: folder at the repo root. Needs Python 3 on PATH. About 4 GB, run once.
::
::   custom\scripts\install-qt.cmd

set "ROOT=%~dp0..\.."
pushd "%ROOT%" || exit /b 1

:: Same aqtinstall commit as CI pins: released aqtinstall versions lag behind new Qt layouts.
python -m pip install --quiet --upgrade "git+https://github.com/miurahr/aqtinstall.git@8d961e61720b3c06583517fbc68d57ec0a4e9f95" || (popd & exit /b 1)

for /f "usebackq delims=" %%v in (`python -c "import json;print(json.load(open('custom/build-config.json'))['qt']['version'])"`) do set "QT_VERSION=%%v"
for /f "usebackq delims=" %%a in (`python -c "import json;print(json.load(open('custom/build-config.json'))['qt']['arch'])"`) do set "QT_ARCH=%%a"
for /f "usebackq delims=" %%m in (`python -c "import json;print(json.load(open('custom/build-config.json'))['qt']['modules'])"`) do set "QT_MODULES=%%m"

echo Installing Qt %QT_VERSION% (%QT_ARCH%) with modules: %QT_MODULES%
echo into %CD%\.qt\Qt ...
python -m aqt install-qt windows desktop %QT_VERSION% %QT_ARCH% -O .qt\Qt -m %QT_MODULES% || (popd & exit /b 1)

echo.
echo Done. Next: custom\scripts\build-local.cmd
popd
endlocal
