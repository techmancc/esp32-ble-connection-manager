# ESP32 + React Dashboard Setup Script
Write-Host "🚀 ESP32 BLE Dashboard Setup" -ForegroundColor Cyan
Write-Host ""

Write-Host "Current Status:" -ForegroundColor Yellow
Write-Host "✅ ESP32 Network: ESP32-BLE-Control-9e9ef0 (Password: esp32ble)"
Write-Host "✅ ESP32 IP: 192.168.4.1"
Write-Host "✅ ESP32 API Working: /api/presets returns JSON"
Write-Host ""

Write-Host "Next Steps:" -ForegroundColor Green
Write-Host ""
Write-Host "1. MAKE SURE you're connected to ESP32 WiFi network:"
Write-Host "   Network: ESP32-BLE-Control-9e9ef0"
Write-Host "   Password: esp32ble" -ForegroundColor Yellow
Write-Host ""

Write-Host "2. Test ESP32 API from your browser:"
Write-Host "   Open: http://192.168.4.1/api/presets" -ForegroundColor Cyan
Write-Host "   (Should show 4 presets in JSON format)"
Write-Host ""

Write-Host "3. Start React Dashboard:"
Write-Host "   Run this in a new PowerShell window:" -ForegroundColor Cyan
Write-Host "   cd 'C:\Users\techm\OneDrive\Documents\Arduino\replit_conn_params_system'"
Write-Host "   npm run dev -- --host 0.0.0.0 --port 3000"
Write-Host ""

Write-Host "4. Find your PC's IP on ESP32 network:"
$adapters = Get-NetAdapter | Where-Object { $_.Status -eq 'Up' }
Write-Host "   Your network adapters:" -ForegroundColor Yellow
foreach ($adapter in $adapters) {
    $ip = Get-NetIPAddress -InterfaceIndex $adapter.InterfaceIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.PrefixOrigin -ne 'WellKnown' }
    if ($ip) {
        Write-Host "   - $($adapter.Name): $($ip.IPAddress)" -ForegroundColor White
        if ($ip.IPAddress -like "192.168.4.*") {
            Write-Host "     👆 This is your ESP32 network IP!" -ForegroundColor Green
        }
    }
}
Write-Host ""

Write-Host "5. Access React Dashboard:" -ForegroundColor Green
Write-Host "   Once React is running, open in browser:"
Write-Host "   http://[YOUR-PC-IP]:3000" -ForegroundColor Cyan
Write-Host "   Example: http://192.168.4.2:3000"
Write-Host ""

Write-Host "6. Expected Result:" -ForegroundColor Green
Write-Host "   ✅ Full React dashboard with styling"
Write-Host "   ✅ 4 presets visible (Low Power, Balanced, iOS Pairing, Android Pairing)"
Write-Host "   ✅ Real-time WebSocket connection to ESP32"
Write-Host ""

Write-Host "Environment Check:" -ForegroundColor Yellow
Write-Host "Your .env file should contain:"
Write-Host "VITE_API_BASE_URL=http://192.168.4.1"
Write-Host "VITE_WS_URL=ws://192.168.4.1:81"
Write-Host ""

# Check if we're on ESP32 network
$esp32IP = Test-Connection -ComputerName "192.168.4.1" -Count 1 -Quiet
if ($esp32IP) {
    Write-Host "🎉 Great! You're connected to ESP32 network!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Quick Test - ESP32 API:" -ForegroundColor Cyan
    try {
        $response = Invoke-WebRequest -Uri "http://192.168.4.1/api/presets" -TimeoutSec 5
        Write-Host "✅ ESP32 API Response: $($response.StatusCode)" -ForegroundColor Green
        $presets = $response.Content | ConvertFrom-Json
        Write-Host "✅ Found $($presets.Count) presets:" -ForegroundColor Green
        foreach ($preset in $presets) {
            Write-Host "   - $($preset.name)" -ForegroundColor White
        }
    }
    catch {
        Write-Host "❌ ESP32 API Test Failed: $($_.Exception.Message)" -ForegroundColor Red
    }
} else {
    Write-Host "⚠️  Not connected to ESP32 network yet!" -ForegroundColor Red
    Write-Host "   Connect to WiFi: ESP32-BLE-Control-9e9ef0" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Need help? The ESP32 web interface at http://192.168.4.1/ has links to try!" -ForegroundColor Cyan