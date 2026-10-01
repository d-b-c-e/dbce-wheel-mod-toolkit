@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "OUT=build\%ARCH%"
if not exist "!OUT!" mkdir "!OUT!"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if /i "%ARCH%"=="x86" (call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul) else (call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul)
cl.exe /nologo /O2 /W4 /WX /EHsc /Fo"!OUT!\\" /Fe"!OUT!\cadence.exe" test_cadence.cpp
if errorlevel 1 exit /b 1
.\!OUT!\cadence.exe "%~2"
exit /b %errorlevel%
