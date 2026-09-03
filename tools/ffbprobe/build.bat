@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
cl.exe /nologo /O2 /W3 /EHsc ffbprobe.cpp /link dinput8.lib dxguid.lib user32.lib /MACHINE:X64 /OUT:ffbprobe.exe

