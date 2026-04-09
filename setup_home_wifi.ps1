#!/usr/bin/env pwsh

<#
.SYNOPSIS
    ESP32 Home WiFi Configuration Script
.DESCRIPTION
    This script helps configure the ESP32 to connect to your home WiFi network
    instead of running its own Access Point.
.EXAMPLE
    .\setup_home_wifi.ps1
#>

Write-Host "🚀 ESP32 Home WiFi Setup" -ForegroundColor Green
Write-Host "========================" -ForegroundColor Green
Write-Host ""

# Check if ESP32 is accessible
$esp32Found = $false
$esp32Url = ""

Write-Host "🔍 Searching for ESP32..." -ForegroundColor Yellow

# First, check if ESP32 is running in Access Point mode
try {
    $response = Invoke-RestMethod -Uri "http://192.168.4.1/api/state" -TimeoutSec 3 -ErrorAction SilentlyContinue
    if ($response) {
        Write-Host "✅ Found ESP32 in Access Point mode at http://192.168.4.1" -ForegroundColor Green
        $esp32Url = "http://192.168.4.1"
        $esp32Found = $true
    }
} catch {
    Write-Host "⚠️  ESP32 not found in Access Point mode" -ForegroundColor Yellow
}

# If not found in AP mode, try to discover on home network
if (-not $esp32Found) {
    Write-Host "🔍 Searching for ESP32 on home network..." -ForegroundColor Yellow

    # Get local network range
    $localIP = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -ne "Loopback Pseudo-Interface 1" } | Select-Object -First 1).IPAddress
    $networkPrefix = ($localIP -split '\.')[0..2] -join '.'

    # Common ESP32 IPs
    $candidateIPs = @(
        "$networkPrefix.100",
        "$networkPrefix.101",
        "$networkPrefix.102",
        "$networkPrefix.103",
        "$networkPrefix.104"
    )

    foreach ($ip in $candidateIPs) {
        Write-Host "  Testing $ip..." -ForegroundColor Gray
        try {
            $response = Invoke-RestMethod -Uri "http://$ip/api/state" -TimeoutSec 2 -ErrorAction SilentlyContinue
            if ($response -and ($response.stagedParams -or $response.appliedParams)) {
                Write-Host "✅ Found ESP32 at http://$ip" -ForegroundColor Green
                $esp32Url = "http://$ip"
                $esp32Found = $true
                break
            }
        } catch {
            # Continue searching
        }
    }
}

if (-not $esp32Found) {
    Write-Host "❌ ESP32 not found on network!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please ensure:" -ForegroundColor Yellow
    Write-Host "  1. ESP32 is powered on and programmed with latest firmware" -ForegroundColor Yellow
    Write-Host "  2. For AP mode: Connect your PC to ESP32's WiFi network 'ESP32-BLE-Control-XXXXXX'" -ForegroundColor Yellow
    Write-Host "  3. For home network: ESP32 should already be connected to your WiFi" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "📡 ESP32 Status: Connected at $esp32Url" -ForegroundColor Green

# Check current WiFi configuration
Write-Host ""
Write-Host "🔧 Configuring Home WiFi Connection..." -ForegroundColor Cyan
Write-Host ""

# Get WiFi credentials from user
Write-Host "Enter your home WiFi credentials:" -ForegroundColor Cyan
$ssid = Read-Host "WiFi Network Name (SSID)"
if ([string]::IsNullOrWhiteSpace($ssid)) {
    Write-Host "❌ SSID cannot be empty!" -ForegroundColor Red
    exit 1
}

$password = Read-Host "WiFi Password" -AsSecureString
$passwordPlain = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($password))

Write-Host ""
Write-Host "📤 Sending WiFi configuration to ESP32..." -ForegroundColor Yellow

# Send WiFi configuration to ESP32
try {
    $configUrl = "$esp32Url/savewifi?ssid=" + [System.Web.HttpUtility]::UrlEncode($ssid) + "&pass=" + [System.Web.HttpUtility]::UrlEncode($passwordPlain)
    $response = Invoke-WebRequest -Uri $configUrl -Method Get -TimeoutSec 10

    if ($response.StatusCode -eq 200 -or $response.StatusCode -eq 302) {
        Write-Host "✅ WiFi configuration sent successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "⏳ ESP32 is now attempting to connect to your home WiFi..." -ForegroundColor Yellow
        Write-Host "   This may take up to 30 seconds." -ForegroundColor Yellow
        Write-Host ""

        # Wait for ESP32 to connect to home WiFi
        Write-Host "🔍 Waiting for ESP32 to connect and searching for new IP address..." -ForegroundColor Yellow

        Start-Sleep -Seconds 5

        # Try to find ESP32 on home network
        $homeNetworkFound = $false
        for ($i = 1; $i -le 12; $i++) {
            Write-Host "  Attempt $i/12..." -ForegroundColor Gray

            # Get current network range
            $localIP = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -ne "Loopback Pseudo-Interface 1" } | Select-Object -First 1).IPAddress
            $networkPrefix = ($localIP -split '\.')[0..2] -join '.'

            # Search for ESP32 on home network
            $candidateIPs = @(
                "$networkPrefix.100",
                "$networkPrefix.101",
                "$networkPrefix.102",
                "$networkPrefix.103",
                "$networkPrefix.104",
                "$networkPrefix.105",
                "$networkPrefix.106",
                "$networkPrefix.107",
                "$networkPrefix.108",
                "$networkPrefix.109"
            )

            foreach ($ip in $candidateIPs) {
                try {
                    $testResponse = Invoke-RestMethod -Uri "http://$ip/api/state" -TimeoutSec 1 -ErrorAction SilentlyContinue
                    if ($testResponse -and ($testResponse.stagedParams -or $testResponse.appliedParams)) {
                        Write-Host "🎉 ESP32 successfully connected to home WiFi!" -ForegroundColor Green
                        Write-Host "📡 New ESP32 IP Address: $ip" -ForegroundColor Green
                        Write-Host ""
                        Write-Host "✅ Configuration Complete!" -ForegroundColor Green
                        Write-Host ""
                        Write-Host "🚀 Next Steps:" -ForegroundColor Cyan
                        Write-Host "  1. Start the React development server:" -ForegroundColor White
                        Write-Host "     cd client && npm run dev -- --host" -ForegroundColor Gray
                        Write-Host "  2. Open your browser to:" -ForegroundColor White
                        Write-Host "     http://localhost:5173" -ForegroundColor Gray
                        Write-Host "  3. Both your PC and ESP32 are now on the same network!" -ForegroundColor White
                        Write-Host ""
                        Write-Host "📊 ESP32 API: http://$ip" -ForegroundColor Green
                        Write-Host "🔧 WiFi Config: http://$ip/wificonfig (if needed)" -ForegroundColor Green

                        $homeNetworkFound = $true
                        break
                    }
                } catch {
                    # Continue searching
                }
            }

            if ($homeNetworkFound) {
                break
            }

            Start-Sleep -Seconds 2
        }

        if (-not $homeNetworkFound) {
            Write-Host "⚠️  ESP32 connection status unknown" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "The WiFi configuration was sent, but ESP32's new IP address couldn't be determined." -ForegroundColor Yellow
            Write-Host ""
            Write-Host "Please check:" -ForegroundColor Cyan
            Write-Host "  1. ESP32 Serial Monitor for connection status" -ForegroundColor White
            Write-Host "  2. Your router's admin panel for connected devices" -ForegroundColor White
            Write-Host "  3. Try: nmap -sn 192.168.1.0/24 (or your network range)" -ForegroundColor White
            Write-Host ""
            Write-Host "If ESP32 failed to connect, it should fall back to Access Point mode." -ForegroundColor Yellow
        }

    } else {
        Write-Host "❌ Failed to send configuration. Status: $($response.StatusCode)" -ForegroundColor Red
    }

} catch {
    Write-Host "❌ Error sending WiFi configuration: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    Write-Host "You can also configure WiFi manually:" -ForegroundColor Yellow
    Write-Host "  1. Open: $esp32Url/wificonfig" -ForegroundColor White
    Write-Host "  2. Enter your WiFi credentials in the web form" -ForegroundColor White
}

Write-Host ""
Write-Host "🔧 For troubleshooting, check ESP32 Serial Monitor at 115200 baud" -ForegroundColor Cyan