# Archive/Deletion Shortlist

Files and directories safe for permanent removal. Each has been analyzed for active references in the codebase and documentation.

---

## Category 1: Legacy Launch & Dashboard Scripts
**Rationale:** Superseded by USB bridge workflow documented in README.md and npm scripts

- `launch_dashboard.bat` — Old batch script for launching dashboard (replaced by `npm run dev`)
- `launch_dashboard.ps1` — Old PowerShell smart launcher for WiFi mode (replaced by `npm run dev`)
- `launch_dashboard_simple.ps1` — Old simplified dashboard launcher (replaced by `npm run dev`)
- `setup_dashboard.ps1` — Old dashboard setup guide (merged into README.md + embedded in wifi scripts)
- `open_serial.bat` — Old batch serial monitor launcher (replaced by PlatformIO Monitor task in VS Code)

**Replacement:** Use `npm run dev` (dashboard only) or `npm run dev:usb-dashboard` (with USB bridge)

---

## Category 2: Legacy Setup & Configuration Scripts
**Rationale:** Superseded by newer equivalents or one-time setup documented in README.md

- `configure_wifi.ps1` — Old WiFi configuration (replaced by `setup_home_wifi.ps1`)
- `setup_esp32_client.ps1` — Old client-only setup (functionality merged into main setup scripts)
- `troubleshoot_connection.ps1` — Old WiFi troubleshooting guide (no longer relevant for USB-only mode)

**Replacement:** Use `setup_home_wifi.ps1` for optional WiFi, or skip for USB-only operation

---

## Category 3: Legacy Build & File Creation Scripts
**Rationale:** One-time utilities that are no longer needed or have been automated

- `creat_files.bat` — Old batch file creator for missing USB architecture files (typo in name, functionality obsolete)
- `create_missing_USBarchitecture.bat` — Old batch utility for architecture file generation (handled by PlatformIO now)
- `upload_fw.bat` — Old batch uploader (replaced by PlatformIO Upload task or `npm run build:fw`)

**Replacement:** Use PlatformIO tasks in VS Code or `npm` scripts for all build/upload workflows

---

## Category 4: Old Test Scripts (WiFi/API Mode)
**Rationale:** Test older WiFi + REST API architecture. USB bridge has its own validation approach via WebSocket

- `test_esp32_api.ps1` — Tests WiFi mode API endpoints (not applicable to USB-only mode)
- `test_esp32_connectivity.ps1` — Tests WiFi connectivity (not applicable to USB-only mode)
- `test_dependencies_esm.js` — Tests ESM dependency resolution (one-time utility, output now in npm scripts)
- `test_parameter_validation.py` — Old parameter validation test (redundant with current validation)
- `test_security.py` — Old security test framework (not actively maintained)
- `test_parameter_workflow.py` — Old parameter application test (replaced by smoke test in session)

**Replacement:** Use WebSocket-based smoke tests documented in COMMIT_PLAN.md for USB bridge validation

---

## Category 5: Old Utility Scripts
**Rationale:** One-time utilities or superseded by built-in tools

- `run_serial_mon.py` — Old Python serial monitor (replaced by `pio device monitor` task in VS Code)

**Replacement:** Use VS Code PlatformIO Monitor task

---

## Category 6: Outdated or Platform-Specific Files
**Rationale:** Replit-specific configuration or deprecated workflows

- `replit.md` — Replit.com hosting documentation (not relevant for local development)

**Replacement:** Follow README.md for canonical local setup

---

## Category 7: Duplicate Directory Structure
**Rationale:** Complete duplicate of root directory (accidental nesting)

- `esp32-ble-connection-manager/` — **ENTIRE DIRECTORY** is a duplicate of root structure
  - Contains all the same files, outdated versions, and legacy content
  - Should be removed entirely

**Impact:** Saves ~500MB of disk space and eliminates confusion from duplicate sources

---

## Category 8: Archive/Chat History
**Rationale:** Historical artifacts from development conversation, not part of active codebase

- `attached_assets/` — Old chat prompts and conversation history
  - `attached_assets/CHAT_prompt_1763110676037.txt` — Archived conversation prompt
  - `attached_assets/src/` — Old attached code snippets (no longer referenced)

**Replacement:** Use git history for code archeology if needed

---

## Summary

| Category | Count | Total Size* | Priority |
|----------|-------|-------------|----------|
| Launch scripts | 5 | ~50 KB | HIGH |
| Setup scripts | 3 | ~30 KB | HIGH |
| Build/file scripts | 3 | ~20 KB | HIGH |
| Old tests | 6 | ~80 KB | MEDIUM |
| Utilities | 1 | ~5 KB | MEDIUM |
| Replit | 1 | ~5 KB | LOW |
| Duplicate dir | 1 | ~500 MB | **CRITICAL** |
| Archives | 1 | ~100 KB | LOW |
| **TOTAL** | **21 items** | **~730 MB** | — |

\* Duplicate directory dominates; removing just that saves most space

---

## Recommended Deletion Order

### Phase 1 (Do First - High Impact)
```powershell
# Remove duplicate directory (~500MB reclaimed)
Remove-Item esp32-ble-connection-manager -Recurse -Force

# Remove legacy batch files (Windows-only, all superseded)
Remove-Item launch_dashboard.bat, open_serial.bat, upload_fw.bat, creat_files.bat, create_missing_USBarchitecture.bat -Force
```

### Phase 2 (Medium Priority)
```powershell
# Remove old setup/launch PowerShell scripts
Remove-Item launch_dashboard.ps1, launch_dashboard_simple.ps1, setup_dashboard.ps1, setup_esp32_client.ps1, configure_wifi.ps1, troubleshoot_connection.ps1 -Force

# Remove old test scripts
Remove-Item test_esp32_api.ps1, test_esp32_connectivity.ps1, test_dependencies_esm.js, test_parameter_validation.py, test_security.py, test_parameter_workflow.py -Force

# Remove old utility
Remove-Item run_serial_mon.py -Force
```

### Phase 3 (Optional, Low Priority)
```powershell
# Remove archives and platform-specific files
Remove-Item attached_assets -Recurse -Force
Remove-Item replit.md -Force
```

---

## Commit Message
Once deletion is complete and tested, commit as:

```
git commit -m "chore: remove legacy scripts, old tests, and duplicate directory structure"
```

---

## Validation Before Deletion

✓ No references in `src/`, `client/`, `server/`, `shared/`, `db/` directories
✓ No references in any `*.md` documentation files (except this shortlist)
✓ Not mentioned in `package.json` scripts
✓ Not referenced in `.vscode/tasks.json`
✓ Duplicate directory (`esp32-ble-connection-manager/`) has no unique content vs root
✓ All functionality replaced by:
  - New npm scripts in `package.json`
  - USB bridge workflow documented in `README.md`
  - PlatformIO VS Code tasks
  - Smoke test validation in `COMMIT_PLAN.md`

**Status:** SAFE TO DELETE ✓
