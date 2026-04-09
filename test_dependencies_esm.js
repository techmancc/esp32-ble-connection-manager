console.log('🔍 Testing USB Bridge Dependencies (ES Modules)...');
console.log('===================================================');

async function testDependencies() {
    const dependencies = [
        { name: 'serialport', specifier: 'serialport' },
        { name: '@serialport/parser-readline', specifier: '@serialport/parser-readline' },
        { name: 'ws', specifier: 'ws' },
        { name: 'express', specifier: 'express' },
        { name: 'cors', specifier: 'cors' },
        { name: 'concurrently', specifier: 'concurrently' }
    ];

    let allGood = true;

    for (const dep of dependencies) {
        try {
            await import(dep.specifier);
            console.log(`✅ ${dep.name}`);
        } catch (error) {
            console.log(`❌ ${dep.name} - ${error.message}`);
            allGood = false;
        }
    }

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

    return allGood;
}

testDependencies().then(success => {
    process.exit(success ? 0 : 1);
}).catch(error => {
    console.error('❌ Test failed:', error.message);
    process.exit(1);
});