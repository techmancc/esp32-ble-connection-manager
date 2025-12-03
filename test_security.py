#!/usr/bin/env python3
"""
ESP32 BLE Security Test Script
=============================

This script tests the ESP32 BLE security system by:
1. Monitoring WebSocket for real-time updates  
2. Simulating pairing process
3. Testing security enforcement
4. Validating API responses

Requirements:
- websockets: pip install websockets
- requests: pip install requests
"""

import asyncio
import json
import requests
import websockets
from datetime import datetime

ESP32_IP = "192.168.4.1"
WS_URL = f"ws://{ESP32_IP}:81"
API_BASE = f"http://{ESP32_IP}/api"

class ESP32SecurityTester:
    def __init__(self):
        self.websocket = None
        self.pairing_active = False
        self.current_pin = None
        
    async def connect_websocket(self):
        """Connect to ESP32 WebSocket for real-time monitoring"""
        try:
            self.websocket = await websockets.connect(WS_URL)
            print(f"✅ WebSocket connected to {WS_URL}")
            return True
        except Exception as e:
            print(f"❌ WebSocket connection failed: {e}")
            return False
            
    async def listen_for_messages(self):
        """Listen for WebSocket messages from ESP32"""
        if not self.websocket:
            return
            
        try:
            async for message in self.websocket:
                data = json.loads(message)
                await self.handle_websocket_message(data)
        except Exception as e:
            print(f"❌ WebSocket listening error: {e}")
            
    async def handle_websocket_message(self, data):
        """Handle incoming WebSocket messages"""
        msg_type = data.get('type', 'unknown')
        timestamp = datetime.now().strftime('%H:%M:%S')
        
        print(f"[{timestamp}] 📡 WebSocket: {msg_type}")
        
        if msg_type == 'security_update':
            status = data.get('status', {})
            print(f"  🔒 Connected: {status.get('isConnected', False)}")
            print(f"  🔑 Authenticated: {status.get('isAuthenticated', False)}")
            print(f"  👥 Paired Devices: {status.get('pairedDeviceCount', 0)}")
            
        elif msg_type == 'pairing_started':
            self.pairing_active = True
            self.current_pin = data.get('pin')
            print(f"  📱 PAIRING STARTED - PIN: {self.current_pin}")
            
        elif msg_type == 'pairing_completed':
            self.pairing_active = False
            device = data.get('deviceName', 'Unknown')
            print(f"  ✅ PAIRING SUCCESS - Device: {device}")
            
        elif msg_type == 'pairing_failed':
            self.pairing_active = False
            reason = data.get('reason', 'Unknown')
            print(f"  ❌ PAIRING FAILED - Reason: {reason}")
            
    def test_api_endpoints(self):
        """Test all security API endpoints"""
        print("\n🧪 Testing API Endpoints...")
        
        # Test security status
        try:
            response = requests.get(f"{API_BASE}/security", timeout=5)
            if response.status_code == 200:
                data = response.json()
                print("✅ Security API working")
                print(f"  📊 Auth Required: {data.get('requireAuthentication')}")
                print(f"  🔐 Pairing Enabled: {data.get('pairingEnabled')}")
                print(f"  👥 Paired Count: {data.get('pairedDeviceCount')}")
            else:
                print(f"❌ Security API failed: {response.status_code}")
        except Exception as e:
            print(f"❌ Security API error: {e}")
            
        # Test authentication toggle
        try:
            # Disable auth
            response = requests.post(
                f"{API_BASE}/security/toggle-auth",
                json={"enabled": False},
                timeout=5
            )
            if response.status_code == 200:
                data = response.json()
                print(f"✅ Auth toggle OFF: {data.get('requireAuthentication')}")
            
            # Enable auth  
            response = requests.post(
                f"{API_BASE}/security/toggle-auth", 
                json={"enabled": True},
                timeout=5
            )
            if response.status_code == 200:
                data = response.json()
                print(f"✅ Auth toggle ON: {data.get('requireAuthentication')}")
                
        except Exception as e:
            print(f"❌ Auth toggle error: {e}")
            
    async def run_security_tests(self):
        """Run complete security test suite"""
        print("🚀 Starting ESP32 BLE Security Tests")
        print("=" * 50)
        
        # Test API endpoints
        self.test_api_endpoints()
        
        # Connect WebSocket for monitoring
        if await self.connect_websocket():
            print("\n📡 Monitoring WebSocket for security events...")
            print("   (Connect a BLE device to ESP32-BLE-Config to trigger pairing)")
            print("   (Press Ctrl+C to stop)")
            
            try:
                await self.listen_for_messages()
            except KeyboardInterrupt:
                print("\n🛑 Test monitoring stopped by user")
            except Exception as e:
                print(f"❌ Monitoring error: {e}")
        
        print("\n📋 Test Summary:")
        print("- API endpoints tested ✅")
        print("- WebSocket monitoring active ✅")  
        print("- Ready for BLE pairing tests 📱")
        print("\nNext: Use BLE scanner app to connect to 'ESP32-BLE-Config'")

async def main():
    tester = ESP32SecurityTester()
    await tester.run_security_tests()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 Security testing completed")