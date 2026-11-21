@echo off
if exist "%~dp0dist\MediaSuite-1.4.1\MediaSuite.exe" (
    start "" "%~dp0dist\MediaSuite-1.4.1\MediaSuite.exe" %*
    exit /b 0
)
echo Run build.cmd first to build Media Suite.
exit /b 1
