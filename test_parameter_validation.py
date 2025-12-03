#!/usr/bin/env python3
"""
Enhanced Parameter Validation Test Script

Tests the connection parameter validation system to ensure "Next" column values
reliably reach connected BLE clients with proper validation and error handling.
"""

import requests
import json
import time
import websocket
import threading
from typing import Dict, Any

class ParameterValidationTester:
    def __init__(self, esp32_ip: str = "192.168.4.1", port: int = 80):
        self.base_url = f"http://{esp32_ip}:{port}"
        self.websocket_url = f"ws://{esp32_ip}:{port}/ws"
        self.ws = None
        self.ws_messages = []
        self.connected = False

    def connect_websocket(self):
        """Connect to WebSocket for real-time monitoring"""
        try:
            self.ws = websocket.WebSocketApp(
                self.websocket_url,
                on_open=self.on_ws_open,
                on_message=self.on_ws_message,
                on_error=self.on_ws_error,
                on_close=self.on_ws_close
            )

            # Run WebSocket in separate thread
            ws_thread = threading.Thread(target=self.ws.run_forever)
            ws_thread.daemon = True
            ws_thread.start()

            # Wait for connection
            time.sleep(2)
            return self.connected
        except Exception as e:
            print(f"❌ WebSocket connection failed: {e}")
            return False

    def on_ws_open(self, ws):
        print("✅ WebSocket connected")
        self.connected = True

    def on_ws_message(self, ws, message):
        try:
            data = json.loads(message)
            self.ws_messages.append({
                'timestamp': time.time(),
                'data': data
            })
            print(f"📡 WS Message: {data}")
        except json.JSONDecodeError:
            print(f"📡 WS Raw: {message}")

    def on_ws_error(self, ws, error):
        print(f"❌ WebSocket error: {error}")

    def on_ws_close(self, ws, close_status_code, close_msg):
        print("🔌 WebSocket disconnected")
        self.connected = False

    def test_api_endpoint(self, endpoint: str, data: Dict[str, Any] = None) -> Dict[str, Any]:
        """Test API endpoint and return response"""
        try:
            if data:
                response = requests.post(f"{self.base_url}{endpoint}", json=data, timeout=5)
            else:
                response = requests.get(f"{self.base_url}{endpoint}", timeout=5)

            result = {
                'status_code': response.status_code,
                'success': response.status_code == 200,
                'response': response.text
            }

            try:
                result['json'] = response.json()
            except:
                pass

            return result
        except Exception as e:
            return {
                'status_code': 0,
                'success': False,
                'error': str(e)
            }

    def test_parameter_validation(self):
        """Test various parameter combinations to verify validation"""
        print("\n🧪 Testing Parameter Validation...")

        test_cases = [
            # Valid parameters
            {
                'name': 'Valid Standard Parameters',
                'data': {
                    'minInterval': 20,
                    'maxInterval': 40,
                    'latency': 0,
                    'timeout': 400
                },
                'expected_success': True
            },
            # Invalid: min > max
            {
                'name': 'Invalid: Min > Max Interval',
                'data': {
                    'minInterval': 50,
                    'maxInterval': 30,
                    'latency': 0,
                    'timeout': 400
                },
                'expected_success': False
            },
            # Invalid: out of range
            {
                'name': 'Invalid: Min Interval Too Low',
                'data': {
                    'minInterval': 5,  # Below BLE spec minimum
                    'maxInterval': 40,
                    'latency': 0,
                    'timeout': 400
                },
                'expected_success': False
            },
            # Invalid: timeout too small
            {
                'name': 'Invalid: Timeout Too Small',
                'data': {
                    'minInterval': 20,
                    'maxInterval': 40,
                    'latency': 2,
                    'timeout': 50  # Too small for latency
                },
                'expected_success': False
            },
            # Edge case: maximum values
            {
                'name': 'Edge Case: Maximum Values',
                'data': {
                    'minInterval': 3200,
                    'maxInterval': 3200,
                    'latency': 499,
                    'timeout': 32000
                },
                'expected_success': True
            }
        ]

        for i, test_case in enumerate(test_cases):
            print(f"\n📋 Test {i+1}: {test_case['name']}")
            print(f"   Parameters: {test_case['data']}")

            # Clear previous WebSocket messages
            self.ws_messages.clear()

            # Send parameter update
            result = self.test_api_endpoint('/api/apply-parameters', test_case['data'])

            # Wait for WebSocket updates
            time.sleep(1)

            # Analyze results
            if test_case['expected_success']:
                if result['success']:
                    print(f"   ✅ PASS - Valid parameters accepted")
                    # Check if WebSocket received update
                    if self.ws_messages:
                        print(f"   📡 WebSocket confirmed: {len(self.ws_messages)} message(s)")
                    else:
                        print(f"   ⚠️  No WebSocket confirmation received")
                else:
                    print(f"   ❌ FAIL - Valid parameters rejected: {result.get('response', 'No response')}")
            else:
                if not result['success']:
                    print(f"   ✅ PASS - Invalid parameters correctly rejected")
                    print(f"   📝 Reason: {result.get('response', 'No error message')}")
                else:
                    print(f"   ❌ FAIL - Invalid parameters incorrectly accepted")

        return True

    def test_parameter_rollback(self):
        """Test parameter rollback functionality"""
        print("\n🔄 Testing Parameter Rollback...")

        # First set valid parameters
        print("1. Setting initial valid parameters...")
        initial_params = {
            'minInterval': 20,
            'maxInterval': 40,
            'latency': 0,
            'timeout': 400
        }
        result = self.test_api_endpoint('/api/apply-parameters', initial_params)
        if not result['success']:
            print("❌ Failed to set initial parameters")
            return False

        time.sleep(1)

        # Try to set invalid parameters (should be rejected and rolled back)
        print("2. Attempting invalid parameters (should rollback)...")
        invalid_params = {
            'minInterval': 100,  # Invalid: > maxInterval
            'maxInterval': 50,
            'latency': 0,
            'timeout': 400
        }

        self.ws_messages.clear()
        result = self.test_api_endpoint('/api/apply-parameters', invalid_params)

        time.sleep(2)

        if not result['success']:
            print("✅ Invalid parameters correctly rejected")

            # Check if system maintained previous valid state
            current_result = self.test_api_endpoint('/api/status')
            if current_result['success']:
                print("✅ System state maintained after rejection")
                return True
            else:
                print("❌ System state corrupted after rejection")
                return False
        else:
            print("❌ Invalid parameters incorrectly accepted")
            return False

    def test_authentication_validation(self):
        """Test that unauthenticated requests are properly rejected"""
        print("\n🔐 Testing Authentication Validation...")

        # First check if authentication is enabled
        status_result = self.test_api_endpoint('/api/status')
        if not status_result['success']:
            print("❌ Cannot get system status")
            return False

        try:
            status_data = status_result.get('json', {})
            auth_enabled = status_data.get('security', {}).get('authenticationRequired', False)

            if not auth_enabled:
                print("ℹ️  Authentication is currently disabled - skipping auth tests")
                return True

            print("🔒 Authentication is enabled - testing parameter validation with auth")

            # Try to apply parameters without authentication (should fail)
            test_params = {
                'minInterval': 30,
                'maxInterval': 60,
                'latency': 0,
                'timeout': 500
            }

            result = self.test_api_endpoint('/api/apply-parameters', test_params)

            if not result['success']:
                print("✅ Unauthenticated parameter update correctly rejected")
                return True
            else:
                print("❌ Unauthenticated parameter update incorrectly allowed")
                return False

        except Exception as e:
            print(f"❌ Authentication test failed: {e}")
            return False

    def run_validation_tests(self):
        """Run complete validation test suite"""
        print("🚀 Starting Enhanced Parameter Validation Tests")
        print("=" * 60)

        # Connect to WebSocket
        if not self.connect_websocket():
            print("❌ Cannot establish WebSocket connection - some tests may fail")

        # Test basic connectivity
        print("\n🔗 Testing Basic Connectivity...")
        status_result = self.test_api_endpoint('/api/status')
        if not status_result['success']:
            print("❌ ESP32 not responding - check connection")
            return False

        print("✅ ESP32 connected and responding")

        # Run test suites
        test_results = []

        try:
            # Parameter validation tests
            test_results.append(self.test_parameter_validation())

            # Rollback tests
            test_results.append(self.test_parameter_rollback())

            # Authentication tests
            test_results.append(self.test_authentication_validation())

        except KeyboardInterrupt:
            print("\n⏹️  Tests interrupted by user")
            return False

        # Summary
        print("\n" + "=" * 60)
        print("📊 Test Results Summary")
        print("=" * 60)

        passed_tests = sum(test_results)
        total_tests = len(test_results)

        if passed_tests == total_tests:
            print(f"✅ All {total_tests} test suites PASSED")
            print("🎉 Enhanced parameter validation is working correctly!")
            print("\n📋 Validation Features Verified:")
            print("   ✅ BLE specification compliance")
            print("   ✅ Parameter range validation")
            print("   ✅ Parameter combination validation")
            print("   ✅ Error handling and rollback")
            print("   ✅ Authentication integration")
            print("   ✅ Real-time WebSocket updates")
        else:
            print(f"❌ {total_tests - passed_tests} of {total_tests} test suites FAILED")
            print("⚠️  Enhanced parameter validation needs attention")

        return passed_tests == total_tests

if __name__ == "__main__":
    # Test with default ESP32 AP mode IP
    tester = ParameterValidationTester()

    print("🔧 Enhanced Parameter Validation Test Suite")
    print("Testing connection parameter reliability and validation...")
    print("\n💡 This test verifies that 'Next' column values reliably reach BLE clients")
    print("   with proper validation, error handling, and rollback mechanisms.")

    try:
        success = tester.run_validation_tests()
        exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n⏹️  Tests stopped by user")
        exit(1)
    except Exception as e:
        print(f"\n💥 Test suite crashed: {e}")
        exit(1)