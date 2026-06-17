
Running the Config Manager (Client form)

Use the root folder, not the client folder.

Your client app is now running at:

http://localhost:5173/
Network URL: http://10.0.0.8:5173/
Why this matters:

This project’s Vite config sets root to client internally in vite.config.ts:29, and npm scripts are in package.json:6.
There is no separate client/package.json, so cd client then npm run dev will not work.
Exact commands to run next time:

cd C:\Users\carlsc12\esp32-ble-connection-manager
npm install (only if needed)
npm run dev
Open http://localhost:5173/


***************************************

npm run dev:host:

ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x0 (DOWNLOAD(USB/UART0))
waiting for download

Device is busy or does not respond. Your options:

  - wait until it completes current work;
  - use Ctrl+C to interrupt current work;
  - reset the device and try again;
  - check connection properties;
  - make sure the device has suitable MicroPython / CircuitPython / firmware;
  - make sure the device is not in bootloader mode.

ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x4bc
load:0x403c9700,len:0xbd8
load:0x403cc700,len:0x2a0c
entry 0x403c98d0
E (227) psram: PSRAM ID read error: 0x00ffffff, PSRAM chip not found or not supported, or wrong PSRAM line mode

Starting BLE Peripheral + WebServer (ESP32-S3)
Loaded 0 paired devices
BLE Security configured: Bonding (compatibility mode)
BLE Client Manager Ready - Scanning for peripherals...
🔍 Found stored WiFi credentials for: 'PrincessAndMoana-2.4G-ext'
📶 Attempting to connect to home WiFi...
E (1380) wifi:timeout when WiFi un-init, type=4
📡 WiFi scan match: SSID visible, matches=1, RSSI=-50, channel=3, auth=WPA2_PSK
🔄 Connection attempt 1/7...

✅ Connected to home WiFi successfully!
📡 Network: PrincessAndMoana-2.4G-ext
🌐 IP Address: 192.168.11.109
💻 Web Dashboard: http://192.168.11.109/
📊 WebSocket: ws://192.168.11.109:81
🔧 WiFi Config: http://192.168.11.109/wificonfig
🏠 Both ESP32 and your PC are now on the same network!
🚀 Start React dev server with: npm run dev -- --host
📱 Then open: http://localhost:5173 or http://192.168.11.109:5173
Web server started on port 80
WebSocket server started on port 81
Stored params: min=390.00 max=480.00 latency=4 timeout=6090
Starting BLE scan for peripherals...
Status: connected=false, connHandle=0, authenticated=false, paired_devices=0, stored[min=390.00ms,max=480.00ms,sto=6090ms,lat=4], lastRequested[min=390.00ms,max=480.00ms,sto=6090ms,lat=4]
No connection - restarting scan...