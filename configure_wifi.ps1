#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Manual WiFi Configuration for ESP32
.DESCRIPTION
    Directly configure ESP32 WiFi credentials with proper URL encoding
#>

# Load System.Web assembly for URL encoding
Add-Type -AssemblyName System.Web

Write-Host "🔧 ESP32 Manual WiFi Configuration" -ForegroundColor Green
Write-Host "=================================" -ForegroundColor Green
Write-Host ""

# Check if ESP32 is accessible
$esp32Ip = "192.168.4.1"
try {
    $response = Invoke-RestMethod -Uri "http://$esp32Ip/api/state" -TimeoutSec 3 -ErrorAction Stop
    Write-Host "✅ ESP32 found at $esp32Ip" -ForegroundColor Green
} catch {
    Write-Host "❌ Cannot connect to ESP32 at $esp32Ip" -ForegroundColor Red
    Write-Host "   Please ensure you're connected to ESP32's WiFi network" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "📝 Enter WiFi Credentials:" -ForegroundColor Cyan

# Get SSID
$ssid = "PrincessAndMoana-2.4G-ext"
Write-Host "SSID: $ssid" -ForegroundColor White

# Get Password
$password = "0urC@tCalli3_1"
Write-Host "Password: $('*' * $password.Length)" -ForegroundColor White

Write-Host ""
Write-Host "🔄 Configuring WiFi..." -ForegroundColor Yellow

# URL encode the credentials properly
$encodedSSID = [System.Web.HttpUtility]::UrlEncode($ssid)
$encodedPassword = [System.Web.HttpUtility]::UrlEncode($password)

# Build the configuration URL
$configUrl = "http://$esp32Ip/savewifi?ssid=$encodedSSID&pass=$encodedPassword"

Write-Host "📡 Sending configuration..." -ForegroundColor Yellow
Write-Host "   SSID: $ssid" -ForegroundColor Gray
Write-Host "   URL: http://$esp32Ip/savewifi?ssid=$encodedSSID" -NoNewline -ForegroundColor Gray
Write-Host '&pass=[HIDDEN]' -ForegroundColor Gray

try {
    $response = Invoke-WebRequest -Uri $configUrl -Method Get -TimeoutSec 10

    if ($response.StatusCode -eq 200) {
        Write-Host "✅ WiFi configuration sent successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "⏳ ESP32 is restarting and attempting to connect..." -ForegroundColor Yellow
        Write-Host "   This may take up to 30 seconds." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "📋 Next Steps:" -ForegroundColor Cyan
        Write-Host "  1. Wait 30 seconds for ESP32 to restart" -ForegroundColor White
        Write-Host "  2. Check your router's admin panel for 'ESP32' or similar device" -ForegroundColor White
        Write-Host "  3. Try accessing the new IP address" -ForegroundColor White
        Write-Host "  4. If connection fails, ESP32 will return to Access Point mode" -ForegroundColor White
        Write-Host ""
        Write-Host "🔍 To find ESP32's new IP address:" -ForegroundColor Cyan
        Write-Host "  - Check router admin panel (usually 192.168.1.1 or 10.0.0.1)" -ForegroundColor White
        Write-Host "  - Look for device named 'ESP32' or with MAC starting with 'f0:9e:9e'" -ForegroundColor White
        Write-Host "  - Try common IPs: 192.168.1.100, 10.0.0.100, etc." -ForegroundColor White

    } else {
        Write-Host "⚠️  Unexpected response code: $($response.StatusCode)" -ForegroundColor Yellow
    }

} catch {
    Write-Host "❌ Error sending configuration: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    Write-Host "💡 Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Ensure you're connected to ESP32's WiFi network" -ForegroundColor White
    Write-Host "  2. Try accessing http://192.168.4.1 in a browser first" -ForegroundColor White
    Write-Host "  3. Check if ESP32 is powered and running" -ForegroundColor White
}

Write-Host ""
Write-Host "🔧 You can also configure manually:" -ForegroundColor Cyan
Write-Host "   1. Open: http://192.168.4.1/wificonfig" -ForegroundColor White
Write-Host "   2. Enter credentials in the web form" -ForegroundColor White