# 🔐 ESP32 BLE Security Testing Protocol

Canonical setup and run commands are maintained in `README.md`. Use that file first, then execute this security test protocol.

This script provides comprehensive testing procedures for the ESP32 BLE Connection Parameter System with Security features.

## 🎯 **Testing Overview**

**System Under Test**: ESP32-S3 BLE Peripheral with Security
- **Device Name**: `ESP32-BLE-Config` 
- **WiFi Network**: `ESP32-BLE-Control-9e9ef0`
- **Password**: `esp32ble`
- **Dashboard**: `http://192.168.4.1/`

---

## ✅ **Phase 1: Basic Security Status Verification**

### 1.1 Connect to WiFi Network
```powershell
# Connect to ESP32 WiFi
# Network: ESP32-BLE-Control-9e9ef0
# Password: esp32ble
```

### 1.2 Verify Web Dashboard Access
```powershell
# Open browser and navigate to:
# http://192.168.4.1/
# Should show the enhanced dashboard with Security Panel
```

### 1.3 Test Security API Endpoint
```powershell
# Test the security status API
curl "http://192.168.4.1/api/security"
```

**Expected Response**: JSON with security status, paired devices count (0), authentication state

---

## 🔐 **Phase 2: BLE Pairing Process Testing**

### 2.1 BLE Device Discovery
**Tools Needed**: 
- Smartphone with BLE scanner app (nRF Connect, LightBlue, etc.)
- Or Windows BLE scanner
- Or nRF Connect desktop application

**Steps**:
1. Open BLE scanner app
2. Look for device named: `ESP32-BLE-Config`
3. Verify device appears in scan results

### 2.2 Pairing Initiation Test
**Steps**:
1. **Dashboard Monitor**: Keep `http://192.168.4.1/` open in browser
2. **BLE Connect**: Attempt to connect to `ESP32-BLE-Config` via BLE scanner
3. **Watch Dashboard**: Should show "Pairing in Progress" card appear
4. **PIN Display**: Dashboard should display the pairing PIN in large font
5. **Mobile Entry**: Enter the PIN shown on dashboard into your mobile device

**Expected Behavior**:
- Dashboard shows real-time pairing status
- PIN appears prominently in blue card
- PIN expires after 30 seconds
- Success/failure notifications appear

### 2.3 Pairing Success Verification
**After successful pairing**:
1. **Dashboard Check**: Security panel should show:
   - Connection Status: "Authenticated" (green)
   - Current Device: Device MAC address
   - Paired Devices: 1/10
2. **Serial Monitor**: Should show pairing success messages
3. **Persistent Storage**: Device should be saved in ESP32 memory

---

## 🛡️ **Phase 3: Security Enforcement Testing**

### 3.1 Test Without Authentication Requirement
**Initial State**: Authentication requirement OFF

**Steps**:
1. **Connect BLE Device**: Ensure device is paired and connected
2. **Parameter Change**: Try changing connection parameters via web dashboard
3. **Verify**: Parameters should change successfully without additional security checks

**Expected**: Parameter changes work normally

### 3.2 Enable Authentication Requirement
**Steps**:
1. **Dashboard**: Navigate to Security Panel
2. **Toggle Switch**: Turn ON "Require Authentication"
3. **Verify**: Switch shows enabled state
4. **API Test**: 
```powershell
curl -X POST "http://192.168.4.1/api/security/toggle-auth" -H "Content-Type: application/json" -d "{\"enabled\": true}"
```

### 3.3 Test Authentication Enforcement
**With Authentication ON**:

**Test A - Authenticated Device**:
1. **Ensure**: BLE device is paired and connected
2. **Parameter Change**: Attempt to change connection parameters
3. **Expected**: Changes should succeed

**Test B - Unauthenticated Device**:
1. **Disconnect**: Disconnect BLE device
2. **Parameter Change**: Try changing parameters via web dashboard
3. **Expected**: Should show warning about authentication requirement

**Test C - Unpaired Device**:
1. **Remove Pairing**: Use dashboard to remove paired device
2. **BLE Reconnect**: Try to reconnect with BLE (will be unauthenticated)
3. **Parameter Change**: Attempt parameter changes
4. **Expected**: Should be blocked due to authentication requirement

---

## 🗑️ **Phase 4: Device Management Testing**

### 4.1 Multiple Device Pairing
**Steps**:
1. **Pair Device 1**: Complete pairing with first device
2. **Pair Device 2**: Use second device/computer to pair
3. **Dashboard Check**: Should show 2 devices in paired devices list
4. **Verify**: Each device shows name, MAC address, pairing timestamp

### 4.2 Device Removal Testing
**Steps**:
1. **Select Device**: Choose a paired device from the list
2. **Remove**: Click the trash icon next to device
3. **Confirm**: Device should disappear from list
4. **Verify**: Paired device count decreases
5. **Test Connection**: Removed device should need to re-pair

### 4.3 Active Device Identification
**Steps**:
1. **Multiple Pairs**: Ensure multiple devices are paired
2. **Connect One**: Have only one device connected via BLE
3. **Dashboard Check**: Connected device should show "Active" badge
4. **Switch Devices**: Connect different device
5. **Verify**: "Active" badge moves to new device

---

## 📡 **Phase 5: Real-time Updates Testing**

### 5.1 WebSocket Connectivity Test
**Steps**:
1. **Open Dashboard**: In browser
2. **BLE Connect**: Connect device via BLE
3. **Real-time Check**: Security panel should update immediately without page refresh
4. **Status Changes**: Connection/disconnection should reflect instantly

### 5.2 Security Event Broadcasting
**Events to Test**:
- Pairing started → PIN display appears
- Pairing completed → Success notification + device list update
- Pairing failed → Error notification
- Device connected/disconnected → Status badge changes
- Authentication toggle → Real-time enforcement changes

---

## 🔧 **Phase 6: Edge Case Testing**

### 6.1 Connection Limits
**Test**: Try pairing more than 10 devices (system limit)
**Expected**: Should handle gracefully, possibly reject 11th device

### 6.2 PIN Timeout Testing
**Steps**:
1. **Initiate Pairing**: Start BLE connection
2. **Wait**: Don't enter PIN for 30+ seconds
3. **Verify**: Pairing should timeout and fail gracefully

### 6.3 Rapid Connect/Disconnect
**Steps**:
1. **Rapid Cycle**: Quickly connect and disconnect BLE device multiple times
2. **Monitor**: Dashboard should handle rapid state changes
3. **Verify**: No crashes or stuck states

### 6.4 Network Interruption
**Steps**:
1. **WiFi Disconnect**: Temporarily disconnect from ESP32 WiFi
2. **BLE Activity**: Perform BLE operations while WiFi down
3. **WiFi Reconnect**: Reconnect WiFi
4. **Sync Check**: Dashboard should resync with current state

---

## 📊 **Expected Test Results Summary**

| Test Phase | Key Metrics | Success Criteria |
|------------|-------------|------------------|
| **Basic Status** | API Response | JSON with security status |
| **BLE Discovery** | Device Visible | `ESP32-BLE-Config` appears |
| **Pairing Process** | PIN Display | Real-time PIN on dashboard |
| **Security Enforcement** | Auth Blocking | Parameter changes blocked when required |
| **Device Management** | CRUD Operations | Add/remove devices successfully |
| **Real-time Updates** | WebSocket Sync | Instant status updates |

---

## 🚨 **Troubleshooting Common Issues**

### Issue: Can't see BLE device
**Solutions**:
- Ensure ESP32 is advertising (check serial monitor)
- Restart BLE scanner app
- Check device compatibility with BLE 4.0+

### Issue: Pairing fails
**Solutions**:
- Check PIN entry timing (30-second limit)
- Verify correct PIN from dashboard
- Clear BLE cache on mobile device

### Issue: Dashboard not updating
**Solutions**:
- Check WiFi connection to ESP32
- Refresh browser page
- Verify WebSocket connection in browser dev tools

### Issue: Authentication not enforcing
**Solutions**:
- Verify toggle is enabled in dashboard
- Check device is properly paired
- Test with completely new/unpaired device

---

## ✅ **Test Completion Checklist**

- [ ] Basic WiFi and dashboard connectivity
- [ ] BLE device discovery and visibility
- [ ] Successful PIN-based pairing process
- [ ] Real-time pairing status display
- [ ] Authentication requirement toggle
- [ ] Security enforcement validation
- [ ] Multiple device pairing (2+ devices)
- [ ] Device removal functionality
- [ ] Active device identification
- [ ] Real-time WebSocket updates
- [ ] Edge case handling (timeouts, limits)
- [ ] Network interruption recovery

**Test Environment**: 
- ESP32-S3 with 16MB Flash, 8MB PSRAM
- NimBLE-Arduino v1.4.3 with security features
- React dashboard with SecurityPanel component
- BLE 4.0+ capable test devices

**Test Date**: ___________
**Tester**: ___________
**Results**: ___________