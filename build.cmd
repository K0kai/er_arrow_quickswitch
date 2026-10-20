@echo off
call "%~dp0build-switcher.cmd" %*
exit /b %errorlevel%
