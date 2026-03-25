@echo off
setlocal

set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%.."

echo [prebuild-clean] Cleaning stale build cache and artifacts...

if exist ".xmake" (
    echo [prebuild-clean] Remove .xmake\
    rmdir /s /q ".xmake"
)

if exist "build" (
    echo [prebuild-clean] Remove build\
    rmdir /s /q "build"
)

if exist "output" (
    echo [prebuild-clean] Remove output\
    rmdir /s /q "output"
)

echo [prebuild-clean] Recreate output\
mkdir output >nul 2>&1

echo [prebuild-clean] Done.
popd
endlocal
