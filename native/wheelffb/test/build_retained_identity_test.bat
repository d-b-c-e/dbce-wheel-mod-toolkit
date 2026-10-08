@echo off
REM Production source with fake COM devices/window only; never opens a real device.
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
if not "%ARCH%"=="x64" if not "%ARCH%"=="x86" exit /b 2
set "OUT=..\build"
if "%ARCH%"=="x86" set "OUT=..\build\x86"
if not exist "%OUT%" mkdir "%OUT%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 3
if "%ARCH%"=="x86" (call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul) else (call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul)
if errorlevel 1 exit /b 4
cl /nologo /O2 /W4 /WX /EHsc /Fo"%OUT%\retained_identity_test.obj" /Fe"%OUT%\retained_identity_test.exe" retained_identity_test.cpp
if errorlevel 1 exit /b 5
"%OUT%\retained_identity_test.exe"
exit /b %errorlevel%
