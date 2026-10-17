// Moments firmware - web console (single page, vanilla HTML/CSS/JS, served from flash).
#pragma once

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Moments</title>
<style>
:root{--bg:#f6f5f2;--card:#fff;--ink:#1d1d1f;--mut:#6b6b70;--line:#e3e1dc;--bad:#c62828}
*{box-sizing:border-box}body{margin:0;font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--ink)}
header{position:sticky;top:0;z-index:5;background:var(--ink);color:#fff;display:flex;align-items:center;gap:14px;padding:10px 16px;flex-wrap:wrap}
header b{font-size:18px;letter-spacing:.5px}
nav{display:flex;gap:4px;flex-wrap:wrap}nav a{color:#ccc;text-decoration:none;padding:6px 10px;border-radius:8px;cursor:pointer}nav a.on{background:#fff;color:var(--ink)}
#hdr{margin-left:auto;color:#bbb;font-size:13px}
main{max-width:1100px;margin:0 auto;padding:16px 16px 90px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:16px}
.card .card{margin:8px 0;padding:10px}
.card h2{margin:0 0 12px;font-size:17px}h3{margin:14px 0 6px;font-size:14px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px}
.stat{background:var(--bg);border-radius:10px;padding:10px 12px}.stat small{color:var(--mut);display:block}.stat b{font-size:16px}
label.f{display:flex;flex-direction:column;gap:4px;margin:6px 0;font-size:13px;color:var(--mut)}
label.c{display:flex;align-items:center;gap:8px;margin:8px 0}
input,select,textarea{font:inherit;padding:8px 10px;border:1px solid var(--line);border-radius:8px;background:#fff;color:var(--ink);width:100%}
input[type=checkbox]{width:auto}input[type=range]{padding:0}textarea{min-height:90px}
button{font:inherit;padding:8px 14px;border-radius:9px;border:1px solid var(--ink);background:#fff;color:var(--ink);cursor:pointer}
button.p{background:var(--ink);color:#fff}button.d{border-color:var(--bad);color:var(--bad)}button.s{padding:4px 9px;font-size:13px}
.row{display:flex;gap:8px;align-items:flex-end;flex-wrap:wrap}.row>*{flex:1;min-width:120px}.row>button,.row>.k{flex:none;min-width:0}
.list .it{display:flex;align-items:center;gap:8px;padding:8px 0;border-bottom:1px solid var(--line)}.list .it:last-child{border:0}
.it .t{flex:1}.mut{color:var(--mut);font-size:13px}.chips{display:flex;gap:6px;flex-wrap:wrap;margin:4px 0}
.chip{padding:5px 10px;border:1px solid var(--line);border-radius:20px;cursor:pointer;user-select:none;font-size:13px;background:#fff}.chip.on{background:var(--ink);color:#fff;border-color:var(--ink)}
.hidden{display:none!important}
#savebar{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:var(--ink);color:#fff;padding:10px 16px;border-radius:12px;display:flex;gap:12px;align-items:center;z-index:9;box-shadow:0 6px 20px #0003}
#savebar button{border-color:#fff}
#modal{position:fixed;inset:0;background:#0006;z-index:20;display:flex;align-items:flex-start;justify-content:center;overflow:auto;padding:30px 10px}
#modal .box{background:#fff;border-radius:16px;padding:18px;max-width:680px;width:100%}
#toast{position:fixed;top:70px;left:50%;transform:translateX(-50%);background:var(--ink);color:#fff;padding:8px 14px;border-radius:10px;z-index:30}
.designer{display:flex;gap:16px;flex-wrap:wrap;align-items:flex-start}
.gridbox{position:relative;background-color:#fff;border:1px solid var(--ink);touch-action:none;flex:none}
.wbox{position:absolute;border:2px solid #555;background:#f2f1ec;border-radius:6px;font-size:11px;padding:3px;overflow:hidden;cursor:move;touch-action:none;user-select:none}
.wbox.sel{border-color:#d4570f;background:#fff4ec}.wbox .rs{position:absolute;right:0;bottom:0;width:16px;height:16px;background:#555;cursor:nwse-resize}
.prev{max-width:100%;border:1px solid var(--line);background:#fff;margin-top:10px}
.side{flex:1;min-width:260px}
.pill{display:inline-block;padding:2px 8px;border-radius:10px;background:var(--bg);font-size:12px}
.warn{background:#fff6e0;border:1px solid #f0c36d;border-radius:10px;padding:8px 12px;margin-bottom:12px}
pre{background:var(--bg);padding:8px;border-radius:8px}
</style></head><body>
<header><b>Moments</b><nav id="nav"></nav><span id="hdr"></span></header>
<main id="main"></main>
<div id="savebar" class="hidden"><span>Unsaved changes</span><button onclick="saveCfg()">Save</button><button onclick="discard()">Discard</button></div>
<div id="modal" class="hidden"></div>
<div id="toast" class="hidden"></div>
<script>
const $=(s,r=document)=>r.querySelector(s);
function h(t,a,...k){const e=document.createElement(t);if(a)for(const n in a){const v=a[n];if(n==='class')e.className=v;else if(n==='style')e.style.cssText=v;else if(n.startsWith('on'))e[n]=v;else if(v!==undefined&&v!==null&&v!==false)e.setAttribute(n,v===true?'':v);}
for(const c of k.flat()){if(c==null||c===false)continue;e.append(c.nodeType?c:document.createTextNode(String(c)));}return e;}
let TOK=localStorage.getItem('mtok')||'',CFG=null,ORIG='',WDEF=[],SEC={},ST=null,LISTS=null,IMAGES=[],PAGE=location.hash.slice(1)||'home',TIMER=null,SEL_SCREEN=null,SEL_W=null;
const PAGES=[['home','Home'],['moments','Moments'],['screens','Screens'],['apps','Apps'],['content','Content'],['connections','Connections'],['system','System']];
const POSES=[['PS','Portrait · Stand'],['LS','Landscape · Stand'],['PV','Portrait · Vertical'],['LV','Landscape · Vertical']];
const APPS=[['chef','Chef'],['lists','Lists'],['timer','Timer'],['gallery','Gallery'],['guardian','Guardian']];
const DAYS=['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
const CET='CET-1CEST,M3.5.0,M10.5.0/3',EET='EET-2EEST,M3.5.0/3,M10.5.0/4',USE='EST5EDT,M3.2.0,M11.1.0',USC='CST6CDT,M3.2.0,M11.1.0',USM='MST7MDT,M3.2.0,M11.1.0',USP='PST8PDT,M3.2.0,M11.1.0',AUE='AEST-10AEDT,M10.1.0,M4.1.0/3';
const TZ={'Europe/London':'GMT0BST,M3.5.0/1,M10.5.0','Europe/Dublin':'IST-1GMT0,M10.5.0,M3.5.0/1','Europe/Lisbon':'WET0WEST,M3.5.0/1,M10.5.0','Atlantic/Canary':'WET0WEST,M3.5.0/1,M10.5.0',
'Europe/Istanbul':'<+03>-3','Europe/Moscow':'MSK-3','Asia/Dubai':'<+04>-4','Asia/Karachi':'PKT-5','Asia/Kolkata':'IST-5:30','Asia/Calcutta':'IST-5:30','Asia/Kathmandu':'<+0545>-5:45','Asia/Dhaka':'<+06>-6',
'Asia/Bangkok':'<+07>-7','Asia/Jakarta':'WIB-7','Asia/Ho_Chi_Minh':'<+07>-7','Asia/Singapore':'<+08>-8','Asia/Kuala_Lumpur':'<+08>-8','Asia/Hong_Kong':'HKT-8','Asia/Shanghai':'CST-8','Asia/Taipei':'CST-8','Asia/Manila':'PST-8',
'Asia/Tokyo':'JST-9','Asia/Seoul':'KST-9','Asia/Jerusalem':'IST-2IDT,M3.4.4/26,M10.5.0','Asia/Riyadh':'<+03>-3','Asia/Tehran':'<+0330>-3:30','Australia/Brisbane':'AEST-10','Australia/Adelaide':'ACST-9:30ACDT,M10.1.0,M4.1.0/3',
'Australia/Darwin':'ACST-9:30','Australia/Perth':'AWST-8','Pacific/Auckland':'NZST-12NZDT,M9.5.0,M4.1.0/3','Pacific/Honolulu':'HST10','America/Phoenix':'MST7','America/Anchorage':'AKST9AKDT,M3.2.0,M11.1.0',
'America/Halifax':'AST4ADT,M3.2.0,M11.1.0','America/St_Johns':'NST3:30NDT,M3.2.0,M11.1.0','America/Mexico_City':'CST6','America/Bogota':'<-05>5','America/Lima':'<-05>5','America/Sao_Paulo':'<-03>3',
'America/Argentina/Buenos_Aires':'<-03>3','America/Santiago':'<-04>4<-03>,M9.1.6/24,M4.1.6/24','Africa/Johannesburg':'SAST-2','Africa/Cairo':'EET-2EEST,M4.5.5/0,M10.5.4/24','Africa/Lagos':'WAT-1','Africa/Nairobi':'EAT-3',
'Africa/Casablanca':'<+01>-1','UTC':'UTC0','Etc/UTC':'UTC0'};
for(const z of ['Paris','Berlin','Madrid','Rome','Amsterdam','Brussels','Vienna','Zurich','Stockholm','Oslo','Copenhagen','Prague','Warsaw','Budapest','Belgrade','Zagreb','Luxembourg','Monaco','Malta','Bratislava','Ljubljana'])TZ['Europe/'+z]=CET;
for(const z of ['Athens','Helsinki','Kiev','Kyiv','Bucharest','Sofia','Riga','Vilnius','Tallinn'])TZ['Europe/'+z]=EET;
for(const z of ['New_York','Toronto','Detroit','Montreal','Indiana/Indianapolis','Kentucky/Louisville'])TZ['America/'+z]=USE;
for(const z of ['Chicago','Winnipeg'])TZ['America/'+z]=USC;for(const z of ['Denver','Edmonton','Boise'])TZ['America/'+z]=USM;for(const z of ['Los_Angeles','Vancouver','Tijuana'])TZ['America/'+z]=USP;
for(const z of ['Sydney','Melbourne','Canberra','Hobart'])TZ['Australia/'+z]=AUE;
function tzPosix(z){if(!z)return null;if(TZ[z])return TZ[z];if(z===Intl.DateTimeFormat().resolvedOptions().timeZone){const y=new Date().getFullYear(),a=new Date(y,0,1).getTimezoneOffset(),b=new Date(y,6,1).getTimezoneOffset();if(a===b){const m=a,s=m>=0?'':'-',x=Math.abs(m);return '<'+(m<=0?'+':'-')+String(Math.floor(x/60)).padStart(2,'0')+String(x%60).padStart(2,'0')+'>'+s+Math.floor(x/60)+(x%60?':'+String(x%60).padStart(2,'0'):'');}}return null;}
function toast(m){const t=$('#toast');t.textContent=m;t.classList.remove('hidden');clearTimeout(t._t);t._t=setTimeout(()=>t.classList.add('hidden'),2800);}
async function api(p,o={}){o.headers=Object.assign({'X-Token':TOK},o.headers||{});if(o.json!==undefined){o.method=o.method||'POST';o.body=JSON.stringify(o.json);o.headers['Content-Type']='application/json';}
const r=await fetch(p,o);if(r.status===401){showLogin();throw new Error('Please log in');}
const ct=r.headers.get('content-type')||'';if(!r.ok){let m='HTTP '+r.status;try{m=(await r.json()).error||m}catch(e){}throw new Error(m);}
return ct.includes('json')?r.json():r;}
async function act(a,extra={}){try{const r=await api('/api/action',{json:Object.assign({a},extra)});toast(r.msg||(r.ok===false?'Failed':'Done'));return r;}catch(e){toast(e.message);return null;}}
function touch(){if(CFG&&JSON.stringify(CFG)!==ORIG)$('#savebar').classList.remove('hidden');else $('#savebar').classList.add('hidden');}
async function saveCfg(){try{await api('/api/config',{json:CFG});ORIG=JSON.stringify(CFG);touch();toast('Saved');}catch(e){toast(e.message);}}
function discard(){CFG=JSON.parse(ORIG);touch();render();}
function showLogin(){const m=$('#modal');m.innerHTML='';const pin=h('input',{type:'text',inputmode:'numeric',placeholder:'6-digit PIN',maxlength:6,autocomplete:'off'});
const go=async()=>{try{const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pin:pin.value.trim()})});const j=await r.json();if(!r.ok)throw new Error(j.error||'Login failed');TOK=j.token;localStorage.setItem('mtok',TOK);m.classList.add('hidden');boot();}catch(e){toast(e.message);}};
pin.onkeydown=e=>{if(e.key==='Enter')go();};
m.append(h('div',{class:'box'},h('h2',{},'Connect to Moments'),h('p',{class:'mut'},'Enter the PIN shown on the device. To show it, hold the AI button for 3 seconds.'),pin,h('div',{class:'row',style:'margin-top:12px'},h('button',{class:'p',onclick:go},'Log in'))));m.classList.remove('hidden');pin.focus();}
function modal(title,body,onok,okText='OK'){const m=$('#modal');m.innerHTML='';const close=()=>m.classList.add('hidden');
m.append(h('div',{class:'box'},h('h2',{},title),body,h('div',{class:'row',style:'margin-top:14px;justify-content:flex-end'},h('button',{class:'k',onclick:close},onok?'Cancel':'Close'),onok?h('button',{class:'p k',onclick:()=>{if(onok()===false)return;close();}},okText):null)));m.classList.remove('hidden');}
function inp(o,k,type='text',at={}){const e=h('input',Object.assign({type},at));if(type==='checkbox'){e.checked=!!o[k];e.onchange=()=>{o[k]=e.checked;touch();};}
else{e.value=o[k]??'';e.oninput=()=>{o[k]=(type==='number'||type==='range')?(e.value===''?null:Number(e.value)):e.value;touch();};}return e;}
function sel(o,k,opts,cb){const e=h('select',{});for(const op of opts){const [v,l]=Array.isArray(op)?op:[op,op];e.append(h('option',{value:v},l));}
const first=opts.length?(Array.isArray(opts[0])?opts[0][0]:opts[0]):'';e.value=(o[k]??first);if(o[k]===undefined)o[k]=first;e.onchange=()=>{o[k]=e.value;touch();if(cb)cb();};return e;}
function selNum(o,k,opts){const e=sel({v:String(o[k]??0)},'v',opts.map(x=>[String(x[0]),x[1]]));e.value=String(o[k]??0);e.onchange=()=>{o[k]=Number(e.value);touch();};return e;}
function fld(l,e){return h('label',{class:'f'},l,e);}
function chk(o,k,l){return h('label',{class:'c'},inp(o,k,'checkbox'),l);}
function chips(arr,opts,onch){const box=h('div',{class:'chips'});for(const op of opts){const [v,l]=Array.isArray(op)?op:[op,op];const c=h('span',{class:'chip'+(arr.includes(v)?' on':'')},l);
c.onclick=()=>{const i=arr.indexOf(v);if(i>=0)arr.splice(i,1);else arr.push(v);c.classList.toggle('on');touch();if(onch)onch();};box.append(c);}return box;}
function lines(o,k){const e=h('textarea',{});e.value=(o[k]||[]).join('\n');e.oninput=()=>{o[k]=e.value.split('\n').map(s=>s.trim()).filter(Boolean);touch();};return e;}
function area(o,k){const e=h('textarea',{});e.value=o[k]||'';e.oninput=()=>{o[k]=e.value;touch();};return e;}
function card(t,...k){return h('div',{class:'card'},t?h('h2',{},t):null,...k);}
function uid(p){return p+Math.random().toString(36).slice(2,8);}
const SAVED='•••••• saved (type to replace)';
function secretInput(key,label,ph=''){const e=h('input',{type:'password',placeholder:SEC[key]?SAVED:ph,autocomplete:'new-password'});
const b=h('button',{class:'k',onclick:async()=>{try{await api('/api/secrets',{json:{[key]:e.value}});SEC[key]=!!e.value;e.value='';e.placeholder=SEC[key]?SAVED:ph;toast(SEC[key]?'Saved':'Cleared');}catch(x){toast(x.message);}}},'Save');
return fld(label,h('div',{class:'row'},e,b));}
function nav(){const n=$('#nav');n.innerHTML='';for(const [id,l] of PAGES)n.append(h('a',{class:PAGE===id?'on':'',onclick:()=>{location.hash=id;}},l));}
window.onhashchange=()=>{PAGE=location.hash.slice(1)||'home';render();};
async function boot(){try{[CFG,WDEF,SEC]=await Promise.all([api('/api/config'),api('/api/widgets'),api('/api/secrets')]);ORIG=JSON.stringify(CFG);
try{LISTS=await api('/api/lists');IMAGES=await api('/api/images');}catch(e){}render();poll();}catch(e){if(e.message!=='Please log in')toast(e.message);}}
async function poll(){clearTimeout(TIMER);try{ST=await api('/api/state');$('#hdr').textContent=(ST.moment||'')+' · '+ST.poseName+' · '+(ST.soc>=0?ST.soc+'%':'');if(PAGE==='home')live();}catch(e){}TIMER=setTimeout(poll,4000);}
function render(){if(!CFG)return;nav();const m=$('#main');m.innerHTML='';({home:pHome,moments:pMoments,screens:pScreens,apps:pApps,content:pContent,connections:pConn,system:pSystem}[PAGE]||pHome)(m);}
function stat(l,v){return h('div',{class:'stat'},h('small',{},l),h('b',{},v));}
function fmtT(t){if(!t||t<1e9)return '-';const d=new Date(t*1000);return d.toLocaleDateString(undefined,{weekday:'short'})+' '+d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});}

/* ---------------- Home ---------------- */
function pHome(m){
if(!CFG.imu.cal)m.append(h('div',{class:'warn'},'Orientation is not calibrated yet - do the two steps under "Orientation" so poses are detected correctly.'));
if(CFG.loc.lat==null)m.append(h('div',{class:'warn'},'Set your location in Connections for weather and sunrise.'));
if(!CFG.wifi.length)m.append(h('div',{class:'warn'},'Add your Wi-Fi in Connections so Moments can fetch data.'));
m.append(card('Now',h('div',{id:'live',class:'grid'})));
const why=h('div',{class:'list'});
m.append(card('Why this Moment?',why,h('button',{onclick:async()=>{why.innerHTML='';try{const w=await api('/api/why');why.append(h('div',{class:'it'},h('span',{class:'t'},h('b',{},w.moment||'-'),'  '+w.target),h('span',{class:'mut'},w.pose+' · '+w.daypart)));
if(w.override)why.append(h('div',{class:'it'},h('span',{class:'t'},'Override: '+w.override)));for(const x of w.moments)why.append(h('div',{class:'it'},h('span',{class:'t'},(x.match?'✓ ':'· ')+x.name),h('span',{class:'mut'},x.why)));}catch(e){toast(e.message);}}},'Explain')));
m.append(card('Quick actions',h('div',{class:'row'},h('button',{class:'k',onclick:()=>act('refresh')},'Refresh now'),h('button',{class:'k',onclick:()=>act('sound',{preset:'chime'})},'Test sound'),h('button',{class:'k',onclick:()=>act('notifyTest')},'Test notification'),h('button',{class:'k',onclick:()=>act('clearShow')},'Clear "Show"'),h('button',{class:'k',onclick:()=>act('exitConfig')},'Leave setup mode'))));
m.append(card('Orientation',h('p',{class:'mut'},'One-time setup. 1) "Show step 1", hold the device so the text on it reads upright, keep still, press "Step 1". 2) "Show step 2", turn it so the text reads upright again, keep still, press "Step 2".'),
h('div',{class:'row'},h('button',{class:'k',onclick:()=>act('calibrateShow',{step:1})},'Show step 1'),h('button',{class:'p k',onclick:async()=>{const r=await act('calibrate',{step:1});if(r&&r.ok)await syncImu();}},'Step 1'),h('button',{class:'k',onclick:()=>act('calibrateShow',{step:2})},'Show step 2'),
h('button',{class:'p k',onclick:async()=>{const r=await act('calibrate',{step:2});if(r&&r.ok){await syncImu();await act('clearShow');render();}}},'Step 2'))));
m.append(card('Recent events',h('div',{id:'events',class:'list'})));
live();}
async function syncImu(){const imu=(await api('/api/config')).imu;CFG.imu=imu;const o=JSON.parse(ORIG);o.imu=imu;ORIG=JSON.stringify(o);touch();}
function live(){const l=$('#live');if(!l||!ST)return;const s=ST;l.innerHTML='';
l.append(stat('Moment',s.moment||'-'),stat('Pose',s.poseName),stat('Time',s.local),stat('Daypart',s.daypart||'-'),stat('Battery',(s.soc>=0?s.soc+'%':'-')+(s.charging?' charging':'')+(s.usb?' · USB':'')),
stat('Indoor',s.temp!=null?s.temp.toFixed(1)+'°C · '+Math.round(s.hum)+'%':'-'),stat('Wi-Fi',s.wifi.up?(s.wifi.ssid+' ('+s.wifi.rssi+' dBm)'):(s.ap?'setup hotspot':'off')),stat('Guardian',s.guardian.enabled?s.guardian.state:'off'),stat('Owner',s.owner==1?'home':s.owner==0?'away':'unknown'),stat('Mode',s.mode));
const ev=$('#events');if(ev){ev.innerHTML='';for(const e of s.events)ev.append(h('div',{class:'it'},h('span',{class:'mut',style:'width:120px;flex:none'},fmtT(e.t)),h('span',{class:'t'},e.m)));}}

/* ---------------- Moments ---------------- */
function dpName(id){const d=CFG.dayparts.find(x=>x.id===id);return d?d.name:id;}
function daysTxt(b){if(b==null||b===127)return 'every day';if(b===31)return 'weekdays';if(b===96)return 'weekends';return DAYS.filter((d,i)=>b&(1<<i)).join(' ');}
function whatName(w){if(!w)return '-';const i=w.indexOf(':'),k=w.slice(0,i),id=w.slice(i+1);if(k==='screen'){const s=CFG.screens.find(x=>x.id===id);return s?s.name:id;}const a=APPS.find(x=>x[0]===id);return a?a[1]:w;}
function whatOptions(P){const o=[];for(const s of CFG.screens)if(P===undefined||(s.o==='P')===P)o.push(['screen:'+s.id,'Screen: '+s.name+' ('+(s.o==='P'?'portrait':'landscape')+')']);for(const a of APPS)o.push(['app:'+a[0],'App: '+a[1]]);return o;}
function momentSummary(mo){const p=(mo.poses||[]).map(x=>(POSES.find(y=>y[0]===x)||[0,x])[1]).join(', ')||'any pose';const d=(mo.dp||[]).map(dpName).join(', ');
return p+(d?' + '+d:'')+' · '+daysTxt(mo.days)+((mo.cond||[]).length?' · '+mo.cond.length+' condition(s)':'')+' → '+whatName(mo.what);}
const CONDS=[['temp','Temperature'],['hum','Humidity'],['sun','Sunrise / sunset'],['owner','Owner presence'],['battery','Battery'],['power','Power'],['time','Time window'],['pose','Pose'],['days','Days']];
function condEditor(arr){const box=h('div');const draw=()=>{box.innerHTML='';arr.forEach((c,i)=>{const r=h('div',{class:'row'});
r.append(sel(c,'t',CONDS,()=>{const t=c.t;for(const k in c)delete c[k];c.t=t;draw();}));
if(c.t==='temp'||c.t==='hum'||c.t==='battery'){if(!c.op)c.op='above';r.append(sel(c,'op',['above','below','between'],draw),inp(c,'v','number'));if(c.op==='between')r.append(inp(c,'v2','number'));}
else if(c.t==='sun'){if(!c.op)c.op='after';r.append(sel(c,'op',['before','after']),sel(c,'ev',['sunrise','sunset']),inp(c,'off','number',{placeholder:'± min'}));}
else if(c.t==='owner')r.append(sel(c,'v',[['home','is home'],['away','is away']]));
else if(c.t==='power')r.append(sel(c,'v',[['docked','docked (USB)'],['battery','on battery']]));
else if(c.t==='time')r.append(inp(c,'from','time'),inp(c,'to','time'));
else if(c.t==='pose'){if(!Array.isArray(c.v))c.v=[];r.append(chips(c.v,POSES));}
else if(c.t==='days'){if(typeof c.v!=='number')c.v=127;const sel2=h('div',{class:'chips'});const dd=()=>{sel2.innerHTML='';DAYS.forEach((d,j)=>{const ch=h('span',{class:'chip'+(c.v&(1<<j)?' on':'')},d);ch.onclick=()=>{c.v^=(1<<j);dd();};sel2.append(ch);});};dd();r.append(sel2);}
r.append(h('button',{class:'s d k',onclick:()=>{arr.splice(i,1);draw();}},'✕'));box.append(r);});
box.append(h('button',{class:'s',onclick:()=>{arr.push({t:'temp',op:'above',v:20});draw();}},'+ Condition'));};draw();return box;}
function editMoment(mo,isNew){const c=JSON.parse(JSON.stringify(mo));c.poses=c.poses||[];c.dp=c.dp||[];c.cond=c.cond||[];if(c.days==null)c.days=127;
const wb=h('div');const drawWhat=()=>{wb.innerHTML='';const ps=c.poses;const P=ps.length>0&&ps.every(x=>x[0]==='P'),L=ps.length>0&&ps.every(x=>x[0]==='L');
wb.append(fld('Show',sel(c,'what',whatOptions(P?true:(L?false:undefined)))));if(ps.length&&!P&&!L)wb.append(h('p',{class:'mut'},'Mixed orientations: pick an App (apps adapt), or use two Moments.'));};drawWhat();
const db=h('div',{class:'chips'});const drawDays=()=>{db.innerHTML='';DAYS.forEach((d,i)=>{const ch=h('span',{class:'chip'+(c.days&(1<<i)?' on':'')},d);ch.onclick=()=>{c.days^=(1<<i);drawDays();};db.append(ch);});
db.append(h('span',{class:'chip',onclick:()=>{c.days=31;drawDays();}},'Weekdays'),h('span',{class:'chip',onclick:()=>{c.days=96;drawDays();}},'Weekends'),h('span',{class:'chip',onclick:()=>{c.days=127;drawDays();}},'Every day'));};drawDays();
modal(isNew?'New Moment':'Edit Moment',h('div',{},fld('Name',inp(c,'name')),h('h3',{},'Poses (none = any)'),chips(c.poses,POSES,drawWhat),h('h3',{},'Dayparts (none = any time)'),chips(c.dp,CFG.dayparts.map(d=>[d.id,d.name])),h('h3',{},'Days'),db,h('h3',{},'Extra conditions'),condEditor(c.cond),h('h3',{},'What'),wb),
()=>{Object.assign(mo,c);if(isNew)CFG.moments.push(mo);touch();render();});}
const TRIGS=[['time','At a time'],['wake_time','At my wake time'],['daypart_start','Daypart starts'],['daypart_end','Daypart ends'],['sun','Sunrise / sunset'],['temp_above','Temperature rises above'],['temp_below','Temperature falls below'],['hum_above','Humidity rises above'],['hum_below','Humidity falls below'],['motion','Device moved'],['pose_became','Pose became'],['pose_lost','Pose lost'],['owner_left','Owner left'],['owner_arrived','Owner arrived'],['battery_below','Battery falls below %'],['usb_on','USB connected'],['usb_off','USB disconnected']];
const THR=['temp_above','temp_below','hum_above','hum_below','battery_below'];
function trigSummary(t){const n=(TRIGS.find(x=>x[0]===t.t)||[0,t.t])[1];if(t.t==='time')return n+' '+(t.at||'');if(THR.includes(t.t))return n+' '+t.v;if(t.t.startsWith('daypart'))return n+': '+dpName(t.v);if(t.t==='sun')return (t.ev||'sunrise')+(t.off?' '+(t.off>0?'+':'')+t.off+' min':'');return n;}
function editAuto(a,isNew){const c=JSON.parse(JSON.stringify(a));c.cond=c.cond||[];c.act=c.act||[];
const tb=h('div');const drawT=()=>{tb.innerHTML='';const t=c.trig;const r=h('div',{class:'row'},sel(t,'t',TRIGS,()=>{const k=t.t;for(const x in t)delete t[x];t.t=k;drawT();}));
if(t.t==='time')r.append(inp(t,'at','time'));else if(THR.includes(t.t))r.append(inp(t,'v','number'),inp(t,'hyst','number',{placeholder:'hysteresis'}));
else if(t.t.startsWith('daypart'))r.append(sel(t,'v',CFG.dayparts.map(d=>[d.id,d.name])));else if(t.t==='sun')r.append(sel(t,'ev',['sunrise','sunset']),inp(t,'off','number',{placeholder:'± min'}));
else if(t.t==='pose_became'){if(!Array.isArray(t.v))t.v=[];r.append(chips(t.v,POSES));}tb.append(r);};drawT();
const ab=h('div');const drawA=()=>{ab.innerHTML='';c.act.forEach((x,i)=>{const r=h('div',{class:'card'});r.append(h('div',{class:'row'},sel(x,'t',[['notify','Notify'],['sound','Sound'],['show','Show']],()=>{const k=x.t;for(const y in x)delete x[y];x.t=k;drawA();}),h('button',{class:'s d k',onclick:()=>{c.act.splice(i,1);drawA();}},'✕')));
if(x.t==='notify')r.append(fld('Title',inp(x,'title')),fld('Message - variables {{temp}} {{humidity}} {{pose}} {{time}} {{battery}} {{moment}}',inp(x,'msg')),h('div',{class:'row'},fld('Priority 1-5',inp(x,'pri','number',{min:1,max:5})),fld('Tags',inp(x,'tags')),fld('Click URL',inp(x,'click'))));
else if(x.t==='sound')r.append(fld('Sound',sel(x,'preset',['chime','double','alert','wake','siren','beep'])));
else if(x.t==='show')r.append(h('div',{class:'row'},fld('Show',sel(x,'what',whatOptions().concat([['sys:lowbat','Low-battery notice']]))),fld('Minutes',inp(x,'min','number'))));ab.append(r);});
ab.append(h('button',{class:'s',onclick:()=>{c.act.push({t:'notify',title:'Moments',msg:'',pri:3});drawA();}},'+ Action'));};drawA();
modal(isNew?'New automation':'Edit automation',h('div',{},fld('Name',inp(c,'name')),h('h3',{},'When (trigger)'),tb,h('h3',{},'Only if (conditions)'),condEditor(c.cond),fld('Cooldown (minutes)',inp(c,'cool','number')),h('h3',{},'Do'),ab),
()=>{Object.assign(a,c);if(isNew)CFG.autos.push(a);touch();render();});}
function pMoments(m){const lst=h('div',{class:'list'});
CFG.moments.forEach((mo,i)=>{lst.append(h('div',{class:'it'},h('input',{type:'checkbox',checked:mo.on!==false,onchange:e=>{mo.on=e.target.checked;touch();}}),h('span',{class:'t'},h('b',{},mo.name),h('div',{class:'mut'},momentSummary(mo))),
h('button',{class:'s',onclick:()=>{if(i>0){[CFG.moments[i-1],CFG.moments[i]]=[CFG.moments[i],CFG.moments[i-1]];touch();render();}}},'↑'),h('button',{class:'s',onclick:()=>{if(i<CFG.moments.length-1){[CFG.moments[i+1],CFG.moments[i]]=[CFG.moments[i],CFG.moments[i+1]];touch();render();}}},'↓'),
h('button',{class:'s',onclick:()=>editMoment(mo)},'Edit'),h('button',{class:'s d',onclick:()=>{if(confirm('Delete '+mo.name+'?')){CFG.moments.splice(i,1);touch();render();}}},'✕')));});
m.append(card('Screen & App Moments',h('p',{class:'mut'},'First match wins (top to bottom). Nothing matches → the Glance screen for the orientation. Guardian (when armed) overrides everything.'),lst,h('button',{class:'p',onclick:()=>editMoment({id:uid('m'),name:'New Moment',on:true,what:'screen:glance_p',poses:[],dp:[],days:127,cond:[]},true)},'+ New Moment')));
const al=h('div',{class:'list'});
CFG.autos.forEach((a,i)=>{al.append(h('div',{class:'it'},h('input',{type:'checkbox',checked:a.on!==false,onchange:e=>{a.on=e.target.checked;touch();}}),h('span',{class:'t'},h('b',{},a.name),h('div',{class:'mut'},trigSummary(a.trig)+' → '+a.act.map(x=>x.t).join(', '))),
h('button',{class:'s',onclick:()=>act('test',{id:a.id})},'Test'),h('button',{class:'s',onclick:()=>editAuto(a)},'Edit'),h('button',{class:'s d',onclick:()=>{if(confirm('Delete '+a.name+'?')){CFG.autos.splice(i,1);touch();render();}}},'✕')));});
m.append(card('Automation Moments',h('p',{class:'mut'},'Fire once when the trigger happens (with cooldown). "Test" runs the saved actions now.'),al,h('button',{class:'p',onclick:()=>editAuto({id:uid('a'),name:'New automation',on:true,trig:{t:'time',at:'08:00'},cond:[],cool:60,act:[{t:'notify',title:'Moments',msg:'Hello',pri:3}]},true)},'+ New automation')));
const dl=h('div');CFG.dayparts.forEach((d,i)=>{dl.append(h('div',{class:'row'},fld('Name',inp(d,'name')),fld('Start',inp(d,'start','time')),fld('Start anchor',sel(d,'startA',[['','clock time'],['sunrise','sunrise'],['sunset','sunset']])),fld('± min',inp(d,'startO','number')),fld('End',inp(d,'end','time')),h('button',{class:'s d k',onclick:()=>{CFG.dayparts.splice(i,1);touch();render();}},'✕')));});
m.append(card('Dayparts',h('p',{class:'mut'},'Named time windows used by Moments. A start can follow sunrise/sunset ± minutes (needs location).'),dl,h('button',{onclick:()=>{CFG.dayparts.push({id:uid('dp'),name:'New',start:'12:00',end:'13:00'});touch();render();}},'+ Daypart')));}

/* ---------------- Screens ---------------- */
function wdef(t){return WDEF.find(d=>d.type===t)||{type:t,name:t,minW:1,minH:1,maxW:10,maxH:10,schema:[]};}
function template(t,o){const P=o==='P',w=(t2,x,y,ww,hh,s)=>({id:uid('w'),t:t2,x,y,w:ww,h:hh,s:s||{}});
if(t==='minimal')return P?[w('clock',0,2,6,3,{style:'big',date:true}),w('weather_now',0,6,6,3)]:[w('clock',0,1,6,3,{style:'big',date:true}),w('weather_now',6,1,4,3)];
if(t==='dash')return P?[w('clock',0,0,6,2,{style:'big',date:true}),w('weather_now',0,2,6,2),w('agenda',0,4,6,3,{range:'today',max:5}),w('headlines',0,7,6,3,{feed:0,count:3})]:[w('clock',0,0,4,2,{style:'big',date:true}),w('weather_now',4,0,6,2),w('agenda',0,2,5,4,{range:'today',max:5}),w('headlines',5,2,5,4,{feed:0,count:4})];
if(t==='ready')return P?[w('outfit',0,0,6,6),w('hourly',0,6,6,2,{hours:12,show:'rain'}),w('next_event',0,8,6,2)]:[w('outfit',0,0,6,6),w('hourly',6,0,4,2,{hours:12,show:'rain'}),w('next_event',6,2,4,2),w('clock',6,4,4,2,{style:'big'})];
return [];}
function schemaField(o,f){if(o[f.k]===undefined&&f.d!==undefined)o[f.k]=f.d;
switch(f.t){case 'bool':return chk(o,f.k,f.l);case 'int':return fld(f.l,inp(o,f.k,'number',{min:f.min,max:f.max}));case 'select':return fld(f.l,sel(o,f.k,f.o));
case 'textarea':return fld(f.l,area(o,f.k));case 'feed':return fld(f.l,selNum(o,f.k,CFG.feeds.map((x,i)=>[i,x.name])));case 'list':return fld(f.l,selNum(o,f.k,(LISTS||[]).map((x,i)=>[i,x.name])));
case 'note':return fld(f.l,sel(o,f.k,[['','(first note)'],...CFG.content.notes.map(n=>[n.id,n.name])]));case 'image':return fld(f.l,sel(o,f.k,[['','(choose)'],...IMAGES.map(i=>[i,i])]));default:return fld(f.l,inp(o,f.k));}}
function designer(s){const P=s.o==='P',cols=P?6:10,rows=P?10:6;const avail=Math.min(window.innerWidth,1100)-80;const cell=Math.max(28,Math.min(56,Math.floor((P?avail*0.45:avail*0.62)/cols)));
const grid=h('div',{class:'gridbox',style:`width:${cols*cell}px;height:${rows*cell}px;background-image:linear-gradient(#eee 1px,transparent 1px),linear-gradient(90deg,#eee 1px,transparent 1px);background-size:${cell}px ${cell}px`});
const side=h('div',{class:'side'});const img=h('img',{class:'prev',alt:''});
const ov=(a,ign)=>s.w.some(b=>b!==ign&&a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h);
const drawGrid=()=>{grid.innerHTML='';for(const w of s.w){const d=wdef(w.t);const b=h('div',{class:'wbox'+(w===SEL_W?' sel':''),style:`left:${w.x*cell}px;top:${w.y*cell}px;width:${w.w*cell-2}px;height:${w.h*cell-2}px`},d.name);const rs=h('div',{class:'rs'});b.append(rs);
b.onpointerdown=e=>{e.preventDefault();const first=SEL_W!==w;SEL_W=w;const resize=e.target===rs;const sx=e.clientX,sy=e.clientY,o={x:w.x,y:w.y,w:w.w,h:w.h};b.setPointerCapture(e.pointerId);
b.onpointermove=ev=>{const dx=Math.round((ev.clientX-sx)/cell),dy=Math.round((ev.clientY-sy)/cell);const n=resize?{x:o.x,y:o.y,w:Math.max(d.minW,Math.min(d.maxW,o.w+dx)),h:Math.max(d.minH,Math.min(d.maxH,o.h+dy))}:{x:o.x+dx,y:o.y+dy,w:o.w,h:o.h};
n.w=Math.min(n.w,cols);n.h=Math.min(n.h,rows);n.x=Math.max(0,Math.min(cols-n.w,n.x));n.y=Math.max(0,Math.min(rows-n.h,n.y));if(!ov(n,w)){Object.assign(w,n);b.style.left=w.x*cell+'px';b.style.top=w.y*cell+'px';b.style.width=(w.w*cell-2)+'px';b.style.height=(w.h*cell-2)+'px';}};
b.onpointerup=()=>{b.onpointermove=null;b.onpointerup=null;touch();drawGrid();if(first||true)drawSide();};};grid.append(b);}};
const drawSide=()=>{side.innerHTML='';side.append(h('div',{class:'row'},fld('Name',inp(s,'name')),fld('Theme',sel(s,'theme',[['light','Light'],['dark','Inverted (dark)']]))));
const groups={};for(const d of WDEF)(groups[d.group]=groups[d.group]||[]).push(d);const as=h('select',{});for(const g in groups){const og=h('optgroup',{label:g});for(const d of groups[g])og.append(h('option',{value:d.type},d.name));as.append(og);}
side.append(h('div',{class:'row'},as,h('button',{class:'k',onclick:()=>{const d=wdef(as.value);const w={id:uid('w'),t:d.type,x:0,y:0,w:Math.min(Math.max(d.minW,3),cols,d.maxW),h:Math.max(d.minH,1),s:{}};for(const f of d.schema)if(f.d!==undefined)w.s[f.k]=f.d;
for(let y=0;y<=rows-w.h;y++)for(let x=0;x<=cols-w.w;x++){w.x=x;w.y=y;if(!ov(w)){s.w.push(w);SEL_W=w;touch();drawGrid();drawSide();return;}}toast('No free space - make room first');}},'+ Add widget')));
if(SEL_W&&s.w.includes(SEL_W)){const w=SEL_W,d=wdef(w.t);w.s=w.s||{};side.append(h('h3',{},d.name+' settings'),h('div',{class:'mut'},'id '+w.id+' · '+w.w+'×'+w.h+' cells (min '+d.minW+'×'+d.minH+')'));
for(const f of d.schema)side.append(schemaField(w.s,f));side.append(h('div',{class:'row',style:'margin-top:8px'},h('button',{class:'s d k',onclick:()=>{s.w.splice(s.w.indexOf(w),1);SEL_W=null;touch();drawGrid();drawSide();}},'Remove widget')));}
side.append(h('div',{class:'row',style:'margin-top:12px'},h('button',{class:'p k',onclick:async()=>{try{const r=await api('/api/preview',{json:s});img.src=URL.createObjectURL(await r.blob());}catch(e){toast(e.message);}}},'Device preview'),h('button',{class:'k',onclick:()=>act('show',{what:'screen:'+s.id,min:5})},'Show on device (5 min)')));side.append(img);};
drawGrid();drawSide();
return card('Designer: '+s.name+(P?' - portrait 6×10':' - landscape 10×6'),h('div',{class:'designer'},grid,side),h('p',{class:'mut'},'Drag to move, drag the corner to resize; tap a widget for its settings. Preview is rendered by the device. Save, then "Show on device".'));}
function pScreens(m){const lst=h('div',{class:'list'});
for(const s of CFG.screens)lst.append(h('div',{class:'it'},h('span',{class:'t'},h('b',{},s.name),' ',h('span',{class:'pill'},s.o==='P'?'portrait':'landscape'),' ',h('span',{class:'mut'},s.w.length+' widgets')),
h('button',{class:'s',onclick:()=>{SEL_SCREEN=s.id;SEL_W=null;render();}},'Design'),h('button',{class:'s',onclick:()=>{const c=JSON.parse(JSON.stringify(s));c.id=uid('s');c.name=s.name+' copy';c.w.forEach(w=>w.id=uid('w'));CFG.screens.push(c);touch();render();}},'Duplicate'),
h('button',{class:'s d',onclick:()=>{if(s.id.startsWith('glance'))return toast('Glance screens are the fallback and cannot be deleted');if(confirm('Delete '+s.name+'?')){CFG.screens.splice(CFG.screens.indexOf(s),1);touch();render();}}},'✕')));
const nn={name:'My screen',o:'P',tpl:'blank'};
m.append(card('Screens',lst,h('div',{class:'row',style:'margin-top:10px'},fld('Name',inp(nn,'name')),fld('Orientation',sel(nn,'o',[['P','Portrait'],['L','Landscape']])),fld('Template',sel(nn,'tpl',[['blank','Blank'],['minimal','Minimal clock'],['dash','Dashboard'],['ready','Get ready']])),
h('button',{class:'p k',onclick:()=>{const s={id:uid('s'),name:nn.name,o:nn.o,theme:'light',w:template(nn.tpl,nn.o)};CFG.screens.push(s);SEL_SCREEN=s.id;SEL_W=null;touch();render();}},'+ New screen'))));
const s=CFG.screens.find(x=>x.id===SEL_SCREEN);if(s)m.append(designer(s));}

/* ---------------- Apps ---------------- */
function pApps(m){const A=CFG.apps;const c=A.chef;c.windows=c.windows||{};
m.append(card('Chef',h('p',{class:'mut'},'Recipe ideas from your pantry (Content > Pantry) for the current meal. Needs an AI key (Connections).'),h('div',{class:'row'},fld('Diet',inp(c,'diet')),fld('Allergies (never use)',inp(c,'allergies'))),
h('div',{class:'row'},fld('Servings',inp(c,'servings','number')),fld('Max time (min)',inp(c,'maxTime','number')),fld('Cuisines',inp(c,'cuisines'))),h('h3',{},'Meal windows (start times)'),h('div',{class:'row'},...['breakfast','lunch','dinner','snack'].map(k=>fld(k,inp(c.windows,k,'time')))),
h('button',{onclick:()=>act('chef')},'Generate new ideas now')));
const t=A.timer;const pr={v:(t.presets||[]).join(',')};const pe=inp(pr,'v');pe.oninput=()=>{t.presets=pe.value.split(',').map(x=>parseInt(x)).filter(x=>x>0).slice(0,6);touch();};
m.append(card('Timer',fld('Presets (minutes, comma separated, max 6)',pe),h('div',{class:'row'},fld('Break (min)',inp(t,'brk','number')),fld('End sound',sel(t,'sound',['double','chime','alert','beep'])))));
const g=A.gallery;m.append(card('Gallery',h('div',{class:'row'},fld('Change every (min, 0 = never)',inp(g,'interval','number')),chk(g,'shuffle','Shuffle'),chk(g,'gray','4-level gray'))));
const gd=A.guardian;gd.poses=gd.poses||[];
m.append(card('Guardian',h('p',{class:'mut'},'Door notifier, not a security system. When the device is in an arming pose and moves (door opened), it checks for your phone; missing twice = armed. Motion while armed: countdown beeps, then siren and notification unless your phone is found.'),
chk(gd,'on','Enabled'),h('h3',{},'Arming poses'),chips(gd.poses,POSES),h('div',{class:'row'},fld('Grace time (s)',inp(gd,'grace','number')),fld('Siren seconds',inp(gd,'sirenSec','number')),chk(gd,'siren','Siren')),
h('div',{class:'row'},h('button',{class:'k',onclick:()=>act('guardianArm')},'Arm now (test)'),h('button',{class:'k',onclick:()=>act('guardianDisarm')},'Disarm'))));
m.append(card('Lists',h('p',{class:'mut'},'Edit lists in Content > Lists. On the device: tap to tick, swipe for the next list.')));}

/* ---------------- Content ---------------- */
function loadImg(file){return new Promise((res,rej)=>{const i=new Image();i.onload=()=>res(i);i.onerror=()=>rej(new Error('Cannot read image'));i.src=URL.createObjectURL(file);});}
async function imageTo2bpp(file,W,H){const img=await loadImg(file);const c=document.createElement('canvas');c.width=W;c.height=H;const x=c.getContext('2d');x.fillStyle='#fff';x.fillRect(0,0,W,H);
const s=Math.max(W/img.width,H/img.height),dw=img.width*s,dh=img.height*s;x.drawImage(img,(W-dw)/2,(H-dh)/2,dw,dh);const d=x.getImageData(0,0,W,H).data;const g=new Float32Array(W*H);
for(let i=0;i<W*H;i++)g[i]=0.299*d[i*4]+0.587*d[i*4+1]+0.114*d[i*4+2];const lv=[0,85,170,255],stride=Math.ceil(W/4);const out=new Uint8Array(4+stride*H);out[0]=W&255;out[1]=W>>8;out[2]=H&255;out[3]=H>>8;
for(let y=0;y<H;y++)for(let xx=0;xx<W;xx++){const i=y*W+xx,o=g[i],q=Math.max(0,Math.min(3,Math.round(o/85))),e=o-lv[q];if(xx+1<W)g[i+1]+=e*7/16;if(y+1<H){if(xx>0)g[i+W-1]+=e*3/16;g[i+W]+=e*5/16;if(xx+1<W)g[i+W+1]+=e/16;}out[4+y*stride+(xx>>2)]|=q<<(6-2*(xx&3));}
return out;}
async function upload(kind,id,bin){const fd=new FormData();fd.append('file',new Blob([bin]),id+'.bin');const r=await fetch('/api/upload?kind='+kind+'&id='+encodeURIComponent(id),{method:'POST',headers:{'X-Token':TOK},body:fd});
if(!r.ok){let m='Upload failed';try{m=(await r.json()).error||m}catch(e){}throw new Error(m);}}
function pickThumb(it){const f=h('input',{type:'file',accept:'image/*'});f.onchange=async()=>{try{const img=await loadImg(f.files[0]);const c=document.createElement('canvas');c.width=64;c.height=64;const x=c.getContext('2d');x.fillStyle='#fff';x.fillRect(0,0,64,64);
const s=Math.min(64/img.width,64/img.height);x.drawImage(img,(64-img.width*s)/2,(64-img.height*s)/2,img.width*s,img.height*s);const d=x.getImageData(0,0,64,64).data;const g=new Float32Array(4096);for(let i=0;i<4096;i++)g[i]=0.299*d[i*4]+0.587*d[i*4+1]+0.114*d[i*4+2];
const out=new Uint8Array(512);for(let y=0;y<64;y++)for(let xx=0;xx<64;xx++){const i=y*64+xx,q=g[i]>=128?255:0,e=g[i]-q;if(xx<63)g[i+1]+=e*7/16;if(y<63){if(xx>0)g[i+63]+=e*3/16;g[i+64]+=e*5/16;if(xx<63)g[i+65]+=e/16;}if(q)out[y*8+(xx>>3)]|=0x80>>(xx&7);}
await upload('thumb',it.id,out);toast('Photo saved');}catch(e){toast(e.message);}};f.click();}
function editItem(it,isNew){const c=Object.assign({},it);modal(isNew?'New item':'Edit item',h('div',{},fld('Name',inp(c,'name')),h('div',{class:'row'},fld('Category',sel(c,'cat',['top','bottom','outer','shoes','accessory'])),fld('Warmth 0-5',inp(c,'warm','number',{min:0,max:5})),fld('Formality 1-5',inp(c,'form','number',{min:1,max:5}))),
h('div',{class:'row'},fld('Color',inp(c,'color')),chk(c,'rain','Rain-OK'))),()=>{if(!c.name){toast('Name required');return false;}Object.assign(it,c);if(isNew)CFG.content.wardrobe.push(it);touch();render();});}
async function drawLists(box){box.innerHTML='';if(!LISTS){try{LISTS=await api('/api/lists');}catch(e){box.textContent=e.message;return;}}
LISTS.forEach((l,i)=>{const ta=h('textarea',{});ta.value=l.items.map(x=>(x.d?'[x] ':'')+x.t).join('\n');ta.oninput=()=>{l.items=ta.value.split('\n').map(s=>s.trim()).filter(Boolean).map(s=>({t:s.replace(/^\[x\]\s*/i,''),d:/^\[x\]/i.test(s)}));};
const nm=h('input',{type:'text'});nm.value=l.name;nm.oninput=()=>{l.name=nm.value;};box.append(h('div',{class:'card'},h('div',{class:'row'},nm,h('button',{class:'s d k',onclick:()=>{LISTS.splice(i,1);drawLists(box);}},'✕')),ta));});
box.append(h('p',{class:'mut'},'One item per line; prefix [x] for ticked.'),h('div',{class:'row'},h('button',{class:'k',onclick:()=>{LISTS.push({name:'New list',items:[]});drawLists(box);}},'+ List'),h('button',{class:'p k',onclick:async()=>{try{await api('/api/lists',{json:LISTS});toast('Lists saved');}catch(e){toast(e.message);}}},'Save lists')));}
async function drawImages(box){try{IMAGES=await api('/api/images');}catch(e){return;}box.innerHTML='';const l=h('div',{class:'list'});
for(const id of IMAGES)l.append(h('div',{class:'it'},h('span',{class:'t'},id),h('button',{class:'s d',onclick:async()=>{await api('/api/images/delete',{json:{id}});drawImages(box);}},'Delete')));
const f=h('input',{type:'file',accept:'image/*'});const o={o:'L',name:''};
box.append(l,h('div',{class:'row',style:'margin-top:10px'},f,sel(o,'o',[['L','Landscape 800×480'],['P','Portrait 480×800']]),inp(o,'name','text',{placeholder:'name (letters, numbers)'}),
h('button',{class:'p k',onclick:async()=>{if(!f.files[0])return toast('Choose a photo');const id=(o.name||f.files[0].name.replace(/\.[^.]+$/,'')).replace(/[^a-zA-Z0-9_-]/g,'').slice(0,20)||uid('img');
try{toast('Converting...');const bin=await imageTo2bpp(f.files[0],o.o==='P'?480:800,o.o==='P'?800:480);await upload('img',id,bin);toast('Uploaded');drawImages(box);}catch(e){toast(e.message);}}},'Upload')));}
function pContent(m){const C=CFG.content;
m.append(card('Pantry',h('p',{class:'mut'},'One item per line. Used by Chef.'),lines(C,'pantry')));
const wl=h('div',{class:'list'});C.wardrobe.forEach((it,i)=>wl.append(h('div',{class:'it'},h('span',{class:'t'},h('b',{},it.name),h('div',{class:'mut'},it.cat+' · warmth '+it.warm+' · formality '+it.form+(it.rain?' · rain-ok':'')+(it.color?' · '+it.color:''))),
h('button',{class:'s',onclick:()=>editItem(it)},'Edit'),h('button',{class:'s',onclick:()=>pickThumb(it)},'Photo'),h('button',{class:'s d',onclick:()=>{C.wardrobe.splice(i,1);touch();render();}},'✕'))));
m.append(card('Wardrobe',h('p',{class:'mut'},'Used by the Outfit suggestion. Photos become 64×64 thumbnails (upload after saving the item).'),wl,h('button',{class:'p',onclick:()=>editItem({id:uid('c'),name:'',cat:'top',warm:2,rain:false,form:2,color:''},true)},'+ Item')));
const lb=h('div');m.append(card('Lists',lb));drawLists(lb);
const nl=h('div');C.notes.forEach((n,i)=>nl.append(h('div',{class:'card'},h('div',{class:'row'},fld('Title',inp(n,'name')),h('button',{class:'s d k',onclick:()=>{C.notes.splice(i,1);touch();render();}},'✕')),fld('Text',area(n,'text')))));
m.append(card('Notes',nl,h('button',{onclick:()=>{C.notes.push({id:uid('n'),name:'Note',text:''});touch();render();}},'+ Note')));
m.append(card('Quotes',chk(C,'builtinQuotes','Include built-in public-domain quotes'),fld('My quotes (one per line, "Quote - Author")',lines(C,'quotes'))));
const ib=h('div');m.append(card('Images',h('p',{class:'mut'},'Photos are resized and dithered to 4 gray levels in your browser before upload. Used by Gallery and the Image widget.'),ib));drawImages(ib);}

/* ---------------- Connections ---------------- */
function pConn(m){const wl=h('div');const drawW=()=>{wl.innerHTML='';CFG.wifi.forEach((w,i)=>wl.append(h('div',{class:'row'},fld('Network '+(i+1),inp(w,'ssid')),secretInput('wifi'+i+'p','Password'),h('button',{class:'s d k',onclick:()=>{CFG.wifi.splice(i,1);touch();drawW();}},'✕'))));
if(CFG.wifi.length<3)wl.append(h('button',{class:'s',onclick:()=>{CFG.wifi.push({ssid:''});touch();drawW();}},'+ Network'));};drawW();
const sb=h('div',{class:'chips'});
m.append(card('Wi-Fi',h('p',{class:'mut'},'Up to 3 networks. Network names save with the Save bar; passwords save instantly.'),wl,h('div',{class:'row',style:'margin-top:8px'},h('button',{class:'k',onclick:async()=>{sb.textContent='Scanning...';try{const r=await api('/api/wifi/scan');sb.innerHTML='';
for(const n of r)sb.append(h('span',{class:'chip',onclick:()=>{if(CFG.wifi.length>=3)return toast('Max 3 networks');CFG.wifi.push({ssid:n.ssid});touch();drawW();}},n.ssid+' ('+n.rssi+')'));}catch(e){sb.textContent=e.message;}}},'Scan')),sb));
const L=CFG.loc;const q={q:''};const res=h('div',{class:'chips'});
m.append(card('Location & time zone',h('div',{class:'row'},inp(q,'q','text',{placeholder:'Search city (this browser needs internet)'}),h('button',{class:'k',onclick:async()=>{res.textContent='...';try{const r=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=6&language=en&format=json&name='+encodeURIComponent(q.q))).json();res.innerHTML='';
for(const c of (r.results||[]))res.append(h('span',{class:'chip',onclick:()=>{L.lat=+c.latitude.toFixed(4);L.lon=+c.longitude.toFixed(4);L.city=c.name+(c.country_code?', '+c.country_code:'');const p=tzPosix(c.timezone);if(p)L.tz=p;else toast('Set the POSIX time zone manually for '+c.timezone);touch();render();}},c.name+', '+(c.admin1||'')+' '+(c.country_code||'')));
if(!res.innerHTML)res.textContent='No results';}catch(e){res.textContent='Search failed (offline?) - enter coordinates manually.';}}},'Search')),res,
h('div',{class:'row'},fld('City label',inp(L,'city')),fld('Latitude',inp(L,'lat','number',{step:'any'})),fld('Longitude',inp(L,'lon','number',{step:'any'}))),
h('div',{class:'row'},fld('Time zone (POSIX TZ string)',inp(L,'tz')),h('button',{class:'k',onclick:()=>{const z=Intl.DateTimeFormat().resolvedOptions().timeZone,p=tzPosix(z);if(p){L.tz=p;touch();render();toast(z);}else toast('Unknown zone '+z+' - enter the POSIX TZ manually');}},"Use this browser's zone"))));
const P=CFG.profile;P.wake=P.wake||[];
m.append(card('Profile',h('div',{class:'row'},fld('Your name',inp(P,'name')),fld('Units',sel(P,'units',[['C','°C, km/h'],['F','°F, mph']])),chk(P,'h24','24-hour clock')),h('h3',{},'Wake times (sleep calculator, Wake Up automation)'),
h('div',{class:'row'},...DAYS.map((d,i)=>{const e=h('input',{type:'time'});e.value=P.wake[i]||'07:00';e.oninput=()=>{P.wake[i]=e.value;touch();};return fld(d,e);}))));
CFG.cals=CFG.cals||[];for(let i=0;i<3;i++)CFG.cals[i]=CFG.cals[i]||{name:''};
m.append(card('Calendars',h('p',{class:'mut'},'Paste a secret iCal (ICS) address - Google Calendar: Settings > your calendar > "Secret address in iCal format". Outlook: Publish calendar > ICS. Stored as a secret.'),
...[0,1,2].map(i=>h('div',{class:'row'},fld('Name',inp(CFG.cals[i],'name')),secretInput('ics'+i,'ICS URL '+(i+1),'https://.../basic.ics')))));
const fl=h('div');CFG.feeds.forEach((f,i)=>fl.append(h('div',{class:'row'},fld('Name',inp(f,'name')),fld('RSS / Atom URL',inp(f,'url')),h('button',{class:'s d k',onclick:()=>{CFG.feeds.splice(i,1);touch();render();}},'✕'))));
m.append(card('News feeds',fl,h('button',{class:'s',onclick:()=>{CFG.feeds.push({name:'Feed',url:''});touch();render();}},'+ Feed')));
m.append(card('Stocks',h('p',{class:'mut'},'Free API key from twelvedata.com (800 requests/day).'),secretInput('stk_key','Twelve Data API key')));
const A=CFG.ai;m.append(card('AI (optional)',h('p',{class:'mut'},'Any OpenAI-compatible endpoint. Default Groq (free tier, key from console.groq.com). Gemini: https://generativelanguage.googleapis.com/v1beta/openai. Local Ollama: http://<pc-ip>:11434/v1 (no key).'),
h('div',{class:'row'},fld('Base URL',inp(A,'base')),fld('Model',inp(A,'model'))),secretInput('ai_key','API key')));
const N=CFG.notify;
m.append(card('Notifications',chk(N,'ntfy','ntfy push (free app)'),h('div',{class:'row'},fld('ntfy server',inp(N,'ntfyServer')),h('button',{class:'k',onclick:async()=>{const r=await act('ntfyTopic');if(r&&r.topic){SEC.ntfy_topic=true;
modal('Subscribe in the ntfy app',h('div',{},h('p',{},'Your private topic (keep it secret):'),h('pre',{style:'white-space:pre-wrap'},r.topic),h('p',{class:'mut'},'Install ntfy on your phone, tap +, enter the topic and server '+N.ntfyServer+'.')));}}},'Generate topic')),
secretInput('ntfy_topic','Topic (or paste your own)'),secretInput('ntfy_tok','Access token (optional, self-hosted)'),chk(N,'discord','Discord webhook'),secretInput('discord','Discord webhook URL'),
chk(N,'telegram','Telegram bot'),h('div',{class:'row'},secretInput('tg_tok','Bot token'),fld('Chat id',inp(N,'tgChat'))),chk(N,'webhook','Generic webhook (POST JSON)'),secretInput('webhook','Webhook URL'),h('button',{class:'s',onclick:()=>act('notifyTest')},'Send test (save first)')));
const ol=h('div');CFG.owners.forEach((o,i)=>ol.append(h('div',{class:'row'},fld('Name',inp(o,'name')),fld('Type',sel(o,'type',[['mac','Fixed address (BLE tag / watch)'],['irk','Phone (IRK, 32 hex)']],()=>render())),fld(o.type==='irk'?'IRK':'Address',inp(o,'v')),h('button',{class:'s d k',onclick:()=>{CFG.owners.splice(i,1);touch();render();}},'✕'))));
const bl=h('div',{class:'list'});
m.append(card('Owner devices',h('p',{class:'mut'},'Used by Guardian and "owner home/away". Phones use rotating private addresses - add the phone\'s IRK (identity resolving key). BLE tags/watches with a fixed address can be added from a scan. These values are excluded from backups.'),ol,
h('div',{class:'row'},h('button',{class:'k',onclick:()=>{CFG.owners.push({name:'My phone',type:'irk',v:''});touch();render();}},'+ Add manually'),h('button',{class:'k',onclick:async()=>{bl.textContent='Scanning 4 s...';try{const r=await api('/api/ble/scan');bl.innerHTML='';
for(const d of r)bl.append(h('div',{class:'it'},h('span',{class:'t'},(d.name||'(no name)')+' - '+d.addr),h('span',{class:'mut'},d.rssi+' dBm'),h('button',{class:'s',onclick:()=>{CFG.owners.push({name:d.name||'BLE device',type:'mac',v:d.addr});touch();render();}},'Add')));if(!r.length)bl.textContent='Nothing found';}catch(e){bl.textContent=e.message;}}},'Scan nearby')),bl));}

/* ---------------- System ---------------- */
function pSystem(m){const S=CFG.sys,SD=CFG.sound,T=CFG.touch;
m.append(card('Power',fld('Power profile',sel(S,'profile',[['eco','Eco - longest battery'],['balanced','Balanced'],['live','Live - most up to date']])),chk(S,'chargeLimit','Stop charging at 80% when docked'),
h('p',{class:'mut'},ST?('Battery '+ST.soc+'% · '+ST.mv+' mV · '+ST.ma+' mA (avg '+ST.avgMa+')'+(ST.tte>0&&ST.tte<65535?' · ~'+Math.round(ST.tte/60)+' h left':'')+' · wakes '+ST.stats.wakes+' · refreshes '+ST.stats.refreshes+' ('+ST.stats.full+' full)'):'')));
m.append(card('Display',chk(S,'partialAfterSleep','Fast partial refresh after sleep'),fld('Full clean refresh every N partial refreshes',inp(S,'fullEvery','number')),chk(S,'grayswap','Swap the two gray levels (if photos look wrong)'),chk(S,'showName','Show Moment name in the status widget')));
m.append(card('Sound',fld('Volume',inp(SD,'vol','range',{min:0,max:100})),chk(SD,'quiet','Quiet hours (no automation sounds)'),h('div',{class:'row'},fld('From',inp(SD,'quietStart','time')),fld('To',inp(SD,'quietEnd','time'))),
h('div',{class:'row'},...['chime','double','alert','wake'].map(p=>h('button',{class:'s k',onclick:()=>act('sound',{preset:p})},p)))));
m.append(card('Sensors & poses',h('div',{class:'row'},fld('Temperature offset (°C)',inp(S,'tempOff','number',{step:0.1})),fld('Humidity offset (%)',inp(S,'humOff','number',{step:1}))),
h('div',{class:'row'},fld('Vertical: max tilt °',inp(S,'vertMax','number')),fld('Stand: min tilt °',inp(S,'standMin','number')),fld('Stand: max tilt °',inp(S,'standMax','number'))),
h('div',{class:'row'},fld('Stillness threshold (°/s)',inp(S,'still','number',{step:0.5})),fld('Motion wake threshold (1 = most sensitive)',inp(S,'motionThs','number',{min:1,max:63})))));
m.append(card('Touch mapping',h('p',{class:'mut'},'Only change if taps land in the wrong place. Applies after Save.'),chk(T,'swap','Swap axes'),chk(T,'ix','Invert X'),chk(T,'iy','Invert Y')));
m.append(card('Security',chk(S,'insecureTls','Allow HTTPS without certificate check (only for self-hosted servers)'),h('button',{class:'k',onclick:()=>{if(confirm('Log out all browsers?'))act('logoutAll');}},'Log out all sessions')));
const fw=h('input',{type:'file',accept:'.bin'});
m.append(card('Firmware',h('p',{class:'mut'},'Version '+(ST?ST.ver:'')+'. Upload a Moments app .bin. Settings are kept.'),h('div',{class:'row'},fw,h('button',{class:'p k',onclick:async()=>{if(!fw.files[0])return toast('Choose a .bin');const fd=new FormData();fd.append('fw',fw.files[0]);toast('Uploading...');
try{const r=await fetch('/api/ota',{method:'POST',headers:{'X-Token':TOK},body:fd});toast(r.ok?'Updated - rebooting':'Update failed');}catch(e){toast(e.message);}}},'Update'))));
const rf=h('input',{type:'file',accept:'.json'});
m.append(card('Backup & restore',h('div',{class:'row'},h('button',{class:'k',onclick:async()=>{try{const r=await api('/api/backup');const b=new Blob([JSON.stringify(r,null,1)],{type:'application/json'});const a=h('a',{href:URL.createObjectURL(b),download:'moments-config.json'});document.body.append(a);a.click();a.remove();}catch(e){toast(e.message);}}},'Download backup'),rf,
h('button',{class:'k',onclick:async()=>{if(!rf.files[0])return;try{const j=JSON.parse(await rf.files[0].text());const owners=CFG.owners;CFG=j;if((j.owners||[]).some(o=>!o.v))CFG.owners=owners;await saveCfg();render();}catch(e){toast('Invalid file');}}},'Restore')),h('p',{class:'mut'},'Backups contain no passwords, keys or owner device addresses.')));
m.append(card('Device',h('div',{class:'row'},h('button',{class:'k',onclick:()=>act('exitConfig')},'Leave setup mode'),h('button',{class:'k',onclick:()=>{if(confirm('Reboot now?'))act('reboot');}},'Reboot'),h('button',{class:'d k',onclick:()=>{if(confirm('Erase ALL settings, data and secrets?'))act('factoryReset');}},'Factory reset')),
h('p',{class:'mut'},ST?('Heap '+ST.stats.heap+' · PSRAM '+ST.stats.psram+' · storage '+Math.round(ST.stats.fsUsed/1024)+' / '+Math.round(ST.stats.fsTotal/1024)+' KB · uptime '+ST.stats.uptime+' s · display '+(ST.epdOk?'ok':'ERROR')):'')));}

if(!TOK)showLogin();else boot();
</script></body></html>)HTML";
