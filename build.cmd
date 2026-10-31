@echo off
rem Build the unified application in its versioned folder under dist\.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\make-suite-release.ps1" %*
exit /b %errorlevel%
