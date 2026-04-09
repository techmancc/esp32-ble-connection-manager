@echo off
title ESP32 Firmware Upload
echo ========================================
echo 🔧 ESP32 USB Firmware Upload
echo ========================================
echo.

echo 1️⃣ Stopping any running serial monitors...
taskkill /f /im "python.exe" /fi "WINDOWTITLE eq *monitor*" 2>nul >nul
taskkill /f /im "platformio.exe" 2>nul >nul
timeout /t 2 /nobreak >nul

echo 2️⃣ Uploading firmware to ESP32...
pio run --target upload

if %ERRORLEVEL% equ 0 (
    echo.
    echo ✅ Upload successful!
    echo 3️⃣ Starting serial monitor to verify...
    timeout /t 3 /nobreak >nul
    echo Press Ctrl+C to exit monitor when ready
    pio device monitor --baud 115200
) else (
    echo.
    echo ❌ Upload failed!
    echo 💡 Try:
    echo    1. Unplug and replug ESP32 USB cable
    echo    2. Press and hold BOOT button, then press RESET
    echo    3. Run this script again
    echo.
    pause
)