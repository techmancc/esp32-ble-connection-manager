#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Smart ESP32 Dashboard Launcher
.DESCRIPTION
    Automatically detects ESP32 network configuration and launches the React dashboard
    with the correct IP addresses for the current network setup.
#>

param(
    [string]$ESP32IP = "",
    [switch]$StartServer = $false,
    [switch]$Verbose = $false
)

Write-Host "🚀 ESP32 Smart Dashboard Launcher" -ForegroundColor Green
Write-Host "=================================" -ForegroundColor Green
Write-Host ""

# Function to find ESP32 IP
function Find-ESP32 {
    $commonIPs = @("192.168.4.1", "10.0.0.67")

    # Try known IPs first
    foreach ($ip in $commonIPs) {
        try {
            if ($Verbose) { Write-Host "Checking $ip..." -ForegroundColor Gray }
            $response = Invoke-RestMethod -Uri "http://$ip/api/state" -TimeoutSec 2 -ErrorAction Stop
            Write-Host "✅ Found ESP32 at $ip" -ForegroundColor Green
            return $ip
        } catch { }
    }

    # Scan current subnet
    $localIP = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -like "10.*" -or $_.IPAddress -like "192.168.*" })[0].IPAddress
    if ($localIP) {
        $subnet = $localIP -replace '\.\d+$', ''
        Write-Host "🔍 Scanning subnet $subnet.x..." -ForegroundColor Yellow

        50..120 | ForEach-Object {
            $ip = "$subnet.$_"
            try {
                $response = Invoke-RestMethod -Uri "http://$ip/api/state" -TimeoutSec 0.5 -ErrorAction Stop
                Write-Host "✅ Found ESP32 at $ip" -ForegroundColor Green
                return $ip
            } catch { }
        }
    }

    return $null
}

# Find ESP32 IP if not provided
if (-not $ESP32IP) {
    Write-Host "🔍 Searching for ESP32..." -ForegroundColor Yellow
    $ESP32IP = Find-ESP32

    if (-not $ESP32IP) {
        Write-Host "❌ Could not find ESP32. Please ensure:" -ForegroundColor Red
        Write-Host "   1. ESP32 is powered on and running" -ForegroundColor White
        Write-Host "   2. You're connected to the same network as ESP32" -ForegroundColor White
        Write-Host "   3. Try specifying IP manually: .\launch_dashboard.ps1 -ESP32IP 192.168.4.1" -ForegroundColor White
        exit 1
    }
}

# Get dashboard configuration from ESP32
try {
    Write-Host "📡 Getting dashboard configuration from ESP32..." -ForegroundColor Yellow
    $dashConfig = Invoke-RestMethod -Uri "http://$ESP32IP/api/dashboard" -TimeoutSec 5 -ErrorAction Stop

    Write-Host ""
    Write-Host "📊 ESP32 Status:" -ForegroundColor Cyan
    Write-Host "   Network Mode: $($dashConfig.networkMode)" -ForegroundColor White
    Write-Host "   Network: $($dashConfig.networkName)" -ForegroundColor White
    Write-Host "   ESP32 IP: $($dashConfig.esp32IP)" -ForegroundColor White
    Write-Host ""

} catch {
    Write-Host "❌ Failed to get dashboard config: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Check if React dev server should be started
if ($StartServer -or -not (Get-Process -Name "node" -ErrorAction SilentlyContinue)) {
    Write-Host "🔧 Starting React development server..." -ForegroundColor Yellow
    Write-Host "   Running: npm run dev -- --host" -ForegroundColor Gray

    # Start the dev server in background
    Start-Process -FilePath "npm" -ArgumentList "run", "dev", "--", "--host" -NoNewWindow -PassThru
    Write-Host "⏳ Waiting for server to start..." -ForegroundColor Yellow
    Start-Sleep 5
}

# Try dashboard URLs
Write-Host "🎯 Attempting to open dashboard..." -ForegroundColor Green

$dashboardOpened = $false
foreach ($url in $dashConfig.dashboardURLs) {
    try {
        Write-Host "   Trying: $url" -ForegroundColor Gray

        # Test if the URL is accessible
        $response = Invoke-WebRequest -Uri $url -TimeoutSec 3 -ErrorAction Stop
        if ($response.StatusCode -eq 200) {
            Write-Host "✅ Dashboard accessible at: $url" -ForegroundColor Green

            # Open in default browser
            Start-Process $url
            $dashboardOpened = $true
            break
        }
    } catch {
        if ($Verbose) {
            Write-Host "   ❌ $url not accessible" -ForegroundColor Red
        }
    }
}

if (-not $dashboardOpened) {
    Write-Host ""
    Write-Host "⚠️  Dashboard not accessible yet. Manual steps:" -ForegroundColor Yellow
    Write-Host "1. Ensure React server is running:" -ForegroundColor White
    Write-Host "   npm run dev -- --host" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. Try these URLs manually:" -ForegroundColor White
    foreach ($url in $dashConfig.dashboardURLs) {
        Write-Host "   $url" -ForegroundColor Cyan
    }
    Write-Host ""
    Write-Host "3. ESP32 API is available at:" -ForegroundColor White
    Write-Host "   http://$($dashConfig.esp32IP)/" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "🎉 Dashboard launched successfully!" -ForegroundColor Green
    Write-Host "📱 ESP32 Web Interface: http://$($dashConfig.esp32IP)/" -ForegroundColor Cyan
    Write-Host "🔌 WebSocket Endpoint: $($dashConfig.wsEndpoint)" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "💡 Pro tip: Run with -StartServer to auto-start the React dev server" -ForegroundColor Gray
Write-Host "💡 Pro tip: Run with -Verbose for detailed connection testing" -ForegroundColor Gray