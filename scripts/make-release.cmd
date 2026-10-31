@echo off
rem Builds the installer, the portable exe and SHA256SUMS.txt into dist\ and tests them.
rem Usage:  scripts\make-release.cmd [-SkipDeps] [-SkipTest]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make-release.ps1" %*
exit /b %errorlevel%
