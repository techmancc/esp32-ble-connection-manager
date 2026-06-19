# Staged Commit Plan

## Overview
Current working directory contains ~27 modified/new files across firmware, infrastructure, documentation, and UI layers. This plan groups changes into logical stages for clean git history and easier review/rollback.

---

## Commit 1: Workspace Infrastructure & Cleanup Automation
**Theme:** Development tools and build configuration
**Files:**
- `package.json` (add `clean:workspace` scripts)
- `platformio.ini` (add USB CDC on boot flags, debug env config)
- `scripts/clean-workspace.mjs` (new cleanup automation)
- `NRF52_MANUAL_FLASH_FLOW.md` (new manual flash guide)

**Rationale:** Foundational changes that enable subsequent work; safe to merge independently and useful for developers immediately.

**Commands:**
```bash
git add package.json platformio.ini scripts/clean-workspace.mjs NRF52_MANUAL_FLASH_FLOW.md
git commit -m "chore: add workspace cleanup automation and USB build configuration"
```

---

## Commit 2: USB Bridge & BLE-Only Mode Feature
**Theme:** USB serial communication and wireless-only operation
**Files:**
- `src/main.cpp` (add USB command handler, BLE-USB-only mode, parameter apply refactor)
- `scripts/free-port-and-run-usb-bridge.mjs` (port management)
- `scripts/usb_dashboard_bridge.ts` (USB bridge HTTP/WebSocket server)
- `scripts/wait-for-bridge-and-run-vite.mjs` (startup orchestration)
- `README.md` (new top-level canonical commands and USB workflow)

**Rationale:** Core feature enabling USB-only operation without WiFi. Large but cohesive change set. Includes all necessary supporting infrastructure and documentation.

**Commands:**
```bash
git add src/main.cpp scripts/free-port-and-run-usb-bridge.mjs \
  scripts/usb_dashboard_bridge.ts scripts/wait-for-bridge-and-run-vite.mjs README.md
git commit -m "feat: add USB-only mode with serial bridge and dashboard integration"
```

---

## Commit 3: Documentation & Configuration Updates
**Theme:** Docs, IDE settings, and developer notes
**Files:**
- `.vscode/c_cpp_properties.json` (editor C/C++ config)
- `.vscode/launch.json` (debugger config)
- `.vscode/tasks.json` (VS Code task definitions)
- `CLIENT_ESP32_SETUP.md`
- `CONNECTION_GUIDE.md`
- `DASHBOARD_ACCESS.md`
- `ESP32_AP_MODE_GUIDE.md`
- `ESP32_CLIENT_ARCHITECTURE.md`
- `ESP32_CLIENT_INTEGRATION.md`
- `ESP32_INTEGRATION_GUIDE.md`
- `SECURITY_TESTING_GUIDE.md`
- `WIFI_UPGRADE_SUMMARY.md`
- `websocket_test.html` (test utility)
- `client/client instructions.txt` (development notes)
- `client/readme_craig_starting _client.txt` (setup notes)

**Rationale:** All documentation and editor configuration; non-code changes that improve developer experience and project clarity. Can be reviewed and merged independently.

**Commands:**
```bash
git add .vscode/ CLIENT_ESP32_SETUP.md CONNECTION_GUIDE.md DASHBOARD_ACCESS.md \
  ESP32_*.md SECURITY_TESTING_GUIDE.md WIFI_UPGRADE_SUMMARY.md websocket_test.html \
  "client/client instructions.txt" "client/readme_craig_starting _client.txt"
git commit -m "docs: update project guides, IDE config, and developer notes"
```

---

## Commit 4: Dashboard UI Components
**Theme:** React dashboard interface updates
**Files:**
- `client/src/components/BleClientControlPanel.tsx`
- `client/src/lib/utils.ts`
- `client/src/pages/Dashboard.tsx`

**Rationale:** UI layer changes; isolated from firmware and bridge logic. Can be tested independently in dev server.

**Commands:**
```bash
git add client/src/components/BleClientControlPanel.tsx client/src/lib/utils.ts \
  client/src/pages/Dashboard.tsx
git commit -m "ui: update dashboard components and utilities for USB bridge integration"
```

---

## Testing Before Commit
**Recommended validation order:**
1. After Commit 1: `npm run clean:workspace` (verify cleanup automation)
2. After Commit 2: Run firmware build and USB bridge smoke test
3. After Commit 3: Verify docs render and IDE settings are recognized
4. After Commit 4: `npm run dev` (verify dashboard startup)

**Smoke test example (after Commit 2):**
```bash
npm run dev:usb-dashboard
# In another terminal:
node -e "const ws=require('ws'); const s=new ws('ws://127.0.0.1:8787/ws'); s.on('open',()=>{s.send(JSON.stringify({type:'ble_command',command:'start_scan'})); setTimeout(()=>process.exit(0),3000);})"
```

---

## Rollback Strategy
If any commit causes issues:
```bash
git revert <commit-hash>
# Or reset to pre-commit state:
git reset --hard HEAD~1
```

Each commit is independently functional and can be reverted without affecting others.
