@echo off
title ESP32 Firmware Upload
color 0A
echo ========================================
echo 🔧 ESP32 USB Firmware Upload
echo ========================================
echo.

echo 1️⃣ Stopping any running serial monitors...
taskkill /f /im "python.exe" /fi "WINDOWTITLE eq *monitor*" 2>nul >nul
taskkill /f /im "platformio.exe" 2>nul >nul
taskkill /f /im "node.exe" /fi "WINDOWTITLE eq *bridge*" 2>nul >nul
echo    ✅ Processes stopped
timeout /t 2 /nobreak >nul

echo.
echo 2️⃣ Building and uploading USB firmware to ESP32...
echo    📦 Compiling ESP32 USB firmware...
pio run --target upload

if %ERRORLEVEL% equ 0 (
    echo.
    echo ✅ Upload successful!
    echo.
    echo 3️⃣ Verifying ESP32 startup...
    timeout /t 3 /nobreak >nul
    echo    📊 Starting serial monitor (Press Ctrl+C to exit)
    echo.
    pio device monitor --baud 115200
) else (
    echo.
    echo ❌ Upload failed!
    echo.
    echo 🔧 Troubleshooting steps:
    echo    1. Unplug ESP32 USB cable for 5 seconds
    echo    2. Plug it back in
    echo    3. Press and hold BOOT button on ESP32
    echo    4. Press RESET button while holding BOOT
    echo    5. Release BOOT button
    echo    6. Run this script again
    echo.
    pause
)