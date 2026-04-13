#!/usr/bin/env pwsh

# ESP32 API Test Script
# This script tests if the ESP32 API endpoints are working properly

Write-Host "🧪 ESP32 API Test Script" -ForegroundColor Cyan
Write-Host "========================" -ForegroundColor Cyan

$workspaceRoot = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$envLocalPath = Join-Path $workspaceRoot "client/.env.local"

$baseUrl = ""
if (Test-Path $envLocalPath) {
    $apiLine = Get-Content -Path $envLocalPath | Where-Object { $_ -match '^VITE_API_BASE_URL=' } | Select-Object -First 1
    if ($apiLine) {
        $baseUrl = ($apiLine -replace '^VITE_API_BASE_URL=', '').Trim()
    }
}

if ([string]::IsNullOrWhiteSpace($baseUrl)) {
    $baseUrl = "http://192.168.4.1"
}

try {
    $baseUri = [System.Uri]$baseUrl
    $esp32IP = $baseUri.Host
} catch {
    $esp32IP = "192.168.4.1"
    $baseUrl = "http://$esp32IP"
}

Write-Host "`n📡 Testing ESP32 API at: $baseUrl" -ForegroundColor Yellow

# Test 1: Get State
Write-Host "`n1️⃣ Testing /api/state endpoint..."
try {
    $response = Invoke-RestMethod -Uri "$baseUrl/api/state" -Method GET -TimeoutSec 10
    Write-Host "   ✅ State API working!" -ForegroundColor Green
    Write-Host "   📊 Current interval: $($response.parameters.current.connectionIntervalMin)-$($response.parameters.current.connectionIntervalMax)ms" -ForegroundColor White
} catch {
    Write-Host "   ❌ State API failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 2: Get Presets
Write-Host "`n2️⃣ Testing /api/presets endpoint..."
try {
    $presets = Invoke-RestMethod -Uri "$baseUrl/api/presets" -Method GET -TimeoutSec 10
    Write-Host "   ✅ Presets API working!" -ForegroundColor Green
    Write-Host "   📋 Found $($presets.Count) presets:" -ForegroundColor White
    foreach ($preset in $presets) {
        Write-Host "      - $($preset.name): $($preset.description)" -ForegroundColor Gray
    }
} catch {
    Write-Host "   ❌ Presets API failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 3: Get History
Write-Host "`n3️⃣ Testing /api/history endpoint..."
try {
    $history = Invoke-RestMethod -Uri "$baseUrl/api/history" -Method GET -TimeoutSec 10
    Write-Host "   ✅ History API working!" -ForegroundColor Green
    Write-Host "   📈 Found $($history.Count) history entries" -ForegroundColor White
} catch {
    Write-Host "   ❌ History API failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 4: WebSocket availability (just check if port is open)
Write-Host "`n4️⃣ Testing WebSocket port 81..."
try {
    $tcp = New-Object System.Net.Sockets.TcpClient
    $tcp.ConnectAsync($esp32IP, 81).Wait(3000)
    if ($tcp.Connected) {
        Write-Host "   ✅ WebSocket port 81 is open!" -ForegroundColor Green
        $tcp.Close()
    } else {
        Write-Host "   ❌ WebSocket port 81 is not accessible" -ForegroundColor Red
    }
} catch {
    Write-Host "   ❌ WebSocket test failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "`n🎯 Test Summary:" -ForegroundColor Cyan
Write-Host "   ESP32 IP: $esp32IP" -ForegroundColor White
Write-Host "   Base URL: $baseUrl" -ForegroundColor White
Write-Host "   React Client: http://localhost:5173/" -ForegroundColor White
Write-Host "`n💡 If all tests pass, the presets should work in the React dashboard!" -ForegroundColor Green