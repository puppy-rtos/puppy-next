@echo off
set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%.."

echo Starting QEMU test...
echo.

start "" /b D:\RT-ThreadStudio\repo\Extract\Debugger_Support_Packages\RealThread\QEMU\4.2.0.4\qemu-system-arm.exe -M stm32f407-atk-explorer -kernel output/firmware.bin -serial stdio -nographic

timeout /t 6 /nobreak >nul

taskkill /F /IM qemu-system-arm.exe >nul 2>&1

echo.
echo Test finished.

popd
