@echo off
setlocal
call "%~dp0suite-env.cmd"
if errorlevel 1 exit /b 1
cmake -S "%~dp0.." -B "%~dp0..\build\suite" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=%LOCALFLIX_QT_DIR%"
if errorlevel 1 exit /b 1
cmake --build "%~dp0..\build\suite" --parallel 6
exit /b %errorlevel%
