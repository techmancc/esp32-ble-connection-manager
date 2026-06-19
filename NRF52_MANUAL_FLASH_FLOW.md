# nRF52 Manual Flash Flow (Workspace Root)

Canonical setup and run commands are maintained in `README.md`. This file is specific to nRF52 artifact-only build and manual flashing.

This workspace contains multiple modules. The firmware project is in:

- `esp32-ble-connection-manager/`

Use this root-level wrapper flow to build firmware artifacts without uploading.

## Build artifacts only (no upload)

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_nrf52_fw_manual.ps1
```

Or run VS Code task:

- `nRF52: Build FW Artifacts (Manual Flash)`

## Output location

Artifacts are generated inside the nested firmware project:

- `esp32-ble-connection-manager/artifacts/nrf52-manual/<timestamp>/`

Use `firmware.hex` in nRF Connect Desktop Programmer.
