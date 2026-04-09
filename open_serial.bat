import { SerialPort } from 'serialport';
import { ReadlineParser } from '@serialport/parser-readline';

async function testESP32Connection() {
    console.log('🔍 Testing ESP32 USB Connection...');
    console.log('==================================');
    
    try {
        // List all available ports
        const ports = await SerialPort.list();
        console.log('\n📡 Available Serial Ports:');
        ports.forEach((port, index) => {
            console.log(`  ${index + 1}. ${port.path} - ${port.manufacturer || 'Unknown'}`);
        });
        
        // Find ESP32 port
        const esp32Port = ports.find(port => 
            port.manufacturer && 
            (port.manufacturer.toLowerCase().includes('silicon labs') || 
             port.manufacturer.toLowerCase().includes('ftdi') ||
             port.manufacturer.toLowerCase().includes('ch340') ||
             port.manufacturer.toLowerCase().includes('ch343') ||
             port.manufacturer.toLowerCase().includes('cp210'))
        );
        
        if (!esp32Port) {
            console.log('\n❌ ESP32 not found!');
            console.log('💡 Make sure ESP32 is connected and drivers are installed');
            console.log('Available manufacturers:', ports.map(p => p.manufacturer).filter(Boolean).join(', '));
            return false;
        }
        
        console.log(`\n✅ Found ESP32 at: ${esp32Port.path}`);
        console.log(`📋 Manufacturer: ${esp32Port.manufacturer}`);
        
        // Test connection
        console.log('\n🔌 Testing serial connection...');
        
        const serialPort = new SerialPort({
            path: esp32Port.path,
            baudRate: 115200
        });
        
        const parser = serialPort.pipe(new ReadlineParser({ delimiter: '\n' }));
        
        let connected = false;
        let messageReceived = false;
        let testTimeout;
        
        return new Promise((resolve) => {
            parser.on('data', (data) => {
                const message = data.trim();
                console.log(`📨 ESP32: ${message}`);
                messageReceived = true;
                
                if (message.includes('ESP32') || 
                    message.includes('Ready') || 
                    message.includes('PONG') ||
                    message.includes('BLE') ||
                    message.includes('Parameter')) {
                    connected = true;
                    clearTimeout(testTimeout);
                    console.log('\n🎉 ESP32 USB connection successful!');
                    setTimeout(() => {
                        serialPort.close();
                        resolve(true);
                    }, 1000);
                }
            });
            
            serialPort.on('open', () => {
                console.log('🔗 Serial port opened successfully');
                
                // Send ping command after a short delay
                setTimeout(() => {
                    console.log('📤 Sending PING command...');
                    serialPort.write('PING\n');
                }, 2000);
                
                // Test timeout
                testTimeout = setTimeout(() => {
                    if (messageReceived) {
                        console.log('\n✅ ESP32 is responding (firmware may need updating)');
                        connected = true;
                    } else {
                        console.log('\n⏱️ Connection test timeout - no response from ESP32');
                        console.log('💡 ESP32 might need firmware upload or reset');
                    }
                    serialPort.close();
                    resolve(connected || messageReceived);
                }, 8000);
            });
            
            serialPort.on('error', (err) => {
                console.log(`\n❌ Serial connection error: ${err.message}`);
                clearTimeout(testTimeout);
                resolve(false);
            });
        });
        
    } catch (error) {
        console.log(`\n❌ Test failed: ${error.message}`);
        return false;
    }
}

// Run the test
testESP32Connection().then(success => {
    if (success) {
        console.log('\n🚀 USB connection is working!');
        console.log('📋 Next steps:');
        console.log('   • Upload latest firmware: upload_firmware.bat');
        console.log('   • Start bridge server: npm run dev');
        console.log('   • Access dashboard: http://localhost:3000');
    } else {
        console.log('\n🔧 Troubleshooting steps:');
        console.log('   1. Check USB cable is connected properly');
        console.log('   2. Upload firmware: upload_firmware.bat');
        console.log('   3. Press ESP32 reset button');
        console.log('   4. Run this test again: node test_usb_connection.js');
    }
    process.exit(success ? 0 : 1);
});							