@echo off
REM Builds and runs the force-model conformance check (x64, MSVC).
REM   build_conformance.bat            check against conformance.expected.txt
REM   build_conformance.bat --update   regenerate it, deliberately
setlocal enabledelayedexpansion
cd /d "%~dp0"
if not exist build mkdir build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
cl.exe /nologo /O2 /W4 /EHsc /D_CRT_SECURE_NO_WARNINGS /Fo"build\\" /Fe"build\conformance.exe" conformance.cpp
if errorlevel 1 exit /b 1
cl.exe /nologo /O2 /W4 /EHsc /Fo"build\\" /Fe"build\events.exe" test_events.cpp
if errorlevel 1 exit /b 1
.\build\events.exe ..\..\profiles
if errorlevel 1 exit /b 1
cd build
REM explicit ".\" because NoDefaultCurrentDirectoryInExePath may be set
.\conformance.exe %1
exit /b %errorlevel%
