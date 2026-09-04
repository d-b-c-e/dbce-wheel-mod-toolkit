@echo off
REM Compiles dinput_proxy.h in both architectures and runs its checks.
REM A proxy header that only compiles on the machine that wrote it is no use;
REM every consumer of this is 32-bit or 64-bit depending on the game.
setlocal enabledelayedexpansion
cd /d "%~dp0"
if not exist build mkdir build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"

for %%A in (x64 x86) do (
  setlocal
  if /i "%%A"=="x86" ( call "!VSPATH!\VC\Auxiliary\Build\vcvars32.bat" >nul ) else ( call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul )
  cl.exe /nologo /O2 /W4 /EHsc /D_CRT_SECURE_NO_WARNINGS /Fo"build\%%A_" /Fe"build\proxy_test_%%A.exe" test_dinput_proxy.cpp /link dinput8.lib dxguid.lib user32.lib
  if errorlevel 1 ( echo BUILD FAILED for %%A & exit /b 1 )
  endlocal
)

echo.
.\build\proxy_test_x64.exe
if errorlevel 1 exit /b 1
.\build\proxy_test_x86.exe
exit /b %errorlevel%
