console.log('🔍 Testing USB Bridge Dependencies...');
console.log('===================================');

try {
    const { SerialPort } = require('serialport');
    console.log('✅ serialport module loaded');

    const { ReadlineParser } = require('@serialport/parser-readline');
    console.log('✅ parser-readline module loaded');

    const WebSocket = require('ws');
    console.log('✅ ws module loaded');

    const express = require('express');
    console.log('✅ express module loaded');

    const cors = require('cors');
    console.log('✅ cors module loaded');

    const concurrently = require('concurrently');
    console.log('✅ concurrently module loaded');

    console.log('\n🎉 All dependencies are ready!');
    console.log('📋 Next steps:');
    console.log('   1. Upload firmware: upload_firmware.bat');
    console.log('   2. Test connection: node test_usb_connection.js');
    console.log('   3. Start dashboard: npm run dev');

} catch (error) {
    console.log(`❌ Missing dependency: ${error.message}`);
    console.log('🔧 Dependencies should already be installed');
    console.log('💡 If issues persist, try: npm install --force');
}