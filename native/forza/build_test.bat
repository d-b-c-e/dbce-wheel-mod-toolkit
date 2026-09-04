@echo off
REM Builds and runs the forza_packet.h conformance test (x64, MSVC).
setlocal enabledelayedexpansion
cd /d "%~dp0"
if not exist build mkdir build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
cl.exe /nologo /O2 /W4 /EHsc /Fo"build\\" /Fe"build\test_forza_packet.exe" test_forza_packet.cpp
if errorlevel 1 exit /b 1
"build\test_forza_packet.exe"
exit /b %errorlevel%
