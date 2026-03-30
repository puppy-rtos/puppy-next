@echo off
set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%.."

echo Starting QEMU test...
echo.

if not exist "output\firmware\firmware.bin" (
	echo ERROR: output\firmware\firmware.bin not found. Build firmware first.
	popd
	exit /b 1
)

d:/Scoop/apps/python/3.13.7/python.exe tools/qemu_cmd_runner.py
set TEST_EXIT=%ERRORLEVEL%

echo.
echo Test finished.

popd
exit /b %TEST_EXIT%
