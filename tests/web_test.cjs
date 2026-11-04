// Execute the actual embedded UI against a small browser/API double.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const calls = [], elements = new Map();
const context = new Proxy({}, {get(target,key){return key === 'measureText' ? ()=>({width:10}) : ()=>{}}});
function element(id){if(!elements.has(id))elements.set(id,{value:id==='hours'?'8':'',textContent:'',checked:false,clientWidth:600,files:[],getContext:()=>context,querySelector:()=>({disabled:false}),click(){}});return elements.get(id)}
const fixture={csrf:'test-token',sensor_ok:true,temperature:21.5,humidity:55.2,pressure_hpa:1002.5,external:true,network:'<script>unsafe</script>',used_bytes:5000,total_bytes:1245184,epoch:0,session:9,elapsed_s:42,version:'test',variant:'bench-service',config:{interval_s:60,screen_timeout_s:15,brightness:120,ssid:'test-network'}};
const sandbox={console,document:{getElementById:element,createElement:()=>({click(){}})},window:{devicePixelRatio:1,addEventListener(){}},fetch:async(path,options)=>{calls.push({path,options});return {ok:true,json:async()=>path.startsWith('/api/history')?{bins:[null,[21,22,50,55,9],null]}:fixture}},setInterval(){},setTimeout(){},Blob,URL,confirm:()=>false,location:{href:''},FormData:class{},XMLHttpRequest:class{}};
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(process.argv[2],'utf8'),sandbox);
(async()=>{
    // Let the initial status -> history request chain settle.
    for(let i=0;i<20;i++)await Promise.resolve();
    assert.equal(element('temp').textContent,'21.5 °C');
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
    console.log('Web: initial render, history gaps, CSRF, config, CSV actions OK');
})().catch(e=>{console.error(e);process.exitCode=1});
