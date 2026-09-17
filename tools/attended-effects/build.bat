@echo off
REM Builds the standalone Win32 diagnostic, compiling the frozen adapter source in.
REM No adjacent DLL is loaded. This script NEVER launches the UI.
REM build.bat [x64 or x86] -> build\[x86\]attended_effects.exe
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "OUT=build"
if /i "%ARCH%"=="x86" set "OUT=build\x86"

if not exist "%OUT%" mkdir "%OUT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if /i "%ARCH%"=="x86" ( call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul ) else ( call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul )

set "REVISION="
for /f %%R in ('git rev-parse HEAD') do set "REVISION=%%R"
if not defined REVISION ( echo ERROR: source revision unavailable & exit /b 1 )
cl.exe /DPROBE_SOURCE_REV=\"%REVISION%\" /nologo /O2 /W4 /EHsc /Fo"%OUT%\\" /Fe"%OUT%\attended_effects.exe" attended_effects.cpp /link /SUBSYSTEM:WINDOWS gdi32.lib user32.lib dinput8.lib dxguid.lib ole32.lib
if errorlevel 1 exit /b 1

echo Built %OUT%\attended_effects.exe
REM Do NOT launch the UI: only the owner may initiate attended tests.
exit /b 0
