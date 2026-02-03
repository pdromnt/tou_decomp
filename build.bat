@echo off
set TARGET=tou_decomp.exe

echo Checking for running instances...
tasklist /FI "IMAGENAME eq %TARGET%" 2>NUL | find /I /N "%TARGET%">NUL
if "%ERRORLEVEL%"=="0" (
    echo [WARNING] %TARGET% is currently running. Attempting to close...
    taskkill /F /IM %TARGET%
    timeout /t 1 >nul
)

echo Cleaning old build...
mingw32-make clean >nul 2>&1

echo Building %TARGET%...
mingw32-make -j%NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed! Check the output above.
    exit /b %ERRORLEVEL%
)

echo Installing...
rem copy /Y %TARGET% ..\ >nul

echo Start game? (y/n)
set /p CHOICE=
if /I "%CHOICE%"=="y" (
    start %TARGET%
)

pause