@echo off
setlocal
set "HERE=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl.exe /nologo /W4 /WX /EHsc "%HERE%digital_input_test.cpp" /Fo"%HERE%..\build\digital-input-test.obj" /Fe"%HERE%..\build\digital-input-test.exe"
if errorlevel 1 exit /b 1
"%HERE%..\build\digital-input-test.exe"
exit /b %errorlevel%
