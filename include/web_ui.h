#pragma once
#include <Arduino.h>

const char WEB_HEAD[] PROGMEM = R"HTML(<!doctype html><html lang="cs"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>T-Display · monitor prostředí</title><link rel="stylesheet" href="/assets/app.css"></head>)HTML";
const char WEB_TAIL[] PROGMEM = R"HTML(</main><footer><span id="connection">Připojování…</span><span id="authState"></span></footer><script src="/assets/app.js" defer></script></body></html>)HTML";

const char WEB_CSS[] PROGMEM = R"CSS(
:root{font-family:system-ui,-apple-system,sans-serif;color:#dce8ed;background:#10191f;color-scheme:dark;font-synthesis:none}
*{box-sizing:border-box}body{max-width:1080px;margin:auto;padding:24px}header{display:flex;align-items:baseline;gap:16px;padding:8px 0 24px}header span{color:#9fb4c1}.brand{font-size:24px;font-weight:750;color:#ecf7fa;text-decoration:none}
nav{display:flex;gap:4px;overflow:auto;border-bottom:1px solid #344b58;margin-bottom:28px}nav a{flex-shrink:0;padding:13px 17px;color:#afc5d1;text-decoration:none;border-bottom:3px solid transparent}nav a[aria-current=page]{color:#97e7d4;border-color:#78d8bd;background:#172b2b}
h1{font-size:28px;margin:0 0 8px}h2{font-size:18px;margin:0 0 16px}p{line-height:1.55;color:#afc5d1}.lede{max-width:760px;margin:8px 0 24px}.card{background:#192730;border-radius:12px;padding:22px;margin:20px 0}.metrics{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px}.metric{background:#192730;border-radius:12px;padding:22px}.metric span{color:#afc5d1}.value{font-size:clamp(23px,4vw,38px);font-weight:650;margin-top:10px;white-space:nowrap;font-variant-numeric:tabular-nums}.row{display:flex;flex-wrap:wrap;gap:16px;align-items:end}.row>*{flex:1;min-width:140px}.row>button,.row>.btn{flex:0 0 auto;min-width:0}
dl{margin:0}.details{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}.details div{min-width:0}dt{color:#afc5d1;font-size:13px;margin-bottom:5px}dd{margin:0;line-height:1.5;overflow-wrap:anywhere;font-variant-numeric:tabular-nums}label{display:block;line-height:1.5;margin:12px 0;color:#cbdde6}input,select,button,.btn{font:inherit;border:1px solid #4c6979;border-radius:6px;padding:10px 12px;color:inherit;background:#101c24}input:not([type=file]):not([type=checkbox]),select{display:block;width:100%;margin-top:6px}input[type=checkbox]{margin-right:10px;accent-color:#78d8bd}button,.btn{cursor:pointer;text-decoration:none;display:inline-block;background:#285c68;white-space:normal}button:hover,.btn:hover{background:#337184}button:disabled{opacity:.5;cursor:wait}a{color:#8ad9ec}.actions{display:flex;gap:12px;flex-wrap:wrap;margin-top:18px}.muted,small{color:#afc5d1}.note{border-left:3px solid #78d8bd;padding-left:14px}.note.warn{border-color:#e4b676}#message{white-space:pre-wrap;line-height:1.5;color:#f4ce91;margin:10px 0;min-height:1px}#message:empty{margin:0}canvas{display:block;width:100%;height:310px;margin-top:14px}#chartPoint{font-variant-numeric:tabular-nums;min-height:24px}progress{display:block;width:100%;height:16px;margin:14px 0;accent-color:#78d8bd}pre{white-space:pre-wrap;overflow-wrap:anywhere;padding:14px;background:#101c24;border-radius:6px}pre:empty{display:none}footer{display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap;margin-top:32px;padding:18px 0;border-top:1px solid #344b58;font-size:12px;color:#9fb4c1}code{color:#97e7d4}
@media(max-width:650px){body{padding:16px}header{padding-bottom:18px}header span{font-size:13px}nav a{padding:12px}.metrics{grid-template-columns:1fr;gap:10px}.metric{display:flex;justify-content:space-between;align-items:center;padding:18px}.value{margin:0;font-size:27px}.details{grid-template-columns:1fr}.card{padding:18px}.row>*{min-width:100%}.row>button{width:100%}}
)CSS";

const char PAGE_OVERVIEW[] PROGMEM = R"HTML(
<p class="lede" id="clockBrief">Načítání měření…</p>
<div class="metrics"><div class="metric"><span>Teplota</span><div class="value" id="temp">—</div></div><div class="metric"><span>Vlhkost</span><div class="value" id="rh">—</div></div><div class="metric"><span>Tlak</span><div class="value" id="pressure">—</div></div></div>
<section class="card"><h2>Průběh měření</h2><dl class="details"><div><dt>Čidlo</dt><dd id="overviewSensor">—</dd></div><div><dt>Provozní režim</dt><dd id="overviewPower">—</dd></div><div><dt>Další plánované měření</dt><dd id="overviewNext">—</dd></div><div><dt>Poslední úspěšný zápis</dt><dd id="overviewLast">—</dd></div><div><dt>Uložené záznamy</dt><dd id="overviewCount">—</dd></div><div><dt>Stav měření a úložiště</dt><dd id="overviewHealth">—</dd></div></dl><div class="actions"><a class="btn" href="/history">Otevřít historii</a><a class="btn" href="/device">Podrobnosti zařízení</a></div></section>
)HTML";

const char PAGE_HISTORY[] PROGMEM = R"HTML(
<p class="lede">Graf pracuje s uplynulým časem v jedné měřicí relaci. Správné datum ani připojení k internetu nejsou potřeba.</p>
<section class="card"><div class="row"><label>Veličina<select id="metric"><option value="t">Teplota · °C</option><option value="p">Tlak · hPa</option><option value="h">Vlhkost · % RH</option></select></label><label>Rozsah<select id="hours"><option value="8">8 hodin</option><option value="24">24 hodin</option><option value="168">7 dní</option><option value="720">30 dní</option></select></label><button type="button" id="refresh">Obnovit</button></div><label>Měřicí relace<select id="session"></select></label><p id="historyBasis" class="note"></p><p id="sessionsNotice" class="muted"></p><h2 id="chartRange">Historie</h2><canvas id="chart" role="img" aria-label="Graf vybrané veličiny v uplynulém čase"></canvas><p id="chartPoint">Najetím na graf zobrazíš minimum a maximum vzorků v daném úseku.</p><p><small>Svislé úsečky zachovávají výkyvy. Mezery znamenají chybějící nebo neplatná měření. Vlhkost se u BMP nevymýšlí.</small></p><div class="actions"><button type="button" id="download">CSV zvolené relace a rozsahu</button><a class="btn" href="/api/export.csv">CSV všech relací</a></div><p><small>Úplné CSV obsahuje i relace bez známého data a záznamy s chybou čidla.</small></p></section>
)HTML";

const char PAGE_DEVICE[] PROGMEM = R"HTML(
<p class="lede">Provozní stav, úložiště a časové údaje. Neznámé datum se nenahrazuje odhadovaným časem.</p>
<section class="card"><h2>Zařízení</h2><dl class="details"><div><dt>Firmware a varianta</dt><dd id="deviceVersion">—</dd></div><div><dt>Rozpoznané čidlo</dt><dd id="deviceSensor">—</dd></div><div><dt>Napájení</dt><dd id="devicePower">—</dd></div><div><dt>Napětí baterie</dt><dd id="deviceBattery">—</dd></div><div><dt>Síť a LAN adresa</dt><dd id="deviceNetwork">—</dd></div><div><dt>Záložní přístupový bod</dt><dd id="deviceAP">—</dd></div><div><dt>Aktuální relace a její délka</dt><dd id="deviceSession">—</dd></div><div><dt>Interval a další měření</dt><dd id="deviceNext">—</dd></div><div><dt>Poslední úspěšný zápis</dt><dd id="deviceLast">—</dd></div><div><dt>Poslední platné měření</dt><dd id="deviceGood">—</dd></div></dl></section>
<section class="card"><h2>Historie a úložiště</h2><progress id="storageMeter" value="0" max="100"></progress><dl class="details"><div><dt>Obsazené místo</dt><dd id="storageBytes">—</dd></div><div><dt>Počet záznamů</dt><dd id="recordCount">—</dd></div><div><dt>Platná měření / chyby čidla</dt><dd id="validCount">—</dd></div><div><dt>Počet měřicích relací</dt><dd id="sessionCount">—</dd></div><div><dt>První uložený záznam</dt><dd id="historyFirst">—</dd></div><div><dt>Poslední uložený záznam</dt><dd id="historyLast">—</dd></div><div><dt>Stav úložiště</dt><dd id="storageHealth">—</dd></div><div><dt>Kalendářní čas</dt><dd id="deviceClock">—</dd></div></dl><div class="actions"><button id="deviceRefresh" type="button">Obnovit stav</button><a class="btn" href="/history">Prohlédnout relace</a></div></section>
<section class="card"><h2>Diagnostika běhu</h2><dl class="details"><div><dt>Doba od startu</dt><dd id="runtimeUptime">—</dd></div><div><dt>Volná RAM / minimum od startu</dt><dd id="runtimeHeap">—</dd></div><div><dt>Největší volný blok RAM</dt><dd id="runtimeBlock">—</dd></div><div><dt>Nejmenší rezerva zásobníku</dt><dd id="runtimeStack">—</dd></div><div><dt>Důvod posledního resetu (ESP-IDF)</dt><dd id="runtimeReset">—</dd></div></dl></section>
)HTML";

const char PAGE_SETTINGS[] PROGMEM = R"HTML(
<p class="lede">Nastavení se uchová po restartu. Změny jednotlivých částí se ukládají samostatně.</p>
<section class="card"><h2>Měření a displej</h2><form id="measurementForm"><div class="row"><label>Interval měření (60–3600 s)<input id="interval" type="number" min="60" max="3600" required></label><label>Zhasnutí displeje (5–120 s)<input id="timeout" type="number" min="5" max="120" required></label><label>Jas (1–255)<input id="brightness" type="number" min="1" max="255" required></label></div><p id="retention"></p><button>Uložit měření a displej</button></form></section>
<section class="card"><h2>Wi-Fi</h2><form id="networkForm"><div class="row"><label>Název sítě (SSID)<input id="ssid" maxlength="32" autocomplete="off"></label><label>Heslo Wi-Fi (prázdné = ponechat)<input id="wifiPassword" type="password" maxlength="63" autocomplete="new-password"></label></div><label><input id="clearWifi" type="checkbox">Zapomenout Wi-Fi a použít záložní AP</label><p>Změna sítě se projeví po restartu.</p><button>Uložit síť</button></form></section>
<section class="card"><h2>Přístup k webu</h2><form id="accessForm"><label><input id="authEnabled" type="checkbox">Vyžadovat přihlášení k webovému rozhraní</label><p class="note warn">Při vypnutí může zařízení spravovat každý, kdo má přístup do jeho sítě. Heslo záložního Wi-Fi přístupového bodu zůstane zachované.</p><label>Nové heslo webu a záložního AP (12–63 znaků; prázdné = ponechat)<input id="adminPassword" type="password" minlength="12" maxlength="63" autocomplete="new-password"></label><p>Webové heslo se mění ihned, heslo AP po restartu. Uživatelské jméno je admin.</p><button>Uložit přístup</button></form></section>
<section class="card"><h2>Záloha konfigurace</h2><p>Export obsahuje uložená nastavení včetně přepínače přihlášení, ale žádná hesla.</p><div class="actions"><button id="backup" type="button">Export konfigurace</button></div><label>Načíst konfiguraci ze souboru<input id="restore" type="file" accept="application/json,.json"></label><pre id="importPreview"></pre><button id="importApply" type="button" disabled>Použít zobrazenou konfiguraci</button></section>
<section class="card"><h2>Restart</h2><p>Uložená historie a konfigurace zůstanou zachované. Začne nová měřicí relace.</p><button id="restart" type="button">Restartovat zařízení</button></section>
)HTML";

const char PAGE_TIME[] PROGMEM = R"HTML(
<p class="lede">Správné datum není podmínkou měření. Intervaly, pořadí vzorků a grafy používají čas uplynulý v měřicí relaci.</p>
<section class="card"><h2>Čas zařízení</h2><dl class="details"><div><dt>Kalendářní čas UTC</dt><dd id="timeUTC">—</dd></div><div><dt>Relativní čas relace</dt><dd id="timeRelative">—</dd></div></dl><p class="note">Deep sleep relaci zachová. Reset nebo úplný výpadek napájení založí novou relaci. Bez známého data nelze určit délku mezery mezi relacemi; v historii proto zůstávají oddělené.</p></section>
<section class="card"><h2>Volitelná synchronizace</h2><form id="ntpForm"><label><input id="autoTime" type="checkbox">Získávat čas z internetu při Wi-Fi připojení</label><button>Uložit synchronizaci</button></form><div class="actions"><button id="sync" type="button">Nastavit čas podle prohlížeče</button><button id="relativeOnly" type="button">Vypnout NTP a používat pouze relativní čas</button></div><p><small>Změna nebo vymazání data neposune plán měření a nepřepíše starší záznamy. V CSV zůstane UTC neznámých záznamů prázdné. Přesnost časových rozestupů závisí na hodinách ESP32.</small></p></section>
)HTML";

const char PAGE_FIRMWARE[] PROGMEM = R"HTML(
<p class="lede">Aktualizace aplikace přes Wi-Fi při externím napájení.</p>
<section class="card"><h2>Aktuální firmware</h2><p id="version">Načítání…</p><p id="firmwareVariant"></p><p>Vyber <code>firmware.bin</code> pro původní T-Display se stejnou variantou zapojení. Aktualizace zachová historii a konfiguraci. Během přenosu může vzniknout mezera v měření.</p><form id="ota"><label>Soubor firmwaru<input id="firmware" type="file" accept=".bin" required></label><button>Nahrát firmware a restartovat</button></form><progress id="progress" value="0" max="100"></progress><p id="otaStatus" role="status"></p><p><small>Používají se dva oddíly aplikace a aplikační hlídání prvního startu. Změny bootloaderu nebo rozdělení flash vyžadují USB.</small></p></section>
)HTML";

const char WEB_JS[] PROGMEM = R"JS(
'use strict';
const PAGE=document.body.dataset.page;
const $=id=>document.getElementById(id);
const text=(id,v)=>{const e=$(id);if(e)e.textContent=v};
const setValue=(id,v)=>{const e=$(id);if(e)e.value=v};
const setChecked=(id,v)=>{const e=$(id);if(e)e.checked=Boolean(v)};
const say=s=>text('message',s);
let token='',state=null,busy=false,historyData=[],historyMeta=null,chartGeometry=null,importPayload=null;
async function api(path,data){
 const options={cache:'no-store',credentials:'same-origin'};
 if(data!==undefined){options.method='POST';options.headers={'Content-Type':'application/json','X-CSRF-Token':token};options.body=JSON.stringify(data)}
 const r=await fetch(path,options);let body;
 try{body=await r.json()}catch{throw Error(r.status===401?'Je vyžadováno přihlášení. Otevři stránku znovu.':'Neočekávaná odpověď zařízení.')}
 if(!r.ok)throw Error(body.error||('HTTP '+r.status));return body;
}
function duration(s){s=Math.max(0,Math.floor(Number(s)||0));const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return (d?d+' d ':'')+String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s%60).padStart(2,'0')}
function place(r){return r?'relace '+r.session+', +'+duration(r.elapsed_s)+(r.epoch?' · '+new Date(r.epoch*1000).toISOString():' · bez data'):'Zatím žádný záznam'}
function powerLabel(s){return s.power_mode==='bench'?'Servisní test · Wi-Fi trvale zapnutá':s.power_mode==='usb'?'USB · servis':'Baterie · úsporné měření'}
function retention(){if(!$('interval'))return;const n=Number($('interval').value)||60;const bytes=30*86400/n*24;text('retention','30 dní při tomto intervalu: přibližně '+Math.ceil(bytes/1024)+' KiB dat plus režie úložiště.')}
function fillSettings(c){setValue('interval',c.interval_s);setValue('timeout',c.screen_timeout_s);setValue('brightness',c.brightness);setValue('ssid',c.ssid||'');setChecked('authEnabled',c.auth_enabled);setChecked('autoTime',c.auto_time);retention()}
function renderStatus(first){
 const s=state,utc=s.time_known?new Date(s.epoch*1000).toISOString():'Není nastavený · měření používá relativní čas';
 text('temp',s.sensor_ok?s.temperature.toFixed(1)+' °C':'—');
 text('rh',s.sensor_ok&&s.has_humidity&&Number.isFinite(s.humidity)?s.humidity.toFixed(1)+' %':s.sensor_ok&&!s.has_humidity?'Neměří':'—');
 text('pressure',s.sensor_ok?s.pressure_hpa.toFixed(2)+' hPa':'—');
 text('clockBrief',s.time_known?'Kalendářní čas UTC: '+utc:'Kalendářní čas není nastavený. Měření a časové rozestupy fungují v relaci '+s.session+'.');
 const next='za '+s.next_sample_in_s+' s · interval '+s.interval_s+' s',last=s.last_write_age_s!==null&&s.last_write_age_s!==undefined?'před '+s.last_write_age_s+' s · '+place(s.last_write):place(s.last_write);
 text('overviewSensor',s.sensor_type+(s.sensor_error?' · '+s.sensor_error:''));text('overviewPower',powerLabel(s));text('overviewNext',next);text('overviewLast',last);text('overviewCount',s.record_count+' záznamů / '+s.session_count+' relací');text('overviewHealth',s.storage_error||s.sensor_error||'Měření a ukládání v pořádku');
 text('deviceVersion',s.version+' · '+s.variant);text('deviceSensor',s.sensor_type+' · adresa 0x'+Number(s.sensor_address).toString(16)+' · ID 0x'+Number(s.sensor_chip_id).toString(16));text('devicePower',powerLabel(s));text('deviceBattery',Number.isFinite(s.battery_v)?s.battery_v.toFixed(2)+' V':'Na USB není samostatně měřitelné');text('deviceNetwork',s.network+(s.lan_ip?' · '+s.lan_ip:''));text('deviceAP',s.ap_ip||'Neaktivní');text('deviceSession',s.session+' · +'+duration(s.elapsed_s));text('deviceNext',next);text('deviceLast',last);text('deviceGood',place(s.last_measurement));
 text('storageBytes',Math.round(s.used_bytes/1024)+' / '+Math.round(s.total_bytes/1024)+' KiB');text('recordCount',s.record_count);text('validCount',s.valid_record_count+' platných / '+Math.max(0,s.record_count-s.valid_record_count)+' s chybou');text('sessionCount',s.session_count);text('historyFirst',place(s.first_record));text('historyLast',place(s.last_write));text('storageHealth',s.storage_error||'V pořádku');text('deviceClock',utc);
 const kib=n=>Number.isFinite(n)?(n/1024).toFixed(1)+' KiB':'—';
 text('runtimeUptime',duration(s.uptime_s));text('runtimeHeap',kib(s.free_heap_bytes)+' / '+kib(s.min_free_heap_bytes));text('runtimeBlock',kib(s.largest_free_block_bytes));text('runtimeStack',kib(s.loop_stack_free_min_bytes));text('runtimeReset',s.reset_reason===undefined?'—':s.reset_reason);
 if($('storageMeter'))$('storageMeter').value=s.total_bytes?100*s.used_bytes/s.total_bytes:0;
 text('timeUTC',utc);text('timeRelative','Relace '+s.session+' · +'+duration(s.elapsed_s));text('version','Firmware '+s.version);text('firmwareVariant','Varianta: '+s.variant+' · '+s.sensor_type);text('connection',s.network+' · FW '+s.version);text('authState',s.config.auth_enabled?'Web: přihlášení zapnuté':'Web: přihlášení vypnuté');
 if(first)fillSettings(s.config);
}
async function loadStatus(first=false){state=await api('/api/status');token=state.csrf;renderStatus(first);return state}
async function loadSessions(){
 const result=await api('/api/sessions'),select=$('session');select.replaceChildren();
 const add=(id,label)=>{const o=document.createElement('option');o.value=String(id);o.textContent=label;select.appendChild(o)};
 add(result.current,'Aktuální relace '+result.current);
 for(const s of result.sessions)if(s.id!==result.current)add(s.id,'Relace '+s.id+' · '+s.count+' záznamů · +'+duration(s.first_elapsed_s)+' až +'+duration(s.last_elapsed_s));
 select.value=String(result.current);text('sessionsNotice',result.total>result.sessions.length?'Výběr obsahuje nejnovějších 64 relací. Úplné CSV obsahuje všechny.':'K dispozici je '+result.total+' měřicích relací.');
}
async function loadHistory(){
 const session=$('session').value||String(state.session);
 historyMeta=await api('/api/history?hours='+$('hours').value+'&session='+encodeURIComponent(session));historyData=historyMeta.bins;
 text('historyBasis','Relace '+historyMeta.session+' · rozsah vzhledem '+(Number(session)===state.session?'k nynějšku':'k poslednímu záznamu relace')+' (+ '+duration(historyMeta.reference_elapsed_s)+'). Délku výpadků mezi relacemi neodhadujeme.');draw();
}
function metric(){return {t:{lo:0,hi:1,label:'Teplota',unit:'°C',color:'#ffbd78',span:1},p:{lo:5,hi:6,label:'Tlak',unit:'hPa',color:'#97e7d4',span:.5},h:{lo:2,hi:3,label:'Vlhkost',unit:'% RH',color:'#82d8eb',span:1}}[$('metric').value]}
function draw(){
 const c=$('chart');if(!c)return;
 const m=metric(),dpr=window.devicePixelRatio||1,w=c.clientWidth,h=310;c.width=w*dpr;c.height=h*dpr;
 const x=c.getContext('2d');x.scale(dpr,dpr);x.clearRect(0,0,w,h);x.font='12px system-ui';
 const left=64,right=w-14,top=24,bottom=h-42,width=right-left,height=bottom-top;
 const available=b=>b&&Number.isFinite(b[m.lo])&&Number.isFinite(b[m.hi]);
 let low=Infinity,high=-Infinity;for(const b of historyData)if(available(b)){low=Math.min(low,b[m.lo]);high=Math.max(high,b[m.hi])}
 const empty=!Number.isFinite(low);text('chartRange',empty?m.label+' · bez dat':m.label+' · '+low.toFixed(2)+' až '+high.toFixed(2)+' '+m.unit);
 if(empty){low=0;high=1}else{const padding=Math.max((high-low)*.06,m.span/2);low-=padding;high+=padding}
 x.strokeStyle='#557080';x.beginPath();x.moveTo(left,top);x.lineTo(left,bottom);x.lineTo(right,bottom);x.stroke();
 if(!empty){x.fillStyle='#afc5d1';for(let i=0;i<3;i++){const y=top+i*height/2,value=high-i*(high-low)/2;x.fillText(value.toFixed(2),2,y+4)}}
 let prev=null;
 historyData.forEach((b,i)=>{if(!available(b)){prev=null;return}const px=left+i*width/Math.max(1,historyData.length-1),y1=bottom-(b[m.hi]-low)/(high-low)*height,y2=bottom-(b[m.lo]-low)/(high-low)*height,py=(y1+y2)/2;x.strokeStyle=m.color;x.beginPath();x.moveTo(px,y1);x.lineTo(px,y2+1);if(prev&&prev.session===b[4]){x.moveTo(prev.px,prev.py);x.lineTo(px,py)}x.stroke();prev={px,py,session:b[4]}});
 x.fillStyle='#afc5d1';x.fillText('-'+$('hours').value+' h',left,bottom+22);x.fillText('-'+Number($('hours').value)/2+' h',left+width/2-20,bottom+22);x.fillText(historyMeta&&historyMeta.session!==state.session?'konec':'nyní',right-32,bottom+22);
 if(empty)x.fillText($('metric').value==='h'&&state&&!state.has_humidity?'Připojené čidlo vlhkost neměří':'V této relaci a rozsahu nejsou data',left+10,top+height/2);
 chartGeometry={left,width};
}
function inspect(event){
 if(!chartGeometry||!historyData.length)return;
 const rect=$('chart').getBoundingClientRect(),index=Math.max(0,Math.min(historyData.length-1,Math.round((event.clientX-rect.left-chartGeometry.left)/chartGeometry.width*(historyData.length-1)))),b=historyData[index],m=metric();
 if(!b||!Number.isFinite(b[m.lo])||!Number.isFinite(b[m.hi])){text('chartPoint','V tomto úseku není dostupné měření.');return}
 const age=Number($('hours').value)*3600*(historyData.length-1-index)/historyData.length;
 text('chartPoint','Asi '+duration(age)+' před koncem zobrazeného rozsahu · '+b[m.lo].toFixed(2)+' až '+b[m.hi].toFixed(2)+' '+m.unit);
}
function bind(id,event,action){const e=$(id);if(e)e[event]=async ev=>{try{await action(ev)}catch(error){say(error.message)}}}
bind('refresh','onclick',loadHistory);bind('hours','onchange',loadHistory);bind('session','onchange',loadHistory);bind('metric','onchange',draw);bind('chart','onmousemove',inspect);window.addEventListener('resize',draw);
bind('download','onclick',()=>{location.href='/api/export.csv?hours='+$('hours').value+'&session='+encodeURIComponent($('session').value)});
bind('deviceRefresh','onclick',()=>loadStatus());
bind('interval','oninput',retention);
bind('measurementForm','onsubmit',async e=>{e.preventDefault();await api('/api/config',{interval_s:Number($('interval').value),screen_timeout_s:Number($('timeout').value),brightness:Number($('brightness').value)});await loadStatus();say('Měření a displej uloženy.');retention()});
bind('networkForm','onsubmit',async e=>{e.preventDefault();const c={ssid:$('ssid').value};if($('wifiPassword').value)c.wifi_password=$('wifiPassword').value;if($('clearWifi').checked)c.clear_wifi=true;await api('/api/config',c);setValue('wifiPassword','');await loadStatus();say('Síť uložena. Změna se projeví po restartu.')});
bind('accessForm','onsubmit',async e=>{e.preventDefault();const c={auth_enabled:$('authEnabled').checked},changedPassword=$('adminPassword').value;if(changedPassword)c.admin_password=changedPassword;await api('/api/config',c);say('Přístup uložen.');if(c.auth_enabled){location.href='/settings'}else{setValue('adminPassword','');await loadStatus()}});
bind('ntpForm','onsubmit',async e=>{e.preventDefault();await api('/api/config',{auto_time:$('autoTime').checked});await loadStatus();say('Nastavení synchronizace uloženo.')});
bind('sync','onclick',async()=>{await api('/api/time',{epoch:Math.floor(Date.now()/1000)});await loadStatus();say('Datum nastaveno podle prohlížeče. Plán měření pokračuje beze změny.')});
bind('relativeOnly','onclick',async()=>{await api('/api/time',{clear:true});await loadStatus(true);say('NTP vypnuto, datum není nastavené. Měření pokračuje podle relativního času.')});
bind('backup','onclick',()=>{const blob=new Blob([JSON.stringify({version:1,...state.config},null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='t-display-config.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000)});
bind('restore','onchange',async()=>{const f=$('restore').files[0];if(!f||f.size>4096)throw Error('Vyber JSON do 4 KiB.');const c=JSON.parse(await f.text());if(!c||c.version!==1)throw Error('Nepodporovaná konfigurace.');importPayload={};for(const k of ['interval_s','screen_timeout_s','brightness','ssid','auth_enabled','auto_time'])if(Object.prototype.hasOwnProperty.call(c,k))importPayload[k]=c[k];text('importPreview',JSON.stringify(importPayload,null,2));$('importApply').disabled=false;say('Zkontroluj zobrazené změny, zejména přihlášení. Pak je můžeš použít.')});
bind('importApply','onclick',async()=>{if(!importPayload)return;await api('/api/config',importPayload);await loadStatus(true);say('Importovaná konfigurace uložena. Změny sítě vyžadují restart.');$('importApply').disabled=true});
bind('restart','onclick',async()=>{await api('/api/restart',{});say('Zařízení se restartuje. Obnov stránku po opětovném připojení.')});
bind('ota','onsubmit',e=>{
 e.preventDefault();if(busy)return;const f=$('firmware').files[0];if(!f)return;busy=true;$('ota').querySelector('button').disabled=true;
 const xhr=new XMLHttpRequest(),data=new FormData();data.append('firmware',f,f.name);xhr.open('POST','/api/update');xhr.setRequestHeader('X-CSRF-Token',token);xhr.timeout=180000;
 xhr.upload.onprogress=e=>{if(e.lengthComputable)$('progress').value=100*e.loaded/e.total};
 xhr.onload=()=>{busy=false;$('ota').querySelector('button').disabled=false;let message;try{const r=JSON.parse(xhr.responseText);message=r.error||'Firmware přijat. Zařízení se restartuje; obnov stránku po připojení.'}catch{message='Neočekávaná odpověď. Ověř stav zařízení.'}text('otaStatus',message)};
 xhr.onerror=xhr.ontimeout=()=>{busy=false;$('ota').querySelector('button').disabled=false;text('otaStatus','Spojení přerušeno. Ověř verzi po opětovném připojení.')};xhr.send(data);
});
loadStatus(true).then(async()=>{if(PAGE==='history'){await loadSessions();await loadHistory()}}).catch(e=>say(e.message));
if(['overview','device','time'].includes(PAGE))setInterval(()=>{if(!busy)loadStatus().catch(e=>say('Spojení: '+e.message))},10000);
if(PAGE==='history')setInterval(()=>{if(!busy)loadStatus().then(loadHistory).catch(e=>say(e.message))},60000);
)JS";
