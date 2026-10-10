@echo off
REM Builds and runs the test-injection and no-force-mode checks against fake COM devices only (injection_test.cpp).
REM Optional argument x86 selects the 32-bit harness. The no-force latch is per process, so every scenario runs in its
REM own process; the enable-versus-open race runs ten times. No hardware is opened.
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "OUT=..\build"
if /i "%ARCH%"=="x86" set "OUT=..\build\x86"

if not exist "%OUT%" mkdir "%OUT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if /i "%ARCH%"=="x86" ( call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul ) else ( call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul )

cl.exe /nologo /std:c++17 /O2 /W4 /EHsc /Fo"%OUT%\\" /Fe"%OUT%\injection_test.exe" injection_test.cpp /link user32.lib dinput8.lib dxguid.lib ole32.lib
if errorlevel 1 exit /b 1

for %%s in (armed forcefirst effectfirst externalfirst) do (
  .\%OUT%\injection_test.exe %%s
  if errorlevel 1 exit /b 1
)
for /l %%i in (1,1,10) do (
  .\%OUT%\injection_test.exe race
  if errorlevel 1 exit /b 1
)
exit /b 0
