@echo off
REM Builds/runs device-free lifecycle checks against the actual candidate DLL.
setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
if not "%ARCH%"=="x64" if not "%ARCH%"=="x86" exit /b 2
set "OUT=%HERE%..\build"
if "%ARCH%"=="x86" set "OUT=%HERE%..\build\x86"
if not exist "%OUT%\WheelFfb.dll" exit /b 3
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 4
if "%ARCH%"=="x86" (call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul) else (call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul)
if errorlevel 1 exit /b 5
cl /nologo /O2 /W4 /EHsc /Fo"%OUT%\abi_lifecycle_test.obj" /Fe"%OUT%\abi_lifecycle_test.exe" "%HERE%abi_lifecycle_test.cpp" /link /MACHINE:%ARCH%
if errorlevel 1 exit /b 6
for %%i in ("%OUT%\WheelFfb.dll") do set "DLL=%%~fi"
"%OUT%\abi_lifecycle_test.exe" "%DLL%"
exit /b %errorlevel%
