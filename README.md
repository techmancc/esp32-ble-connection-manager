# ESP32 BLE Connection Manager

This repository combines firmware, dashboard, and helper tooling for BLE connection parameter management.

## What Is In This Workspace

- ESP32 firmware (PlatformIO) in workspace root
- React dashboard (Vite) served from `client/` via root `package.json`
- Optional Node server code in `server/` for API/WebSocket workflows
- nRF52 manual artifact build wrapper in `scripts/build_nrf52_fw_manual.ps1`

## Canonical Quick Start

Use these steps as the default workflow. Other docs should defer to this page for setup/run commands.

### 1. Install JS dependencies

```powershell
npm install
```

### 2. Build ESP32 firmware

```powershell
$env:Path += ";$HOME/.platformio/penv/Scripts"
pio run -e esp32s3-n16r8
```

VS Code task alternative: `PlatformIO: Build`.

### 3. Upload ESP32 firmware

```powershell
$env:Path += ";$HOME/.platformio/penv/Scripts"
pio run -e esp32s3-n16r8 -t upload
```

VS Code task alternative: `PlatformIO: Upload`.

### 4. Configure ESP32 networking (optional but recommended)

```powershell
./setup_home_wifi.ps1
```

Fallback AP mode details are in `ESP32_AP_MODE_GUIDE.md`.

### 5. Start dashboard development server

```powershell
npm run dev
```

Use hosted LAN mode if needed:

```powershell
npm run dev:host
```

Open `http://localhost:5173`.

### 6. Monitor serial output (optional)

```powershell
$env:Path += ";$HOME/.platformio/penv/Scripts"
pio device monitor
```

VS Code task alternative: `PlatformIO: Monitor`.

## BLE-Only USB Branch Mode (No WiFi)

This branch is configured for BLE-only wireless operation:

- BLE remains enabled for central scanning/connection management.
- WiFi is forced off at startup.
- HTTP API and WebSocket services are not started.
- Control is done over USB serial at 115200 baud.

### Browser UI (No manual USB typing)

Run the dashboard plus USB bridge together:

```powershell
npm run dev:usb-dashboard
```

Then open `http://localhost:5173`.

How it works:

- Browser talks to `http://127.0.0.1:8787` for REST.
- Browser WebSocket connects to `ws://127.0.0.1:8787/ws`.
- Bridge translates dashboard actions to ESP32 USB serial commands.
- ESP32 remains WiFi-off in this branch.

New in this branch:

- USB bridge status banner in the dashboard header area.
- Live serial status: connected/disconnected, selected COM port, baud, last error.
- Auto-port selection button (bridge picks likely ESP32 COM port automatically).
- Manual port selector with one-click switch to selected COM port.

If the bridge shows COM access errors, close any other serial monitor/tool first.

### USB command reference (optional)

On firmware boot, you can still send these USB commands from serial monitor:

```text
HELP
PING
GET_STATE
SCAN_NOW
CONNECT <ble-address>
DISCONNECT
SET_FILTER <name-substring>
CLEAR_FILTER
SET_NEXT <minMs> <maxMs> <latency> <timeoutMs>
APPLY
```

Example sequence:

```text
SET_NEXT 270 340 7 5990
APPLY
GET_STATE
```

This mode guarantees there is no WiFi transport path active on the ESP32.

## Other Common Commands

```powershell
npm run check
npm run build
npm run dev:server
```

nRF52 artifact-only build:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/build_nrf52_fw_manual.ps1
```

Or run VS Code task: `nRF52: Build FW Artifacts (Manual Flash)`.

## Documentation Map

- `CONNECTION_GUIDE.md`: practical connection steps
- `DASHBOARD_ACCESS.md`: dashboard access modes
- `CLIENT_ESP32_SETUP.md`: client/ESP32 integration reference
- `ESP32_CLIENT_INTEGRATION.md`: API compatibility details
- `ESP32_INTEGRATION_GUIDE.md`: firmware-side integration details
- `ESP32_AP_MODE_GUIDE.md`: AP-mode workflow
- `WIFI_UPGRADE_SUMMARY.md`: network architecture notes
- `SECURITY_TESTING_GUIDE.md`: BLE security validation checklist
- `NRF52_MANUAL_FLASH_FLOW.md`: nRF52 manual flashing artifact flow

## Notes

- Run dashboard commands from workspace root, not from `client/`.
- There is no separate `client/package.json` in this workspace.
- This workspace also contains a nested copy under `esp32-ble-connection-manager/`; root-level docs are the canonical set.
