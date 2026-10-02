@echo off
setlocal
set "HERE=%~dp0"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "OUT=%HERE%..\build\test-%ARCH%"
if not exist "%OUT%" mkdir "%OUT%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
if /i "%ARCH%"=="x86" (call "%VSPATH%\VC\Auxiliary\Build\vcvars32.bat" >nul) else (call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul)
if errorlevel 1 exit /b 1
cl.exe /nologo /W4 /WX /EHsc "%HERE%digital_input_test.cpp" /Fo"%OUT%\digital-input-test.obj" /Fe"%OUT%\digital-input-test.exe"
if errorlevel 1 exit /b 1
"%OUT%\digital-input-test.exe"
exit /b %errorlevel%
