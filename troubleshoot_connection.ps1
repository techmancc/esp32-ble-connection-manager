#!/usr/bin/env pwsh

<#
.SYNOPSIS
    ESP32 Connection Troubleshooter
.DESCRIPTION
    Diagnoses connection issues between your PC and ESP32
#>

Write-Host "🔧 ESP32 Connection Troubleshooter" -ForegroundColor Green
Write-Host "================================" -ForegroundColor Green
Write-Host ""

# Step 1: Check if ESP32 is connected via USB
Write-Host "1️⃣ Checking USB Connection..." -ForegroundColor Cyan
$serialPorts = Get-CimInstance -ClassName Win32_SerialPort | Where-Object { $_.Name -like "*USB*" -or $_.Name -like "*ESP32*" -or $_.Name -like "*CP210*" -or $_.Name -like "*CH340*" }

if ($serialPorts) {
    Write-Host "✅ Found USB serial devices:" -ForegroundColor Green
    $serialPorts | ForEach-Object { Write-Host "   $($_.Name) - $($_.DeviceID)" -ForegroundColor White }
} else {
    Write-Host "❌ No ESP32 USB connection found!" -ForegroundColor Red
    Write-Host "   Please connect ESP32 via USB cable" -ForegroundColor Yellow
}

Write-Host ""

# Step 2: Check WiFi Access Point mode
Write-Host "2️⃣ Checking ESP32 Access Point mode..." -ForegroundColor Cyan
try {
    $apResponse = Invoke-RestMethod -Uri "http://192.168.4.1/api/state" -TimeoutSec 3 -ErrorAction SilentlyContinue
    if ($apResponse) {
        Write-Host "✅ ESP32 found in Access Point mode!" -ForegroundColor Green
        Write-Host "   Access via: http://192.168.4.1" -ForegroundColor White
        $apMode = $true
    } else {
        Write-Host "❌ ESP32 not accessible in AP mode" -ForegroundColor Red
        $apMode = $false
    }
} catch {
    Write-Host "❌ ESP32 not accessible in AP mode" -ForegroundColor Red
    $apMode = $false
}

Write-Host ""

# Step 3: Check home network connection
Write-Host "3️⃣ Searching for ESP32 on home network..." -ForegroundColor Cyan

# Get current PC's IP to determine network range
$pcIP = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { 
    $_.InterfaceAlias -ne "Loopback Pseudo-Interface 1" -and
    $_.IPAddress -ne "127.0.0.1" -and
    $_.IPAddress -notlike "169.254.*" 
} | Select-Object -First 1).IPAddress

Write-Host "   Your PC IP: $pcIP" -ForegroundColor White

if ($pcIP) {
    $networkPrefix = ($pcIP -split '\.')[0..2] -join '.'
    Write-Host "   Scanning network: $networkPrefix.xxx" -ForegroundColor White
    
    $candidateIPs = 100..110 | ForEach-Object { "$networkPrefix.$_" }
    $esp32Found = $false
    
    foreach ($ip in $candidateIPs) {
        Write-Host "   Testing $ip..." -ForegroundColor Gray -NoNewline
        try {
            $response = Invoke-RestMethod -Uri "http://$ip/api/state" -TimeoutSec 1 -ErrorAction SilentlyContinue
            if ($response -and ($response.stagedParams -or $response.appliedParams)) {
                Write-Host " ✅ ESP32 FOUND!" -ForegroundColor Green
                $esp32HomeIP = $ip
                $esp32Found = $true
                break
            } else {
                Write-Host " ❌" -ForegroundColor Red
            }
        } catch {
            Write-Host " ❌" -ForegroundColor Red
        }
    }
    
    if (-not $esp32Found) {
        Write-Host "❌ ESP32 not found on home network" -ForegroundColor Red
    }
} else {
    Write-Host "❌ Could not determine your PC's IP address" -ForegroundColor Red
}

Write-Host ""

# Step 4: Check React dev server
Write-Host "4️⃣ Checking React Development Server..." -ForegroundColor Cyan
try {
    $reactResponse = Invoke-WebRequest -Uri "http://localhost:5173" -TimeoutSec 2 -ErrorAction SilentlyContinue
    if ($reactResponse.StatusCode -eq 200) {
        Write-Host "✅ React server running at http://localhost:5173" -ForegroundColor Green
        
        # Also check network interface
        if ($pcIP) {
            $networkUrl = "http://" + $pcIP + ":5173"
            try {
                $networkResponse = Invoke-WebRequest -Uri $networkUrl -TimeoutSec 2 -ErrorAction SilentlyContinue
                if ($networkResponse.StatusCode -eq 200) {
                    Write-Host "✅ React server accessible on network at $networkUrl" -ForegroundColor Green
                }
            } catch {
                Write-Host "❌ React server not accessible on network" -ForegroundColor Red
            }
        }
    }
} catch {
    Write-Host "❌ React server not running!" -ForegroundColor Red
    Write-Host "   Run: cd client && npm run dev" -ForegroundColor Yellow
}

Write-Host ""

# Summary and recommendations
Write-Host "📋 Summary & Next Steps:" -ForegroundColor Green
Write-Host "========================" -ForegroundColor Green

if ($apMode) {
    Write-Host "✅ ESP32 is running in Access Point mode" -ForegroundColor Green
    Write-Host "   🌐 Dashboard: http://localhost:5173" -ForegroundColor White
    Write-Host "   📡 ESP32 API: http://192.168.4.1" -ForegroundColor White
    Write-Host ""
    Write-Host "🎯 To access dashboard:" -ForegroundColor Cyan
    Write-Host "   1. Make sure you're connected to ESP32's WiFi network" -ForegroundColor White
    Write-Host "   2. Open browser to: http://localhost:5173" -ForegroundColor White
} elseif ($esp32Found) {
    Write-Host "✅ ESP32 found on home network at $esp32HomeIP" -ForegroundColor Green
    Write-Host "   🌐 Dashboard: http://localhost:5173" -ForegroundColor White
    Write-Host "   📡 ESP32 API: http://$esp32HomeIP" -ForegroundColor White
    Write-Host ""
    Write-Host "🎯 Dashboard should work automatically!" -ForegroundColor Green
} else {
    Write-Host "❌ ESP32 not found on any network" -ForegroundColor Red
    Write-Host ""
    Write-Host "🔧 Troubleshooting steps:" -ForegroundColor Yellow
    Write-Host "   1. Connect ESP32 via USB and upload firmware:" -ForegroundColor White
    Write-Host "      pio run --target upload" -ForegroundColor Gray
    Write-Host "   2. Check serial monitor:" -ForegroundColor White
    Write-Host "      pio device monitor" -ForegroundColor Gray
    Write-Host "   3. Look for WiFi network 'ESP32-BLE-Control-XXXXXX'" -ForegroundColor White
    Write-Host "   4. Connect to that network if available" -ForegroundColor White
}

Write-Host ""
Write-Host "💡 Need help? Check serial monitor output:" -ForegroundColor Cyan
Write-Host "   pio device monitor --baud 115200" -ForegroundColor Gray