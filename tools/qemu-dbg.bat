@echo off
set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%.."

start D:\RT-ThreadStudio\repo\Extract\Debugger_Support_Packages\RealThread\QEMU\4.2.0.4\qemu-system-arm -M stm32f407-atk-explorer -kernel output/firmware/firmware.bin -serial stdio -S -s

popd
