@echo off
rem Gera o aplicativo unificado na pasta da versão em dist\.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package-suite.ps1" %*
exit /b %errorlevel%
