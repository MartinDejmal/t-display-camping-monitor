const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const code=fs.readFileSync(process.argv[2],'utf8'),header=fs.readFileSync(process.argv[3],'utf8');
function raw(name){const match=header.match(new RegExp('const char '+name+'\\[\\] PROGMEM = R"([A-Z]+)\\(([\\s\\S]*?)\\)\\1";'));assert(match,'Missing '+name);return match[2]}
const pageNames={overview:'PAGE_OVERVIEW',history:'PAGE_HISTORY',device:'PAGE_DEVICE',settings:'PAGE_SETTINGS',time:'PAGE_TIME',firmware:'PAGE_FIRMWARE'};
function makePage(page){
 const html=raw(pageNames[page])+raw('WEB_TAIL'),ids=[...html.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]);
 assert.equal(new Set(ids).size,ids.length,'Duplicate HTML id');
 const elements=new Map(),calls=[],canvasText=[];
 class Element{
  constructor(id=''){this.id=id;this.value=id==='metric'?'t':id==='hours'?'8':'';this.checked=false;this.textContent='';this.files=[];this.children=[];this.clientWidth=600;this.disabled=false;this.button={disabled:false}}
  appendChild(e){this.children.push(e);if(!this.value)this.value=e.value;return e}
  replaceChildren(){this.children=[];this.value=''}
  querySelector(){return this.button}
  click(){this.clicked=true}
  getBoundingClientRect(){return {left:0}}
  getContext(){return new Proxy({}, {get(target,key){
   if(key==='fillText')return text=>canvasText.push(text);
   if(['moveTo','lineTo','scale'].includes(key))return (...args)=>assert(args.every(Number.isFinite),'Non-finite chart coordinate');
   return ()=>{};
  }})}
 }
 for(const id of [...ids,'message'])elements.set(id,new Element(id));
 const config={interval_s:60,screen_timeout_s:15,brightness:120,ssid:'test-network',auth_enabled:true,auto_time:true};
 const record={session:26,elapsed_s:60,epoch:0};
 const fixture={csrf:'test-token',sensor_ok:true,has_humidity:false,sensor_type:'BMP085/180',sensor_address:119,sensor_chip_id:85,sensor_error:'',temperature:23.9,humidity:null,pressure_hpa:985.74,power_mode:'bench',battery_v:null,external:true,network:'<script>unsafe</script>',lan_ip:'192.168.0.120',ap_ip:'',used_bytes:5000,total_bytes:1245184,record_count:30,valid_record_count:28,session_count:2,first_record:{session:25,elapsed_s:0,epoch:0},last_write:record,last_measurement:record,last_write_age_s:60,interval_s:60,next_sample_in_s:30,time_known:false,epoch:0,session:26,elapsed_s:120,version:'test',variant:'bench-service',config};
 const sandbox={console,document:{body:{dataset:{page}},getElementById:id=>elements.get(id)||null,createElement:()=>new Element()},window:{devicePixelRatio:1,addEventListener(){}},
  fetch:async(path,options)=>{
   calls.push({path,options});let data=fixture;
   if(path==='/api/config'){Object.assign(config,JSON.parse(options.body));data={ok:true}}
   else if(path==='/api/time'){const p=JSON.parse(options.body);if(p.clear){fixture.epoch=0;fixture.time_known=false;config.auto_time=false}else{fixture.epoch=p.epoch;fixture.time_known=true}data={ok:true}}
   else if(path==='/api/sessions')data={current:26,total:2,sessions:[{id:26,count:20,first_elapsed_s:0,last_elapsed_s:120},{id:25,count:10,first_elapsed_s:0,last_elapsed_s:3600}]};
   else if(path.startsWith('/api/history')){const s=Number(new URL(path,'http://device').searchParams.get('session'));data={time_basis:'session_elapsed',session:s,reference_elapsed_s:s===26?120:3600,bins:[null,[21,22,s===25?0:null,s===25?0:null,s,985.6,985.8],null]}}
   return {ok:true,status:200,json:async()=>data};
  },setInterval(){},setTimeout(){},Blob,URL,location:{href:''},FormData:class{},XMLHttpRequest:class{}};
 vm.createContext(sandbox);vm.runInContext(code,sandbox);
 return {elements,calls,canvasText,fixture,sandbox,get:id=>elements.get(id)};
}
async function settle(){for(let i=0;i<100;i++)await Promise.resolve()}
async function run(){
 for(const page of Object.keys(pageNames)){
  const p=makePage(page);await settle();assert.equal(p.get('message').textContent,'','Initial page failed: '+page);
  assert(p.calls.some(c=>c.path==='/api/status'));
  if(page==='overview'){
   assert.equal(p.get('temp').textContent,'23.9 °C');assert.equal(p.get('rh').textContent,'Neměří');assert.equal(p.get('pressure').textContent,'985.74 hPa');
   assert(p.get('clockBrief').textContent.includes('není nastavený'));
  }
  if(page==='history'){
   assert(!p.get('ota')&&!p.get('measurementForm'),'Other tabs must not be embedded in history');
   assert(p.calls.some(c=>c.path==='/api/history?hours=8&session=26'));
   p.get('metric').value='p';await p.get('metric').onchange();assert(p.get('chartRange').textContent.includes('Tlak · 985.60 až 985.80 hPa'));
   p.get('metric').value='h';await p.get('metric').onchange();assert(p.canvasText.includes('Připojené čidlo vlhkost neměří'));
   p.get('session').value='25';await p.get('session').onchange();assert(p.get('chartRange').textContent.includes('0.00 až 0.00'));
   await p.get('download').onclick();assert.equal(p.sandbox.location.href,'/api/export.csv?hours=8&session=25');
   assert(p.get('historyBasis').textContent.includes('poslednímu záznamu'));
  }
  if(page==='device'){
   assert.equal(p.get('recordCount').textContent,30);assert(p.get('deviceNetwork').textContent.includes('<script>unsafe</script>'));
   assert(p.get('deviceClock').textContent.includes('Není nastavený'));assert(p.get('historyFirst').textContent.includes('relace 25'));
  }
  if(page==='settings'){
   assert.equal(p.get('authEnabled').checked,true);p.get('interval').value='120';
   await p.get('measurementForm').onsubmit({preventDefault(){}});
   let c=p.calls.filter(c=>c.path==='/api/config').at(-1);assert.equal(JSON.parse(c.options.body).interval_s,120);assert.equal(c.options.headers['X-CSRF-Token'],'test-token');
   p.get('authEnabled').checked=false;await p.get('accessForm').onsubmit({preventDefault(){}});
   c=p.calls.filter(c=>c.path==='/api/config').at(-1);assert.deepEqual(JSON.parse(c.options.body),{auth_enabled:false});assert.equal(p.fixture.config.auth_enabled,false);
   assert(p.get('authState').textContent.includes('vypnuté'));
   p.get('authEnabled').checked=true;await p.get('accessForm').onsubmit({preventDefault(){}});assert.equal(p.sandbox.location.href,'/settings');
   p.get('restore').files=[{size:100,text:async()=>JSON.stringify({version:1,brightness:44,auth_enabled:false,admin_password:'must-not-import'})}];
   await p.get('restore').onchange();assert(!p.get('importPreview').textContent.includes('must-not-import'));await p.get('importApply').onclick();assert.equal(p.fixture.config.brightness,44);
  }
  if(page==='time'){
   assert(p.get('timeUTC').textContent.includes('Není nastavený'));
   await p.get('sync').onclick();assert(p.fixture.time_known);assert.equal(p.fixture.next_sample_in_s,30);
   await p.get('relativeOnly').onclick();assert(!p.fixture.time_known&&!p.fixture.config.auto_time);assert.equal(p.fixture.epoch,0);assert.equal(p.fixture.next_sample_in_s,30);
  }
  if(page==='firmware'){assert(p.get('version').textContent.includes('Firmware'));assert(p.get('ota'));assert(!p.get('measurementForm'))}
 }
 console.log('Web: six separate pages, pressure/RH charts, old sessions, unknown time, device stats, auth switch, partial settings and safe import OK');
}
run().catch(e=>{console.error(e);process.exitCode=1});
