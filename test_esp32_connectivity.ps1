# Test ESP32 connectivity
Write-Host "Testing ESP32 connectivity..."
Write-Host ""

# Test ping first
Write-Host "1. Testing ping to ESP32..."
$pingResult = Test-Connection -ComputerName 192.168.1.77 -Count 2 -Quiet
if ($pingResult) {
    Write-Host "✅ ESP32 is reachable via ping" -ForegroundColor Green
} else {
    Write-Host "❌ ESP32 is not reachable via ping" -ForegroundColor Red
}

# Test port 80 (HTTP)
Write-Host ""
Write-Host "2. Testing HTTP port 80..."
try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    $tcpClient.ReceiveTimeout = 3000
    $tcpClient.SendTimeout = 3000
    $result = $tcpClient.BeginConnect("192.168.1.77", 80, $null, $null)
    $success = $result.AsyncWaitHandle.WaitOne(3000)

    if ($success -and $tcpClient.Connected) {
        Write-Host "✅ Port 80 is open and responsive" -ForegroundColor Green
        $tcpClient.Close()

        # Try actual HTTP request
        Write-Host ""
        Write-Host "3. Testing HTTP request..."
        try {
            $response = Invoke-WebRequest -Uri "http://192.168.1.77/api/presets" -TimeoutSec 10
            Write-Host "✅ HTTP request successful!" -ForegroundColor Green
            Write-Host "Response: $($response.Content.Substring(0, [Math]::Min(100, $response.Content.Length)))..."
        }
        catch {
            Write-Host "❌ HTTP request failed: $($_.Exception.Message)" -ForegroundColor Red
        }
    } else {
        Write-Host "❌ Port 80 is not responding" -ForegroundColor Red
        $tcpClient.Close()
    }
}
catch {
    Write-Host "❌ Connection test failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test port 81 (WebSocket)
Write-Host ""
Write-Host "4. Testing WebSocket port 81..."
try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    $result = $tcpClient.BeginConnect("192.168.1.77", 81, $null, $null)
    $success = $result.AsyncWaitHandle.WaitOne(3000)

    if ($success -and $tcpClient.Connected) {
        Write-Host "✅ Port 81 is open and responsive" -ForegroundColor Green
        $tcpClient.Close()
    } else {
        Write-Host "❌ Port 81 is not responding" -ForegroundColor Red
        $tcpClient.Close()
    }
}
catch {
    Write-Host "❌ WebSocket port test failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "Test completed!"
Write-Host "If all tests pass, try accessing: http://192.168.1.77/ in your browser"