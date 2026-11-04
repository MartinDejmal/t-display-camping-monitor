#pragma once
#include <Arduino.h>
const char WEB_UI[] PROGMEM = R"HTML(
<!doctype html><html lang="cs"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>T-Display · měření prostředí</title>
<style>
:root{font-family:system-ui,sans-serif;color:#dae6ec;background:#10191f;color-scheme:dark}
body{max-width:980px;margin:auto;padding:24px}h1{font-size:26px;margin-bottom:4px}
p{line-height:1.5;color:#afc3cf}section{background:#192730;padding:20px;margin:20px 0;border-radius:12px}
.row{display:flex;flex-wrap:wrap;gap:16px;align-items:center}.metric{min-width:160px;flex:1}
.value{font-size:32px;font-weight:650}label{display:block;margin:12px 0;flex:1;min-width:180px}
input,select,button{font:inherit;padding:10px;border-radius:6px;border:1px solid #557080;background:#10191f;color:inherit}
input:not([type=file]):not([type=checkbox]),select{display:block;box-sizing:border-box;width:100%;margin-top:5px}
button{cursor:pointer;background:#285c68}button:disabled{opacity:.5;cursor:wait}
canvas{width:100%;height:270px}a{color:#82d8eb}#message{white-space:pre-wrap;color:#ffcf83;min-height:24px}
small{color:#afc3cf}progress{width:100%;height:20px}h2{font-size:19px}code{color:#95e3d1}
</style>
<h1>T-Display</h1><p>Teplota, tlak a dostupná vlhkost v čase · servis zařízení</p>
<div id="message" role="status" aria-live="polite"></div>
<section><div class="row">
<div class="metric">Teplota<div class="value" id="temp">—</div></div>
<div class="metric">Vlhkost<div class="value" id="rh">—</div></div>
<div class="metric">Tlak<div class="value" id="pressure">—</div></div>
</div><p id="status">Načítání…</p><small id="clock"></small></section>
<section><div class="row"><h2>Historie</h2><label>Rozsah<select id="hours"><option value="8">8 hodin</option><option value="24">24 hodin</option><option value="168">7 dní</option><option value="720">30 dní</option></select></label><button id="refresh">Obnovit</button></div>
<canvas id="chart" role="img" aria-label="Graf teploty a vlhkosti; mezery označují chybějící data"></canvas>
<p>Oranžová: teplota °C · modrá: vlhkost % RH. Svislé úsečky zachovávají minimum a maximum. Čas je relativní k nynějšku.</p>
<button id="download">Stáhnout zvolený rozsah CSV</button> <a href="/api/export.csv">Všechna data CSV</a>
<p><small>CSV obsahuje i neplatná měření. Chybějící UTC zůstává prázdné; relace a uplynulý čas jsou zachovány.</small></p></section>
<section><h2>Konfigurace</h2><form id="config"><div class="row">
<label>Interval měření (60–3600 s)<input id="interval" type="number" min="60" max="3600" required></label>
<label>Zhasnutí displeje (5–120 s)<input id="timeout" type="number" min="5" max="120" required></label>
<label>Jas (1–255)<input id="brightness" type="number" min="1" max="255" required></label></div>
<div class="row"><label>Wi-Fi SSID<input id="ssid" maxlength="32" autocomplete="off"></label>
<label>Heslo Wi-Fi (prázdné = ponechat)<input id="wifiPassword" type="password" maxlength="63" autocomplete="new-password"></label></div>
<label><input id="clearWifi" type="checkbox"> Zapomenout Wi-Fi a použít vlastní přístupový bod</label>
<label>Nové heslo webu i přístupového bodu (12–63 znaků; prázdné = ponechat)<input id="adminPassword" type="password" minlength="12" maxlength="63" autocomplete="new-password"></label>
<button>Uložit konfiguraci</button></form>
<p id="retention"></p><div class="row"><button id="sync">Nastavit čas podle prohlížeče</button><button id="backup">Export konfigurace</button><label>Import konfigurace<input id="restore" type="file" accept="application/json,.json"></label><button id="restart">Restartovat</button></div>
<p><small>Export konfigurace neobsahuje hesla. Změna Wi-Fi vyžaduje restart. Pro změny a OTA je nutné externí napájení; přístup je chráněn přihlášením. Rozhraní používej v důvěryhodné místní síti.</small></p></section>
<section><h2>Aktualizace firmwaru</h2><p id="version"></p>
<p>Vyber <code>firmware.bin</code> pro původní T-Display a stejnou variantu zapojení. Aktualizace zachová konfiguraci a historii. Během aktualizace může vzniknout mezera v měření.</p>
<form id="ota"><input id="firmware" type="file" accept=".bin" required><button>Nahrát firmware</button></form>
<progress id="progress" value="0" max="100"></progress><p id="otaStatus"></p></section>
<script>
'use strict';
let token='',state=null,busy=false,historyData=[];
const $=id=>document.getElementById(id),say=s=>$('message').textContent=s;
async function api(path,data){const options={cache:'no-store'};if(data!==undefined){options.method='POST';options.headers={'Content-Type':'application/json','X-CSRF-Token':token};options.body=JSON.stringify(data)}const r=await fetch(path,options);const body=await r.json();if(!r.ok)throw Error(body.error||('HTTP '+r.status));return body}
function configObject(){return {interval_s:Number($('interval').value),screen_timeout_s:Number($('timeout').value),brightness:Number($('brightness').value),ssid:$('ssid').value}}
function fillConfig(c){$('interval').value=c.interval_s;$('timeout').value=c.screen_timeout_s;$('brightness').value=c.brightness;$('ssid').value=c.ssid||''}
function retention(){const n=Number($('interval').value)||60;const bytes=30*86400/n*24;$('retention').textContent='30 dní při tomto intervalu: přibližně '+Math.ceil(bytes/1024)+' KiB dat. Skutečnou délku omezuje volné místo a režie úložiště.'}
async function status(first=false){
 state=await api('/api/status');token=state.csrf;
 $('temp').textContent=state.sensor_ok?state.temperature.toFixed(1)+' °C':'—';
 $('rh').textContent=state.sensor_ok&&state.has_humidity&&Number.isFinite(state.humidity)?state.humidity.toFixed(1)+' %':state.sensor_ok&&!state.has_humidity?'Neměří':'—';
 $('pressure').textContent=state.sensor_ok?state.pressure_hpa.toFixed(1)+' hPa':'—';
 $('status').textContent=(state.sensor_type||'Čidlo')+' · '+state.network+' · '+(state.external?'externí napájení':'baterie')+' · historie '+Math.round(state.used_bytes/1024)+' / '+Math.round(state.total_bytes/1024)+' KiB'+(state.storage_error?' · '+state.storage_error:'')+(state.sensor_error?' · '+state.sensor_error:'');
 $('clock').textContent=state.epoch?'Čas UTC: '+new Date(state.epoch*1000).toISOString():'Kalendářní čas není nastaven. Relace '+state.session+', uplynulo '+state.elapsed_s+' s.';
 $('version').textContent='Firmware '+state.version+' · '+state.variant+' · návrat při selhání prvního startu: aplikační boot guard';
 if(first){fillConfig(state.config);retention()}
}
async function history(){historyData=(await api('/api/history?hours='+$('hours').value)).bins;draw()}
function draw(){
 const c=$('chart'),dpr=window.devicePixelRatio||1,w=c.clientWidth,h=270;c.width=w*dpr;c.height=h*dpr;
 const x=c.getContext('2d');x.scale(dpr,dpr);x.clearRect(0,0,w,h);x.font='12px system-ui';
 const left=48,right=w-12,width=right-left;
 for(let ch=0;ch<2;ch++){
  const available=b=>b&&Number.isFinite(b[ch*2])&&Number.isFinite(b[ch*2+1]);
  let low=Infinity,high=-Infinity;
  for(const b of historyData)if(available(b)){low=Math.min(low,b[ch*2]);high=Math.max(high,b[ch*2+1])}
  const empty=!Number.isFinite(low);if(empty){low=0;high=1}if(high-low<1){low-=.5;high+=.5}
  const top=ch*130+22,bottom=top+76,color=ch?'#82d8eb':'#ffbd78';x.strokeStyle='#557080';x.beginPath();x.moveTo(left,top);x.lineTo(left,bottom);x.lineTo(right,bottom);x.stroke();
  x.fillStyle=color;x.fillText(ch?'Vlhkost % RH':'Teplota °C',left,top-7);
  if(!empty){x.fillText(high.toFixed(1),2,top+5);x.fillText(low.toFixed(1),2,bottom)}
  let prev=null;
  historyData.forEach((b,i)=>{if(!available(b)){prev=null;return}
   const px=left+i*width/Math.max(1,historyData.length-1),y1=bottom-(b[ch*2+1]-low)/(high-low)*76,y2=bottom-(b[ch*2]-low)/(high-low)*76,py=(y1+y2)/2;
   x.strokeStyle=color;x.beginPath();x.moveTo(px,y1);x.lineTo(px,y2+1);
   if(prev&&prev.session===b[4]){x.moveTo(prev.px,prev.py);x.lineTo(px,py)}x.stroke();prev={px,py,session:b[4]}
  });
  x.fillStyle='#afc3cf';x.fillText('-'+$('hours').value+' h',left,bottom+17);x.fillText('nyní',right-25,bottom+17);
  if(empty)x.fillText(ch&&state&&!state.has_humidity?'Připojené čidlo vlhkost neměří':'Zatím nejsou dostupná data',left+20,top+45)
 }
}
$('refresh').onclick=()=>history().catch(e=>say(e.message));$('hours').onchange=$('refresh').onclick;window.addEventListener('resize',draw);$('interval').oninput=retention;
$('download').onclick=()=>{location.href='/api/export.csv?hours='+$('hours').value};
$('config').onsubmit=async e=>{e.preventDefault();try{const c=configObject();if($('wifiPassword').value)c.wifi_password=$('wifiPassword').value;if($('adminPassword').value)c.admin_password=$('adminPassword').value;if($('clearWifi').checked)c.clear_wifi=true;await api('/api/config',c);say('Konfigurace uložena. Změnu sítě použije restart; nové heslo webu platí ihned.');$('wifiPassword').value='';$('adminPassword').value='';retention()}catch(e){say(e.message)}};
$('sync').onclick=()=>api('/api/time',{epoch:Math.floor(Date.now()/1000)}).then(()=>status()).then(()=>say('Čas nastaven.')).catch(e=>say(e.message));
$('backup').onclick=()=>{const blob=new Blob([JSON.stringify({version:1,...configObject()},null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='t-display-config.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000)};
$('restore').onchange=async()=>{try{const f=$('restore').files[0];if(!f||f.size>4096)throw Error('Vyber JSON do 4 KiB.');const c=JSON.parse(await f.text());if(c.version!==1)throw Error('Nepodporovaná verze konfigurace.');fillConfig(c);retention();say('Konfigurace načtena do formuláře. Zkontroluj ji a stiskni Uložit.')}catch(e){say(e.message)}};
$('restart').onclick=async()=>{if(confirm('Restartovat zařízení?'))try{await api('/api/restart',{});say('Zařízení se restartuje. Obnov stránku za několik sekund.')}catch(e){say(e.message)}};
$('ota').onsubmit=e=>{e.preventDefault();if(busy)return;const f=$('firmware').files[0];if(!f||!confirm('Nahrát firmware '+f.name+' a restartovat zařízení?'))return;busy=true;$('ota').querySelector('button').disabled=true;const xhr=new XMLHttpRequest(),data=new FormData();data.append('firmware',f,f.name);xhr.open('POST','/api/update');xhr.setRequestHeader('X-CSRF-Token',token);xhr.timeout=180000;xhr.upload.onprogress=e=>{if(e.lengthComputable)$('progress').value=100*e.loaded/e.total};xhr.onload=()=>{busy=false;$('ota').querySelector('button').disabled=false;let message;try{const r=JSON.parse(xhr.responseText);message=r.error||'Firmware přijat. Zařízení se restartuje; stránku obnov po opětovném připojení.'}catch{message='Neočekávaná odpověď. Ověř stav zařízení.'}$('otaStatus').textContent=message};xhr.onerror=xhr.ontimeout=()=>{busy=false;$('ota').querySelector('button').disabled=false;$('otaStatus').textContent='Spojení přerušeno. Ověř verzi po opětovném připojení.'};xhr.send(data)};
status(true).then(history).catch(e=>say(e.message));setInterval(()=>{if(!busy)status().catch(e=>say('Zařízení nedostupné: '+e.message))},10000);
</script></html>
)HTML";
