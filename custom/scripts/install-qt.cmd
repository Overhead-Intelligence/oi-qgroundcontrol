@echo off
setlocal
:: Installs the Qt SDK this repo builds with (version and modules from
:: .github\build-config.json, the same ones CI uses) into the git-ignored .qt\
:: folder at the repo root. Needs Python 3 on PATH. About 4 GB, run once.
::
::   custom\scripts\install-qt.cmd

set "ROOT=%~dp0..\.."
pushd "%ROOT%" || exit /b 1

:: Same aqtinstall commit as AQT_SOURCE in .github\workflows\oi-windows.yml: the
:: released aqtinstall (3.3.0) does not know the Qt 6.11 repository layout.
python -m pip install --quiet --upgrade "git+https://github.com/miurahr/aqtinstall.git@8d961e61720b3c06583517fbc68d57ec0a4e9f95" || (popd & exit /b 1)

for /f "usebackq delims=" %%v in (`python -c "import json;print(json.load(open('.github/build-config.json'))['qt']['version'])"`) do set "QT_VERSION=%%v"
for /f "usebackq delims=" %%m in (`python -c "import json;print(json.load(open('.github/build-config.json'))['qt']['modules'])"`) do set "QT_MODULES=%%m"

echo Installing Qt %QT_VERSION% (MSVC 2022 x64) with modules: %QT_MODULES%
echo into %CD%\.qt\Qt ...
:: aqt is called as a module: tools\setup\install_qt.py expects an aqt.exe on PATH,
:: which a per-user pip install does not provide.
python -m aqt install-qt windows desktop %QT_VERSION% win64_msvc2022_64 -O .qt\Qt -m %QT_MODULES% || (popd & exit /b 1)

echo.
echo Done. Next: custom\scripts\build-local.cmd
popd
endlocal
