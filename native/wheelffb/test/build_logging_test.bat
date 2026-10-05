@echo off
REM Builds logging_test.exe next to the WheelFfb.dll it is going to load.
REM   build_logging_test.bat        -> build\logging_test.exe      (x64)
REM   build_logging_test.bat x86    -> build\x86\logging_test.exe  (32-bit, the OutRun 2006 case)
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

cl.exe /nologo /O2 /W4 /EHsc /Fo"%OUT%\\" /Fe"%OUT%\logging_test.exe" logging_test.cpp /link user32.lib dinput8.lib dxguid.lib ole32.lib
if errorlevel 1 exit /b 1

echo Built %OUT%\logging_test.exe
for %%M in (off on master-off invalid) do (
 .\%OUT%\logging_test.exe %%M
 if errorlevel 1 exit /b 1
)
exit /b 0
