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
REM FLAVOR: empty (the product) or noforce - the internal test build, force refused from process load
REM (WHEELFFB_NO_FORCE_BUILD). It only ever writes under build\noforce, never over the product DLL, and links
REM deterministically (/Brepro), so the same source and toolchain give the same hash.
set "FLAVOR=%~2"
set "DEFS="
set "LINKDEFS="
set "OUT=%HERE%build"
if /i "%FLAVOR%"=="noforce" ( set "OUT=%HERE%build\noforce" & set "DEFS=/DWHEELFFB_NO_FORCE_BUILD /Brepro" & set "LINKDEFS=/Brepro" ) else if not "%FLAVOR%"=="" ( echo ERROR: unknown flavor %FLAVOR% & exit /b 1 )
if /i "%ARCH%"=="x86" set "OUT=%OUT%\x86"
if not exist "%OUT%" mkdir "%OUT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )

for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH ( echo ERROR: MSVC x64 toolset not found & exit /b 1 )

if /i "%ARCH%"=="x86" ( call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul ) else ( call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul )
if errorlevel 1 ( echo ERROR: vcvars64 failed & exit /b 1 )

REM The no-force provenance records the toolchain: VCToolsVersion and the compiler banner (cl prints it on stderr).
if /i "%FLAVOR%"=="noforce" (
  > "%OUT%\toolchain.txt" echo VCToolsVersion=!VCToolsVersion!
  cl.exe 2>> "%OUT%\toolchain.txt" >nul
)

cl.exe /nologo /std:c++17 /LD /O2 /W4 /EHsc /DNDEBUG %DEFS% ^
  /Fo"%OUT%\\" /Fe"%OUT%\WheelFfb.dll" ^
  "%HERE%WheelFfb.cpp" ^
  /link /DEF:"%HERE%WheelFfb.def" /MACHINE:%ARCH% %LINKDEFS%
exit /b %errorlevel%
