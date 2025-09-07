@echo off
REM Download pre-built ADBC Snowflake driver for Windows
REM Version 1.7.0 from ADBC release 19

set ADBC_VERSION=apache-arrow-adbc-19
set DRIVER_VERSION=1.7.0
set BASE_URL=https://github.com/apache/arrow-adbc/releases/download/%ADBC_VERSION%

echo Detected platform: Windows

REM Create directory for drivers
set DRIVER_DIR=%~dp0..\adbc_drivers
if not exist "%DRIVER_DIR%" mkdir "%DRIVER_DIR%"

REM Windows wheel
set WHEEL_NAME=adbc_driver_snowflake-%DRIVER_VERSION%-py3-none-win_amd64.whl
set WHEEL_URL=%BASE_URL%/%WHEEL_NAME%
set WHEEL_PATH=%DRIVER_DIR%\%WHEEL_NAME%

REM Download the wheel if it doesn't exist
if not exist "%WHEEL_PATH%" (
    echo Downloading %WHEEL_NAME%...
    powershell -Command "Invoke-WebRequest -Uri '%WHEEL_URL%' -OutFile '%WHEEL_PATH%'"
    if errorlevel 1 (
        echo Failed to download ADBC driver
        exit /b 1
    )
) else (
    echo Wheel already exists: %WHEEL_PATH%
)

REM Extract the shared library from the wheel using PowerShell
echo Extracting driver library...
cd /d "%DRIVER_DIR%"
powershell -Command "Expand-Archive -Path '%WHEEL_NAME%' -DestinationPath . -Force"

if errorlevel 1 (
    echo Failed to extract driver library
    exit /b 1
)

REM The library is named .so even on Windows
if exist "adbc_driver_snowflake\libadbc_driver_snowflake.so" (
    move /Y "adbc_driver_snowflake\libadbc_driver_snowflake.so" .
    rmdir /S /Q "adbc_driver_snowflake" 2>nul
    echo ADBC Snowflake driver extracted successfully to: %DRIVER_DIR%\libadbc_driver_snowflake.so
) else (
    echo Failed to find driver library in wheel
    exit /b 1
)

echo ADBC driver setup complete!
echo Driver location: %DRIVER_DIR%\libadbc_driver_snowflake.so