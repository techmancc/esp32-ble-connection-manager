#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Smart ESP32 Dashboard Launcher
.DESCRIPTION
    Automatically launches the React dashboard with the correct IP for your network
#>

param(
    [string]$ESP32IP = "10.0.0.67",
    [switch]$StartServer = $false
)

Write-Host "🚀 ESP32 Smart Dashboard Launcher" -ForegroundColor Green
Write-Host "=================================" -ForegroundColor Green
Write-Host ""

# Check if ESP32 is accessible
try {
    Write-Host "📡 Checking ESP32 at $ESP32IP..." -ForegroundColor Yellow
    $response = Invoke-RestMethod -Uri "http://$ESP32IP/api/state" -TimeoutSec 3 -ErrorAction Stop
    Write-Host "✅ ESP32 found and responding!" -ForegroundColor Green
} catch {
    Write-Host "❌ Could not reach ESP32 at $ESP32IP" -ForegroundColor Red
    Write-Host "Please ensure ESP32 is powered on and connected to your network." -ForegroundColor Yellow
    exit 1
}

# Get current IP configuration to determine dashboard URLs
$localIP = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -like "10.*" -or $_.IPAddress -like "192.168.*" })[0].IPAddress
$subnet = $localIP -replace '\.\d+$', ''

Write-Host ""
Write-Host "🌐 Network Information:" -ForegroundColor Cyan
Write-Host "   Your PC IP: $localIP" -ForegroundColor White
Write-Host "   ESP32 IP: $ESP32IP" -ForegroundColor White
Write-Host "   Network: $subnet.x" -ForegroundColor White
Write-Host ""

# Generate dashboard URLs based on network
$dashboardURLs = @(
    "http://$subnet.5:5173/",
    "http://$subnet.1:5173/", 
    "http://$subnet.100:5173/",
    "http://$subnet.101:5173/"
)

# Check if React dev server should be started
if ($StartServer) {
    Write-Host "🔧 Starting React development server..." -ForegroundColor Yellow
    Start-Process -FilePath "npm" -ArgumentList "run", "dev", "--", "--host" -NoNewWindow
    Write-Host "⏳ Waiting for server to start..." -ForegroundColor Yellow
    Start-Sleep 5
}

# Try dashboard URLs
Write-Host "🎯 Testing dashboard URLs..." -ForegroundColor Green

$dashboardOpened = $false
foreach ($url in $dashboardURLs) {
    try {
        Write-Host "   Trying: $url" -ForegroundColor Gray
        $response = Invoke-WebRequest -Uri $url -TimeoutSec 2 -ErrorAction Stop
        if ($response.StatusCode -eq 200) {
            Write-Host "✅ Dashboard accessible! Opening browser..." -ForegroundColor Green
            Start-Process $url
            $dashboardOpened = $true
            break
        }
    } catch {
        # Silently continue to next URL
    }
}

if (-not $dashboardOpened) {
    Write-Host ""
    Write-Host "⚠️  Dashboard not accessible yet. Please:" -ForegroundColor Yellow
    Write-Host "1. Start the React server:" -ForegroundColor White
    Write-Host "   npm run dev -- --host" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. Then try these URLs:" -ForegroundColor White
    foreach ($url in $dashboardURLs) {
        Write-Host "   $url" -ForegroundColor Cyan
    }
} else {
    Write-Host ""
    Write-Host "🎉 Dashboard launched successfully!" -ForegroundColor Green
}

Write-Host ""
Write-Host "📱 ESP32 Web Interface: http://$ESP32IP/" -ForegroundColor Cyan
Write-Host "🔌 WebSocket: ws://$ESP32IP:81" -ForegroundColor Cyan
Write-Host ""
Write-Host "💡 Use -StartServer to auto-start React dev server" -ForegroundColor Gray