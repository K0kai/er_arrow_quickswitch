@echo off
rem Use an existing Developer Command Prompt, an explicit override, or vswhere.
where cl.exe >nul 2>nul
if not errorlevel 1 exit /b 0
if defined AQS_VCVARS64 goto initialize
set "AQS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%AQS_VSWHERE%" goto missing
for /f "usebackq delims=" %%I in (`"%AQS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "AQS_VS_ROOT=%%I"
if not defined AQS_VS_ROOT goto missing
set "AQS_VCVARS64=%AQS_VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat"
:initialize
if not exist "%AQS_VCVARS64%" goto missing
call "%AQS_VCVARS64%" >nul
exit /b %errorlevel%
:missing
echo Install Visual Studio C++ Build Tools with the Windows SDK, or run from an x64 Developer Command Prompt. 1>&2
exit /b 1
