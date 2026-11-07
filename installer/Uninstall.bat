@echo off
REM Double-click to remove the wheel mod. The game's own files come back. Close the game first.
REM -ExecutionPolicy Bypass: Windows blocks downloaded scripts by default.
title Wheel mod - uninstall
if not exist "%~dp0dbce\Install.ps1" (
    echo Extract the whole zip first, then run this file from the extracted folder.
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dbce\Install.ps1" -Uninstall %*
set "result=%errorlevel%"
echo.
pause
exit /b %result%
