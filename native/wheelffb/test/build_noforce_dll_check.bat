@echo off
REM Builds noforce_dll_check.exe and runs it against the two built DLLs of one architecture: the product
REM (build\[x86\]WheelFfb.dll, status and version only) and the no-force build (build\noforce\[x86\]WheelFfb.dll).
REM Build both first: build.ps1 [-Arch x86] and build.ps1 -NoForce [-Arch x86]. No device, window or force is touched.
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "PRODUCT=..\build"
set "NOFORCE=..\build\noforce"
if /i "%ARCH%"=="x86" ( set "PRODUCT=..\build\x86" & set "NOFORCE=..\build\noforce\x86" )
if not exist "%PRODUCT%\WheelFfb.dll" ( echo ERROR: %PRODUCT%\WheelFfb.dll not built & exit /b 1 )
if not exist "%NOFORCE%\WheelFfb.dll" ( echo ERROR: %NOFORCE%\WheelFfb.dll not built & exit /b 1 )

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if /i "%ARCH%"=="x86" ( call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul ) else ( call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul )

cl.exe /nologo /O2 /W4 /Fo"%NOFORCE%\\" /Fe"%NOFORCE%\noforce_dll_check.exe" noforce_dll_check.c
if errorlevel 1 exit /b 1

set "VERSION="
for /f "usebackq tokens=2 delims==" %%v in (`%NOFORCE%\noforce_dll_check.exe product %PRODUCT%\WheelFfb.dll`) do set "VERSION=%%v"
if not defined VERSION ( echo FAIL: the product check reported no version & exit /b 1 )
"%NOFORCE%\noforce_dll_check.exe" noforce "%NOFORCE%\WheelFfb.dll" !VERSION!
exit /b %errorlevel%
