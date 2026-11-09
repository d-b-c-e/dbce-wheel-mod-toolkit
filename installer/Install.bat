@echo off
REM Double-click to install or update the wheel mod. Close the game first.
REM -ExecutionPolicy Bypass: Windows blocks downloaded scripts by default.
title Wheel mod - install
if not exist "%~dp0dbce\Install.ps1" (
    echo Extract the whole zip first, then run this file from the extracted folder.
    pause
    exit /b 1
)
REM First install or update the central Wheelkit from the bundled offline installer, then add this mod to it.
if exist "%~dp0wheelkit\Setup-Wheelkit.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0wheelkit\Setup-Wheelkit.ps1"
    if errorlevel 1 (
        echo.
        pause
        exit /b 1
    )
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dbce\Install.ps1" %*
set "result=%errorlevel%"
echo.
pause
exit /b %result%
