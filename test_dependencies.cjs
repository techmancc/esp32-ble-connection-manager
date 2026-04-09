console.log('🔍 Testing USB Bridge Dependencies (CommonJS)...');
console.log('==================================================');

const testDeps = [
    'serialport',
    '@serialport/parser-readline',
    'ws',
    'express',
    'cors',
    'concurrently'
];

let allGood = true;

testDeps.forEach(dep => {
    try {
        require(dep);
        console.log(`✅ ${dep}`);
    } catch (error) {
        console.log(`❌ ${dep} - ${error.message}`);
        allGood = false;
    }
});

if (allGood) {
    console.log('\n🎉 All dependencies ready!');
    console.log('\n📋 Next steps for USB architecture:');
    console.log('   1. Upload firmware: upload_firmware.bat');
    console.log('   2. Test USB connection: node test_usb_connection.js');
    console.log('   3. Start bridge & dashboard: npm run dev');
} else {
    console.log('\n🔧 Install missing dependencies:');
    console.log('npm install serialport @serialport/parser-readline ws express cors concurrently');
}

process.exit(allGood ? 0 : 1);