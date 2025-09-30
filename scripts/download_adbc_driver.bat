@echo off
REM Download pre-built ADBC Snowflake driver for Windows
REM Version 1.8.0 from ADBC release 20

set ADBC_VERSION=apache-arrow-adbc-20
set DRIVER_VERSION=1.8.0
set BASE_URL=https://github.com/apache/arrow-adbc/releases/download/%ADBC_VERSION%

echo Detected platform: Windows

REM Create directory for drivers
set DRIVER_DIR=%~dp0..\adbc_drivers
if not exist "%DRIVER_DIR%" mkdir "%DRIVER_DIR%"

REM Windows wheel (download as zip for extraction)
set WHEEL_NAME=adbc_driver_snowflake-%DRIVER_VERSION%-py3-none-win_amd64.whl
set ZIP_NAME=adbc_driver_snowflake.zip
set WHEEL_URL=%BASE_URL%/%WHEEL_NAME%
set ZIP_PATH=%DRIVER_DIR%\%ZIP_NAME%

REM Download the wheel as zip if it doesn't exist
if not exist "%ZIP_PATH%" (
    echo Downloading %WHEEL_NAME% as %ZIP_NAME%...
    wget "%WHEEL_URL%" -O "%ZIP_PATH%"
    if errorlevel 1 (
        echo Failed to download ADBC driver
        exit /b 1
    )
) else (
    echo Zip file already exists: %ZIP_PATH%
)

REM Extract the shared library from the zip using PowerShell
echo Extracting driver library...
cd /d "%DRIVER_DIR%"
powershell -Command "Expand-Archive -Path '%ZIP_NAME%' -DestinationPath . -Force"

if errorlevel 1 (
    echo Failed to extract driver library
    exit /b 1
)

REM The library is named .so even on Windows
if exist "adbc_driver_snowflake\libadbc_driver_snowflake.so" (
    move /Y "adbc_driver_snowflake\libadbc_driver_snowflake.so" .
    rmdir /S /Q "adbc_driver_snowflake" 2>nul
    del "%ZIP_NAME%" 2>nul
    echo ADBC Snowflake driver extracted successfully to: %DRIVER_DIR%\libadbc_driver_snowflake.so
) else (
    echo Failed to find driver library in zip
    exit /b 1
)

echo ADBC driver setup complete!
echo Driver location: %DRIVER_DIR%\libadbc_driver_snowflake.so