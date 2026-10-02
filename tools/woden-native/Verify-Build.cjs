// Read file bytes only: never loads a native module or opens a device.
const fs = require('node:fs'), cp = require('node:child_process'), crypto = require('node:crypto');
const hash = b => crypto.createHash('sha256').update(b).digest('hex');
const git = args => cp.execFileSync('git', args, {encoding:'utf8'}).trim();
if (git(['status','--porcelain'])) throw Error('Source checkout must be clean');
const expected = fs.readFileSync('native/wheelffb/WheelFfb.def','utf8').split(/\r?\n/)
    .filter(x => /^\s+[A-Za-z_][A-Za-z0-9_]*\s*$/.test(x)).map(x=>x.trim()).sort();
if (expected.length !== 39 || !expected.includes('GetInputCapabilities')) throw Error('Wrong HAT ABI');
function artifact(path, machine) {
    const b=fs.readFileSync(path), pe=b.readUInt32LE(0x3c), opt=pe+24;
    if (b.readUInt16LE(pe+4)!==machine) throw Error('Wrong architecture');
    const sections=opt+b.readUInt16LE(pe+20);
    function offset(rva) {
        for(let i=0;i<b.readUInt16LE(pe+6);i++) {
            const s=sections+i*40,va=b.readUInt32LE(s+12);
            if(rva>=va && rva<va+Math.max(b.readUInt32LE(s+8),b.readUInt32LE(s+16))) return b.readUInt32LE(s+20)+rva-va;
        }
        throw Error('Invalid PE RVA');
    }
    const e=offset(b.readUInt32LE(opt+(b.readUInt16LE(opt)===0x20b?112:96)));
    const names=offset(b.readUInt32LE(e+32)), exports=[];
    for(let i=0;i<b.readUInt32LE(e+24);i++) { const a=offset(b.readUInt32LE(names+i*4)); exports.push(b.toString('ascii',a,b.indexOf(0,a))); }
    exports.sort();
    if(JSON.stringify(exports)!==JSON.stringify(expected) || b.readUInt32LE(e+20)!==39) throw Error('Export set differs');
    return {path,sha256:hash(b),machine:machine.toString(16),exports};
}
const paths=['native/wheelffb/WheelFfb.cpp','native/wheelffb/WheelFfb.def','native/wheelffb/include/wheelffb.h',
    'native/wheelffb/include/digital_input.h','native/wheelffb/build.bat','native/wheelffb/test/shutdown_test.cpp',
    'native/wheelffb/test/digital_input_test.cpp','native/wheelffb/test/test_shutdown.bat','native/wheelffb/test/test_digital_input.bat'];
const sources=paths.map(path=>{
    const blob=git(['rev-parse','HEAD:'+path]);
    if(git(['hash-object',path])!==blob) throw Error('Source changed: '+path);
    return {path,gitBlob:blob,checkoutSha256:hash(fs.readFileSync(path))};
});
const result={schema:'dbce.woden-native-backport-build@1',sourceCommit:git(['rev-parse','HEAD']),sourceTree:git(['rev-parse','HEAD^{tree}']),
    dirty:false,baseCommit:'9ad1f64044b57fa05c119697518a47f2a86ba0de',reviewedFix:'f8f0619b5588f2d11b44f4becd4198775d4a8bcf',
    nativeVersion:502,toolkitBase:'v0.12.0 + isolated shutdown/input candidate',inputCapabilities:1,
    buildCommands:['native/wheelffb/build.bat x64','native/wheelffb/build.bat x86'],
    compileFlags:'/nologo /LD /O2 /W4 /EHsc /DNDEBUG; link /DEF /MACHINE',sources,
    artifacts:[artifact('native/wheelffb/build/WheelFfb.dll',0x8664),artifact('native/wheelffb/build/x86/WheelFfb.dll',0x14c)],
    compilerToolchain:process.env.WODEN_BUILD_TOOLCHAIN || null,
    hardwareOpened:false,installed:false,published:false,reviewRequired:true,
    claim:'Fresh source/hash/export provenance; not a reconstruction of historical A5ED binary, physical acceptance or reproducible-build guarantee'};
if(!result.compilerToolchain) throw Error('Set WODEN_BUILD_TOOLCHAIN to recorded compiler and SDK versions');
fs.writeFileSync(process.argv[2],JSON.stringify(result,null,2)+'\n');
console.log('PASS clean source provenance and exact 39-export x64/x86 HAT ABI');
