@echo off
if exist "%~dp0dist\MediaSuite-1.1.0\MediaSuite.exe" (
    start "" "%~dp0dist\MediaSuite-1.1.0\MediaSuite.exe" %*
    exit /b 0
)
echo Execute build.cmd primeiro para gerar o Media Suite.
exit /b 1
