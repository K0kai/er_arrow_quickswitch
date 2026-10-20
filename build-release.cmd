@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package_release.ps1" %*
exit /b %errorlevel%
