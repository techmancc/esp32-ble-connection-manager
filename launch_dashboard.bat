@echo off
echo 🚀 ESP32 Dashboard Quick Launcher
echo ================================

REM Check if ESP32 is accessible
echo 📡 Checking ESP32...
curl -s http://10.0.0.67/api/state > nul 2>&1
if %errorlevel% == 0 (
    echo ✅ ESP32 found at 10.0.0.67
) else (
    echo ❌ ESP32 not accessible at 10.0.0.67
    goto :end
)

echo.
echo 🔧 Starting React development server...
echo    Running: npm run dev -- --host
start /B npm run dev -- --host

echo.
echo ⏳ Waiting 8 seconds for server to start...
timeout /t 8 /nobreak > nul

echo.
echo 🎯 Opening dashboard URLs in your browser...

REM Try multiple potential dashboard URLs based on different network interfaces
start http://192.168.11.203:5173/
timeout /t 1 /nobreak > nul
start http://10.0.0.5:5173/
timeout /t 1 /nobreak > nul
start http://10.0.0.1:5173/
timeout /t 1 /nobreak > nul
start http://192.168.1.100:5173/
timeout /t 1 /nobreak > nul

echo.
echo 📱 ESP32 Web Interface: http://10.0.0.67/
echo 🔌 WebSocket: ws://10.0.0.67:81
echo.
echo 💡 If dashboard doesn't open, manually try:
echo    http://10.0.0.5:5173/
echo    http://10.0.0.1:5173/
echo    http://10.0.0.100:5173/
echo.

:end
pause