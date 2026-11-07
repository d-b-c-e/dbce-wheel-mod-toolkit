@echo off
REM Double-click to change the wheel mod's settings with the game closed.
REM -ExecutionPolicy Bypass: Windows blocks downloaded scripts by default.
title Wheel mod - setup
if not exist "%~dp0dbce\Install.ps1" (
    echo Extract the whole zip first, then run this file from the extracted folder.
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dbce\Install.ps1" -Setup %*
set "result=%errorlevel%"
echo.
pause
exit /b %result%
