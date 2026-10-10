@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
if not "%ARCH%"=="x64" if not "%ARCH%"=="x86" exit /b 2
set "OUT=..\build"
if /i "%ARCH%"=="x86" set "OUT=..\build\x86"
if not exist "%OUT%" mkdir "%OUT%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" exit /b 1
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
if "%ARCH%"=="x86" (call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul) else (call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul)
if errorlevel 1 exit /b 1
cl.exe /nologo /std:c++17 /O2 /W4 /EHsc /Fo"%OUT%\\" /Fe"%OUT%\constant_recovery_test.exe" constant_recovery_test.cpp
exit /b %errorlevel%
