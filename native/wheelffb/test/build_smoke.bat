@echo off
REM Builds smoke.exe next to the WheelFfb.dll it is going to load.
REM   build_smoke.bat        -> build\smoke.exe      (x64)
REM   build_smoke.bat x86    -> build\x86\smoke.exe  (32-bit, the OutRun 2006 case)
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "OUT=..\build"
if /i "%ARCH%"=="x86" set "OUT=..\build\x86"

if not exist "%OUT%\WheelFfb.dll" (
  echo ERROR: %OUT%\WheelFfb.dll not built - run build.bat %ARCH% first
  exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if /i "%ARCH%"=="x86" ( call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul ) else ( call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul )

cl.exe /nologo /O2 /W4 /Fo"%OUT%\\" /Fe"%OUT%\smoke.exe" smoke.c /link user32.lib
if errorlevel 1 exit /b 1

echo Built %OUT%\smoke.exe
exit /b 0
