// Execute the actual embedded UI against a small browser/API double.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const calls = [], elements = new Map();
const canvasText=[];
const context = new Proxy({}, {get(target,key){
    if(key==='measureText')return ()=>({width:10});
    if(key==='fillText')return text=>canvasText.push(text);
    if(['moveTo','lineTo','scale'].includes(key))return (...args)=>assert(args.every(Number.isFinite),'Non-finite chart coordinate');
    return ()=>{};
}});
function element(id){if(!elements.has(id))elements.set(id,{value:id==='hours'?'8':'',textContent:'',checked:false,clientWidth:600,files:[],getContext:()=>context,querySelector:()=>({disabled:false}),click(){}});return elements.get(id)}
const fixture={csrf:'test-token',sensor_ok:true,has_humidity:true,sensor_type:'BME280',temperature:21.5,humidity:55.2,pressure_hpa:1002.5,external:true,network:'<script>unsafe</script>',used_bytes:5000,total_bytes:1245184,epoch:0,session:9,elapsed_s:42,version:'test',variant:'bench-service',config:{interval_s:60,screen_timeout_s:15,brightness:120,ssid:'test-network'}};
const historyFixture={bins:[null,[21,22,50,55,9],null]};
const sandbox={console,document:{getElementById:element,createElement:()=>({click(){}})},window:{devicePixelRatio:1,addEventListener(){}},fetch:async(path,options)=>{calls.push({path,options});return {ok:true,json:async()=>path.startsWith('/api/history')?historyFixture:fixture}},setInterval(){},setTimeout(){},Blob,URL,confirm:()=>false,location:{href:''},FormData:class{},XMLHttpRequest:class{}};
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(process.argv[2],'utf8'),sandbox);
(async()=>{
    // Let the initial status -> history request chain settle.
    for(let i=0;i<20;i++)await Promise.resolve();
    assert.equal(element('temp').textContent,'21.5 °C');
    assert.equal(element('rh').textContent,'55.2 %');
    assert.equal(element('interval').value,60);
    assert(element('status').textContent.includes('<script>unsafe</script>'));
    assert(calls.some(c=>c.path==='/api/history?hours=8'));
    await element('sync').onclick();
    for(let i=0;i<20;i++)await Promise.resolve();
    const sync=calls.find(c=>c.path==='/api/time');
    assert.equal(sync.options.headers['X-CSRF-Token'],'test-token');
    assert(JSON.parse(sync.options.body).epoch>1704067200);
    element('interval').value='120';
    await element('config').onsubmit({preventDefault(){}});
    const config=calls.find(c=>c.path==='/api/config');
    assert.equal(JSON.parse(config.options.body).interval_s,120);
    assert(!('wifi_password' in JSON.parse(config.options.body)));
    element('download').onclick();
    assert.equal(sandbox.location.href,'/api/export.csv?hours=8');
    fixture.sensor_type='BMP085/180';fixture.has_humidity=false;fixture.humidity=null;
    historyFixture.bins=[null,[21,22,null,null,9],null];
    await vm.runInContext('status()',sandbox);
    await vm.runInContext('history()',sandbox);
    assert.equal(element('temp').textContent,'21.5 °C');
    assert.equal(element('rh').textContent,'Neměří');
    assert.equal(element('pressure').textContent,'1002.5 hPa');
    assert(canvasText.includes('Připojené čidlo vlhkost neměří'));
    // A measured zero humidity from BME is distinct from unavailable BMP data.
    fixture.sensor_type='BME280';fixture.has_humidity=true;fixture.humidity=0;
    historyFixture.bins=[[20,21,0,0,9],[21,22,null,null,9],[22,23,50,51,9]];
    await vm.runInContext('status()',sandbox);await vm.runInContext('history()',sandbox);
    assert.equal(element('rh').textContent,'0.0 %');
    console.log('Web: initial render, history gaps, CSRF, config, CSV actions OK');
})().catch(e=>{console.error(e);process.exitCode=1});
