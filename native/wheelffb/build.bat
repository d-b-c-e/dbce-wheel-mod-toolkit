@echo off
REM Builds x64 WheelFfb.dll. Called by build.ps1, but works standalone
REM from any shell - it sets up the MSVC x64 environment itself.
REM
REM Delayed expansion is required: the vswhere path contains "(x86)", and a
REM literal ")" expanded at parse time would close the for-block early.
setlocal enabledelayedexpansion
set "HERE=%~dp0"
REM ARCH: x64 (default) or x86 - the same source builds both; 32-bit games (OutRun 2006) need x86.
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "OUT=%HERE%build"
if /i "%ARCH%"=="x86" set "OUT=%HERE%build\x86"
if not exist "%OUT%" mkdir "%OUT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )

for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH ( echo ERROR: MSVC x64 toolset not found & exit /b 1 )

if /i "%ARCH%"=="x86" ( call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul ) else ( call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul )
if errorlevel 1 ( echo ERROR: vcvars64 failed & exit /b 1 )

cl.exe /nologo /std:c++17 /LD /O2 /W4 /EHsc /DNDEBUG ^
  /Fo"%OUT%\\" /Fe"%OUT%\WheelFfb.dll" ^
  "%HERE%WheelFfb.cpp" ^
  /link /DEF:"%HERE%WheelFfb.def" /MACHINE:%ARCH%
exit /b %errorlevel%
