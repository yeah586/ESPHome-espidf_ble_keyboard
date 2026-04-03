#include "esphome/core/defines.h"
#ifdef USE_BLE_KEYBOARD_WEB_CONTROL

#include "web_control.h"
#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <span>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *const TAG = "ble_kb_web";

// ── Embedded HTML/JS page ──────────────────────────────────────────
// Minified single-page app with keyboard + mouse control
static const char PAGE_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>BLE Keyboard &amp; Mouse</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#1a1e28;--fg:#e2e8f0;--card:#13161e;--border:#252a38;--muted:#6b7a99;--accent:#00d4aa;--active:#03a9f4;--caps:#ff9800;--name:#8892a8}
body.light{--bg:#f0f2f5;--fg:#1a1e28;--card:#ffffff;--border:#d0d5dd;--muted:#6b7a99;--accent:#00875a;--active:#0288d1;--caps:#e65100;--name:#555}
body{background:var(--bg);color:var(--fg);font-family:-apple-system,BlinkMacSystemFont,sans-serif;padding:12px;max-width:680px;margin:0 auto;user-select:none;-webkit-user-select:none;transition:background .2s,color .2s}
h2{font-size:15px;font-weight:600;margin:12px 0 8px;color:var(--accent);display:flex;align-items:center;gap:6px}
h2 svg{width:18px;height:18px;fill:var(--accent)}
.toolbar{display:flex;align-items:center;justify-content:space-between;padding:8px 10px;margin-bottom:10px;background:var(--card);border:1px solid var(--border);border-radius:10px}
.toolbar-left{display:flex;align-items:center;gap:8px}
.status-dot{width:10px;height:10px;border-radius:50%;background:var(--muted);transition:background .3s}
.status-dot.connected{background:var(--accent)}
.status-dot.paired{background:var(--active)}
.status-text{font-size:12px;color:var(--muted)}
.status-text.on{color:var(--accent)}
.dev-name{font-size:11px;color:var(--name);margin-left:4px;font-weight:500}
.toolbar-right{display:flex;align-items:center;gap:6px}
.zoom-controls{display:flex;align-items:center;gap:4px}
.zoom-btn,.theme-btn{width:30px;height:30px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--fg);font-size:16px;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center;touch-action:manipulation}
.zoom-btn:active,.theme-btn:active{background:var(--active);color:#fff}
.theme-btn{font-size:14px}
.zoom-label{font-size:11px;color:var(--muted);min-width:36px;text-align:center}
.card{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:10px;margin-bottom:12px}
.scalable{transform-origin:top center;transition:transform .15s}
.row{display:flex;gap:3px;margin-bottom:3px}
.row:last-child{margin-bottom:0}
.k{flex:1;min-width:0;padding:9px 1px;border:1px solid var(--border);border-radius:5px;background:var(--bg);color:var(--fg);font-size:12px;font-weight:500;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .08s}
.k:active,.k.p{background:var(--active);color:#fff;border-color:var(--active)}
.k.active{background:var(--active);color:#fff;border-color:var(--active)}
.k.caps{background:var(--caps);color:#fff;border-color:var(--caps)}
.k.fk{font-size:10px;padding:6px 1px}
.touchpad{width:100%;aspect-ratio:16/9;background:var(--bg);border-radius:10px;border:2px solid var(--border);cursor:crosshair;touch-action:none;position:relative;overflow:hidden;transition:border-color .15s}
.touchpad.active{border-color:var(--active)}
.touchpad-hint{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);color:var(--muted);font-size:12px;pointer-events:none;opacity:.5}
.touchpad.active .touchpad-hint{opacity:0}
.mbtn-row{display:grid;grid-template-columns:1fr .7fr 1fr;gap:6px;margin-top:8px}
.mbtn{padding:12px 0;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:12px;font-weight:500;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .1s}
.mbtn:active,.mbtn.p{background:var(--active);color:#fff;border-color:var(--active)}
.scroll-row{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:6px}
.sbtn{padding:8px 0;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:16px;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .1s}
.sbtn:active{background:var(--active);color:#fff;border-color:var(--active)}
.prog-btns{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
.prog-btn{padding:8px 14px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:12px;font-weight:500;cursor:pointer;touch-action:manipulation;transition:background .1s}
.prog-btn:active,.prog-btn.p{background:var(--active);color:#fff;border-color:var(--active)}
.prog-empty{font-size:12px;color:var(--muted);padding:4px 0}
.host-bar{display:flex;gap:6px;padding:8px 10px;margin-bottom:10px;background:var(--card);border:1px solid var(--border);border-radius:10px}
.host-btn{flex:1;padding:8px 4px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:11px;font-weight:500;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .15s}
.host-btn.active{background:var(--active);color:#fff;border-color:var(--active)}
.host-btn.occupied{border-color:var(--accent)}
.host-btn .slot-label{font-size:10px;color:var(--muted);display:block}
.host-btn.active .slot-label{color:rgba(255,255,255,.7)}
.forget-btn{padding:8px 12px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:#c44;font-size:11px;font-weight:600;cursor:pointer;touch-action:manipulation;transition:background .15s;white-space:nowrap}
.forget-btn:hover{background:#c44;color:#fff}
.section-toggles{display:flex;gap:4px;align-items:center}
.toggle-btn{padding:4px 8px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--muted);font-size:10px;font-weight:500;cursor:pointer;touch-action:manipulation;transition:background .15s,color .15s}
.toggle-btn.on{background:var(--active);color:#fff;border-color:var(--active)}
.media-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-top:6px}
.media-btn{padding:14px 4px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:11px;font-weight:500;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .1s;display:flex;flex-direction:column;align-items:center;gap:4px}
.media-btn svg{width:20px;height:20px;fill:var(--fg)}
.media-btn:active,.media-btn.p{background:var(--active);color:#fff;border-color:var(--active)}
.media-btn:active svg,.media-btn.p svg{fill:#fff}
</style></head><body>

<div id="scalable" class="scalable">
<div class="toolbar">
<div class="toolbar-left">
<div class="status-dot" id="sdot"></div>
<span class="status-text" id="stxt">Disconnected</span>
<span class="dev-name" id="dname"></span>
</div>
<div class="toolbar-right">
<div class="section-toggles" id="toggle-bar">
<button class="toggle-btn on" data-section="keyboard">Keyboard</button>
<button class="toggle-btn on" data-section="mouse-card">Mouse</button>
<button class="toggle-btn on" data-section="media-card">Remote</button>
<button class="toggle-btn on" data-section="btns-card">Buttons</button>
</div>
<div class="zoom-controls">
<button class="zoom-btn" id="zout">-</button>
<span class="zoom-label" id="zlbl">100%</span>
<button class="zoom-btn" id="zin">+</button>
</div>
<button class="theme-btn" id="thm" title="Toggle light/dark">&#9788;</button>
</div>
</div>

<div class="host-bar" id="host-bar" style="display:none"></div>
<div class="card" id="keyboard"></div>

<div class="card" id="mouse-card">
<h2><svg viewBox="0 0 24 24"><path d="M12 2C8.14 2 5 5.14 5 9v6c0 3.86 3.14 7 7 7s7-3.14 7-7V9c0-3.86-3.14-7-7-7zm0 2c2.76 0 5 2.24 5 5v2h-4V5h-2v6H7V9c0-2.76 2.24-5 5-5z"/></svg>Mouse</h2>
<div class="touchpad" id="touchpad"><span class="touchpad-hint">Drag to move</span></div>
<div class="mbtn-row">
<button class="mbtn" id="ml">Left</button>
<button class="mbtn" id="mm">Middle</button>
<button class="mbtn" id="mr">Right</button>
</div>
<div class="scroll-row">
<button class="sbtn" id="su">&#9650; Up</button>
<button class="sbtn" id="sd">&#9660; Down</button>
</div>
</div>

<div class="card" id="media-card">
<h2><svg viewBox="0 0 24 24"><path d="M21 3H3c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H3V5h18v14zM5 15h2v2H5v-2zm4 0h2v2H9v-2zm4 0h2v2h-2v-2z"/></svg>Remote</h2>
<div class="media-grid">
<button class="media-btn" data-action="consumer:0x0030" data-repeat="1"><svg viewBox="0 0 24 24"><path d="M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42C17.99 7.86 19 9.81 19 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.19 1.01-4.14 2.58-5.42L6.17 5.17C4.23 6.82 3 9.26 3 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.74-1.23-5.18-3.17-6.83z"/></svg>Power</button>
<button class="media-btn" data-action="consumer:0x0042" data-repeat="1"><svg viewBox="0 0 24 24"><path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/></svg>Up</button>
<button class="media-btn" data-action="consumer:0x0221"><svg viewBox="0 0 24 24"><path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg>Search</button>
<button class="media-btn" data-action="consumer:0x0044" data-repeat="1"><svg viewBox="0 0 24 24"><path d="M15.41 16.59L10.83 12l4.58-4.59L14 6l-6 6 6 6z"/></svg>Left</button>
<button class="media-btn" data-action="consumer:0x0041"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/></svg>Enter</button>
<button class="media-btn" data-action="consumer:0x0045" data-repeat="1"><svg viewBox="0 0 24 24"><path d="M8.59 16.59L13.17 12 8.59 7.41 10 6l6 6-6 6z"/></svg>Right</button>
<button class="media-btn" data-action="consumer:0x0224"><svg viewBox="0 0 24 24"><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg>Back</button>
<button class="media-btn" data-action="consumer:0x0043" data-repeat="1"><svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg>Down</button>
<button class="media-btn" data-action="volume_up" data-repeat="1"><svg viewBox="0 0 24 24"><path d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z"/></svg>Vol +</button>
<button class="media-btn" data-action="consumer:0x0223"><svg viewBox="0 0 24 24"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg>Home</button>
<button class="media-btn" data-action="mute"><svg viewBox="0 0 24 24"><path d="M16.5 12c0-1.77-1.02-3.29-2.5-4.03v2.21l2.45 2.45c.03-.2.05-.41.05-.63zm2.5 0c0 .94-.2 1.82-.54 2.64l1.51 1.51C20.63 14.91 21 13.5 21 12c0-4.28-2.99-7.86-7-8.77v2.06c2.89.86 5 3.54 5 6.71zM4.27 3L3 4.27 7.73 9H3v6h4l5 5v-6.73l4.25 4.25c-.67.52-1.42.93-2.25 1.18v2.06c1.38-.31 2.63-.95 3.69-1.81L19.73 21 21 19.73l-9-9L4.27 3zM12 4L9.91 6.09 12 8.18V4z"/></svg>Mute</button>
<button class="media-btn" data-action="volume_down" data-repeat="1"><svg viewBox="0 0 24 24"><path d="M18.5 12c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM5 9v6h4l5 5V4L9 9H5z"/></svg>Vol -</button>
</div>
</div>

<div class="card" id="btns-card" style="display:none">
<h2><svg viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H5V5h14v14zM7 12h2v2H7v-2zm0-4h2v2H7V8zm4 4h2v2h-2v-2zm0-4h2v2h-2V8zm4 4h2v2h-2v-2zm0-4h2v2h-2V8z"/></svg>Buttons</h2>
<div class="prog-btns" id="prog-btns"><span class="prog-empty">Loading...</span></div>
</div>
</div>

<script>
// API helper
function api(endpoint,params){
  const url='/api/ble_keyboard/'+endpoint+'?'+new URLSearchParams(params);
  fetch(url,{method:'POST'}).catch(()=>{});
}

// ── Theme ──
const thmBtn=document.getElementById('thm');
if(localStorage.getItem('blekb_theme')==='light')document.body.classList.add('light');
function toggleTheme(){
  document.body.classList.toggle('light');
  localStorage.setItem('blekb_theme',document.body.classList.contains('light')?'light':'dark');
}
thmBtn.addEventListener('click',toggleTheme);

// ── Zoom ──
let zoom=100;
const scalable=document.getElementById('scalable');
const zlbl=document.getElementById('zlbl');
function setZoom(v){zoom=Math.max(50,Math.min(200,v));scalable.style.transform='scale('+(zoom/100)+')';zlbl.textContent=zoom+'%'}
document.getElementById('zin').addEventListener('click',()=>setZoom(zoom+10));
document.getElementById('zout').addEventListener('click',()=>setZoom(zoom-10));

// ── Status polling ──
const sdot=document.getElementById('sdot');
const stxt=document.getElementById('stxt');
const dname=document.getElementById('dname');
function pollStatus(){
  fetch('/api/ble_keyboard/status').then(r=>r.json()).then(d=>{
    const c=d.connected,p=d.paired;
    sdot.className='status-dot'+(p?' paired':c?' connected':'');
    stxt.className='status-text'+((c||p)?' on':'');
    stxt.textContent=p?'Paired':c?'Connected':'Disconnected';
    if(d.device_name){dname.textContent='('+d.device_name+')';document.title=d.device_name+' — BLE Control'}
  }).catch(()=>{
    sdot.className='status-dot';
    stxt.className='status-text';
    stxt.textContent='Offline';
  });
}
pollStatus();
setInterval(pollStatus,3000);

// ── Host Switcher ──
(function(){
  const bar=document.getElementById('host-bar');
  let lastHostsRaw=null;
  function loadHosts(){
    fetch('/api/ble_keyboard/hosts').then(r=>r.text()).then(txt=>{
      if(txt===lastHostsRaw)return;
      lastHostsRaw=txt;
      const d=JSON.parse(txt);
      if(!d.slots||d.slots.length<=1){bar.style.display='none';return}
      bar.style.display='';
      bar.innerHTML='';
      d.slots.forEach(s=>{
        const b=document.createElement('button');
        b.className='host-btn'+(s.slot===d.active?' active':'')+(s.occupied?' occupied':'');
        b.innerHTML='<span class="slot-label">'+(s.name||('Host '+(s.slot+1)))+'</span>'+(s.occupied?s.addr:'Empty');
        b.addEventListener('pointerdown',e=>{
          e.preventDefault();
          api('switch_host',{slot:s.slot});
          bar.querySelectorAll('.host-btn').forEach(x=>x.classList.remove('active'));
          b.classList.add('active');
        });
        bar.appendChild(b);
      });
      const fb=document.createElement('button');
      fb.className='forget-btn';
      fb.textContent='Forget Host';
      fb.addEventListener('pointerdown',e=>{
        e.preventDefault();
        const active=d.active;
        if(confirm('Forget host in slot '+(active+1)+'?')){
          api('forget_host',{slot:active});
          setTimeout(loadHosts,1000);
        }
      });
      bar.appendChild(fb);
    }).catch(()=>{bar.style.display='none'});
  }
  loadHosts();
  setInterval(loadHosts,5000);
})();

// ── Programmed Buttons ──
(function(){
  const card=document.getElementById('btns-card');
  const container=document.getElementById('prog-btns');
  fetch('/api/ble_keyboard/buttons').then(r=>r.json()).then(btns=>{
    if(!btns.length){card.style.display='none';return}
    card.style.display='';
    container.innerHTML='';
    btns.forEach(b=>{
      const el=document.createElement('button');
      el.className='prog-btn';
      el.textContent=b.name;
      el.addEventListener('pointerdown',e=>{
        e.preventDefault();el.classList.add('p');
        api('press',{action:b.action});
        setTimeout(()=>el.classList.remove('p'),150);
      });
      container.appendChild(el);
    });
  }).catch(()=>{card.style.display='none'});
})();

// ── Keyboard ──
const CK={a:0x04,b:0x05,c:0x06,d:0x07,e:0x08,f:0x09,g:0x0A,h:0x0B,i:0x0C,j:0x0D,k:0x0E,l:0x0F,m:0x10,n:0x11,o:0x12,p:0x13,q:0x14,r:0x15,s:0x16,t:0x17,u:0x18,v:0x19,w:0x1A,x:0x1B,y:0x1C,z:0x1D,'1':0x1E,'2':0x1F,'3':0x20,'4':0x21,'5':0x22,'6':0x23,'7':0x24,'8':0x25,'9':0x26,'0':0x27,'`':0x35,'-':0x2D,'=':0x2E,'[':0x2F,']':0x30,'\\':0x31,';':0x33,"'":0x34,',':0x36,'.':0x37,'/':0x38,' ':0x2C};
const ROWS=[
[{l:'Esc',t:'s',kc:0x29,f:1.2},{l:'F1',t:'s',kc:0x3A},{l:'F2',t:'s',kc:0x3B},{l:'F3',t:'s',kc:0x3C},{l:'F4',t:'s',kc:0x3D},{l:'F5',t:'s',kc:0x3E},{l:'F6',t:'s',kc:0x3F},{l:'F7',t:'s',kc:0x40},{l:'F8',t:'s',kc:0x41},{l:'F9',t:'s',kc:0x42},{l:'F10',t:'s',kc:0x43},{l:'F11',t:'s',kc:0x44},{l:'F12',t:'s',kc:0x45}],
[{l:'`',sl:'~',t:'c',c:'`',sc:'~'},{l:'1',sl:'!',t:'c',c:'1',sc:'!'},{l:'2',sl:'@',t:'c',c:'2',sc:'@'},{l:'3',sl:'#',t:'c',c:'3',sc:'#'},{l:'4',sl:'$',t:'c',c:'4',sc:'$'},{l:'5',sl:'%',t:'c',c:'5',sc:'%'},{l:'6',sl:'^',t:'c',c:'6',sc:'^'},{l:'7',sl:'&',t:'c',c:'7',sc:'&'},{l:'8',sl:'*',t:'c',c:'8',sc:'*'},{l:'9',sl:'(',t:'c',c:'9',sc:'('},{l:'0',sl:')',t:'c',c:'0',sc:')'},{l:'-',sl:'_',t:'c',c:'-',sc:'_'},{l:'=',sl:'+',t:'c',c:'=',sc:'+'},{l:'Bksp',t:'s',kc:0x2A,f:1.5}],
[{l:'Tab',t:'s',kc:0x2B,f:1.3},{l:'q',sl:'Q',t:'c',c:'q',sc:'Q'},{l:'w',sl:'W',t:'c',c:'w',sc:'W'},{l:'e',sl:'E',t:'c',c:'e',sc:'E'},{l:'r',sl:'R',t:'c',c:'r',sc:'R'},{l:'t',sl:'T',t:'c',c:'t',sc:'T'},{l:'y',sl:'Y',t:'c',c:'y',sc:'Y'},{l:'u',sl:'U',t:'c',c:'u',sc:'U'},{l:'i',sl:'I',t:'c',c:'i',sc:'I'},{l:'o',sl:'O',t:'c',c:'o',sc:'O'},{l:'p',sl:'P',t:'c',c:'p',sc:'P'},{l:'[',sl:'{',t:'c',c:'[',sc:'{'},{l:']',sl:'}',t:'c',c:']',sc:'}'},{l:'\\',sl:'|',t:'c',c:'\\',sc:'|'}],
[{l:'Caps',t:'caps',kc:0x39,f:1.5},{l:'a',sl:'A',t:'c',c:'a',sc:'A'},{l:'s',sl:'S',t:'c',c:'s',sc:'S'},{l:'d',sl:'D',t:'c',c:'d',sc:'D'},{l:'f',sl:'F',t:'c',c:'f',sc:'F'},{l:'g',sl:'G',t:'c',c:'g',sc:'G'},{l:'h',sl:'H',t:'c',c:'h',sc:'H'},{l:'j',sl:'J',t:'c',c:'j',sc:'J'},{l:'k',sl:'K',t:'c',c:'k',sc:'K'},{l:'l',sl:'L',t:'c',c:'l',sc:'L'},{l:';',sl:':',t:'c',c:';',sc:':'},{l:"'",sl:'"',t:'c',c:"'",sc:'"'},{l:'Enter',t:'s',kc:0x28,f:1.8}],
[{l:'Shift',t:'m',mod:'shift',bit:0x02,f:2},{l:'z',sl:'Z',t:'c',c:'z',sc:'Z'},{l:'x',sl:'X',t:'c',c:'x',sc:'X'},{l:'c',sl:'C',t:'c',c:'c',sc:'C'},{l:'v',sl:'V',t:'c',c:'v',sc:'V'},{l:'b',sl:'B',t:'c',c:'b',sc:'B'},{l:'n',sl:'N',t:'c',c:'n',sc:'N'},{l:'m',sl:'M',t:'c',c:'m',sc:'M'},{l:',',sl:'<',t:'c',c:',',sc:'<'},{l:'.',sl:'>',t:'c',c:'.',sc:'>'},{l:'/',sl:'?',t:'c',c:'/',sc:'?'},{l:'Shift R',t:'m',mod:'rshift',bit:0x20,f:2}],
[{l:'Ctrl',t:'m',mod:'ctrl',bit:0x01,f:1.2},{l:'Win',t:'m',mod:'win',bit:0x08,f:1.2},{l:'Alt',t:'m',mod:'alt',bit:0x04,f:1.2},{l:'',t:'c',c:' ',sc:' ',f:6},{l:'Alt R',t:'m',mod:'altgr',bit:0x40,f:1.2},{l:'Del',t:'s',kc:0x4C,f:1.2},{l:'\u2190',t:'s',kc:0x50},{l:'\u2191',t:'s',kc:0x52},{l:'\u2193',t:'s',kc:0x51},{l:'\u2192',t:'s',kc:0x4F}]
];

let shift=false,capsLock=false,ctrl=false,alt=false,win=false,rshift=false,altgr=false;
const modBtns={shift:[],ctrl:[],alt:[],win:[],rshift:[],altgr:[]};
const charKeys=[];
let capsBtn=null;

function buildKeyboard(){
  const kb=document.getElementById('keyboard');
  ROWS.forEach((row,ri)=>{
    const rd=document.createElement('div');
    rd.className='row';
    row.forEach((k,ki)=>{
      const b=document.createElement('button');
      b.className='k';
      if(ri===0)b.classList.add('fk');
      if(k.f)b.style.flex=k.f;
      b.textContent=k.c===' '?'Space':k.l;
      b.dataset.r=ri;b.dataset.k=ki;
      if(k.t==='c'&&k.sl)charKeys.push({btn:b,def:k});
      if(k.t==='m'&&modBtns[k.mod])modBtns[k.mod].push(b);
      if(k.t==='caps')capsBtn=b;
      rd.appendChild(b);
    });
    kb.appendChild(rd);
  });
  kb.addEventListener('pointerdown',e=>{
    const b=e.target.closest('.k');if(!b)return;
    e.preventDefault();
    b.classList.add('p');setTimeout(()=>b.classList.remove('p'),120);
    const k=ROWS[+b.dataset.r][+b.dataset.k];
    onKey(k);
  });
}

function toggleMod(mod){
  if(mod==='shift'){shift=!shift}else if(mod==='ctrl'){ctrl=!ctrl}else if(mod==='alt'){alt=!alt}else if(mod==='win'){win=!win}else if(mod==='rshift'){rshift=!rshift}else if(mod==='altgr'){altgr=!altgr}
  const active=mod==='shift'?shift:mod==='ctrl'?ctrl:mod==='alt'?alt:mod==='win'?win:mod==='rshift'?rshift:altgr;
  modBtns[mod].forEach(b=>b.classList.toggle('active',active));
  if(mod==='shift'||mod==='rshift')updateLabels();
}

function updateLabels(){
  const sh=(shift||rshift)!==capsLock;
  charKeys.forEach(({btn,def})=>{
    const isLetter=def.c>='a'&&def.c<='z';
    btn.textContent=isLetter?(sh?def.sl:def.l):((shift||rshift)?def.sl:def.l);
  });
}

function onKey(k){
  if(k.t==='m'){toggleMod(k.mod);return}
  if(k.t==='caps'){
    capsLock=!capsLock;
    if(capsBtn)capsBtn.classList.toggle('caps',capsLock);
    api('key',{modifier:0,keycode:0x39});
    updateLabels();return;
  }
  let mb=0;
  if(ctrl)mb|=0x01;if(alt)mb|=0x04;if(win)mb|=0x08;if(rshift)mb|=0x20;if(altgr)mb|=0x40;
  if(k.t==='c'){
    if(mb!==0){
      if(shift)mb|=0x02;
      const code=CK[k.c];
      if(code!==undefined)api('key',{modifier:mb,keycode:code});
    }else{
      const isL=k.c>='a'&&k.c<='z';
      const sh=isL?((shift||rshift)!==capsLock):(shift||rshift);
      const ch=sh?k.sc:k.c;
      api('string',{keys:ch});
    }
  }else if(k.t==='s'){
    if(shift)mb|=0x02;
    api('key',{modifier:mb,keycode:k.kc});
  }
  if(shift)toggleMod('shift');
  if(ctrl)toggleMod('ctrl');
  if(alt)toggleMod('alt');
  if(win)toggleMod('win');
  if(rshift)toggleMod('rshift');
  if(altgr)toggleMod('altgr');
}

buildKeyboard();

// ── Mouse ──
(function(){
  const pad=document.getElementById('touchpad');
  let tracking=false,lastX=0,lastY=0,lastTime=0,startTime=0,moved=false,startSX=0,startSY=0;
  let accumX=0,accumY=0;
  const baseSens=1.0,accelFactor=0.15,maxSens=4.0,scrollSens=2,tapDeadZone=5;

  function onStart(x,y){tracking=true;lastX=startSX=x;lastY=startSY=y;lastTime=startTime=Date.now();moved=false;accumX=0;accumY=0;pad.classList.add('active')}
  function onMove(x,y){
    if(!tracking)return;
    if(!moved){const td=Math.abs(x-startSX)+Math.abs(y-startSY);if(td<tapDeadZone)return}
    const now=Date.now(),dt=Math.max(now-lastTime,1);
    const rawDx=x-lastX,rawDy=y-lastY;
    const dist=Math.sqrt(rawDx*rawDx+rawDy*rawDy);
    const speed=dist/dt;
    const sens=Math.min(baseSens+speed*accelFactor,maxSens);
    accumX+=rawDx*sens;accumY+=rawDy*sens;
    const dx=Math.trunc(accumX),dy=Math.trunc(accumY);
    if(dx!==0||dy!==0){
      const cx=Math.max(-127,Math.min(127,dx)),cy=Math.max(-127,Math.min(127,dy));
      api('mouse_move',{x:cx,y:cy});
      accumX-=dx;accumY-=dy;moved=true;
    }
    lastX=x;lastY=y;lastTime=now;
  }
  function onEnd(){
    if(!tracking)return;tracking=false;pad.classList.remove('active');
    if(!moved&&Date.now()-startTime<250)api('mouse_click',{btn:1});
  }

  pad.addEventListener('mousedown',e=>{e.preventDefault();onStart(e.clientX,e.clientY)});
  window.addEventListener('mousemove',e=>onMove(e.clientX,e.clientY));
  window.addEventListener('mouseup',()=>onEnd());
  pad.addEventListener('touchstart',e=>{e.preventDefault();const t=e.touches[0];onStart(t.clientX,t.clientY)},{passive:false});
  pad.addEventListener('touchmove',e=>{e.preventDefault();const t=e.touches[0];onMove(t.clientX,t.clientY)},{passive:false});
  pad.addEventListener('touchend',e=>{e.preventDefault();if(e.touches.length===0)onEnd()},{passive:false});

  let wheelAccum=0;
  pad.addEventListener('wheel',e=>{
    e.preventDefault();
    wheelAccum+=-e.deltaY*scrollSens*0.02;
    const s=Math.trunc(wheelAccum);
    if(s!==0){api('mouse_scroll',{amount:Math.max(-127,Math.min(127,s))});wheelAccum-=s}
  },{passive:false});

  // Mouse buttons
  const btnMap={ml:1,mm:4,mr:2};
  for(const[id,btn]of Object.entries(btnMap)){
    const el=document.getElementById(id);
    el.addEventListener('pointerdown',e=>{e.preventDefault();el.classList.add('p');api('mouse_click',{btn:btn})});
    el.addEventListener('pointerup',()=>el.classList.remove('p'));
    el.addEventListener('pointerleave',()=>el.classList.remove('p'));
  }

  // Scroll buttons
  let si=null;
  function startScroll(a){api('mouse_scroll',{amount:a});si=setInterval(()=>api('mouse_scroll',{amount:a}),150)}
  function stopScroll(){if(si){clearInterval(si);si=null}}
  for(const[id,a]of[['su',3],['sd',-3]]){
    const el=document.getElementById(id);
    el.addEventListener('pointerdown',e=>{e.preventDefault();startScroll(a)});
    el.addEventListener('pointerup',stopScroll);
    el.addEventListener('pointerleave',stopScroll);
  }
})();

// ── Media Controls ──
(function(){
  const grid=document.querySelector('.media-grid');
  if(!grid)return;
  let ri=null;
  function stopRepeat(){if(ri){clearInterval(ri);ri=null}}
  grid.addEventListener('pointerdown',e=>{
    const b=e.target.closest('.media-btn');if(!b)return;
    e.preventDefault();b.classList.add('p');
    const action=b.dataset.action;
    api('press',{action:action});
    if(b.dataset.repeat){ri=setInterval(()=>api('press',{action:action}),200)}
  });
  grid.addEventListener('pointerup',e=>{
    const b=e.target.closest('.media-btn');if(b)b.classList.remove('p');stopRepeat();
  });
  grid.addEventListener('pointerleave',e=>{
    const b=e.target.closest('.media-btn');if(b)b.classList.remove('p');stopRepeat();
  },true);
})();

// ── Section Toggles ──
(function(){
  const bar=document.getElementById('toggle-bar');
  const KEY='blekb_sections';
  let state={};
  try{state=JSON.parse(localStorage.getItem(KEY))||{}}catch(e){}
  bar.querySelectorAll('.toggle-btn').forEach(btn=>{
    const id=btn.dataset.section;
    if(state[id]===false){btn.classList.remove('on');const el=document.getElementById(id);if(el)el.style.display='none'}
    btn.addEventListener('click',()=>{
      const on=btn.classList.toggle('on');
      const el=document.getElementById(id);
      if(el)el.style.display=on?'':'none';
      state[id]=on;
      localStorage.setItem(KEY,JSON.stringify(state));
    });
  });
})();
</script></body></html>)rawhtml";


// ── Internal handler class ─────────────────────────────────────────
// Inherits from the platform-specific AsyncWebHandler via web_server_base

class BleKbWebHandler : public AsyncWebHandler {
 public:
  BleKbWebHandler(EspidfBleKeyboard *kb) : kb_(kb) {}

  static std::string get_url(AsyncWebServerRequest *request) {
    char buf[513];
    std::span<char, 513> span(buf);
    auto ref = request->url_to(span);
    return std::string(ref.begin(), ref.end());
  }

  bool canHandle(AsyncWebServerRequest *request) const override {
    std::string url = get_url(request);
    return url == "/ble_keyboard" ||
           url.rfind("/api/ble_keyboard/", 0) == 0;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
      std::string url = get_url(request);

      auto send_response = [request](int code, const char* type, const char* content) {
        AsyncWebServerResponse* response = request->beginResponse(code, type, content);
        response->addHeader("Connection", "close");
        request->send(response);
      };

    // Serve the page
    if (url == "/ble_keyboard") {
      send_response(200, "text/html", PAGE_HTML);
      return;
    }

    std::string path = url.substr(strlen("/api/ble_keyboard/"));

    // GET-only endpoints (read state)
    if (path == "status") {
      std::string json = "{\"connected\":";
      json += kb_->is_connected() ? "true" : "false";
      json += ",\"paired\":";
      json += kb_->is_paired() ? "true" : "false";
      json += ",\"device_name\":\"";
      json += kb_->device_name();
      json += "\"}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "buttons") {
      // JSON-escape a string (handles \n, \r, \t, \, ")
      auto json_escape = [](const std::string &s) -> std::string {
        std::string out;
        out.reserve(s.size() + 4);
        for (char c : s) {
          switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
          }
        }
        return out;
      };
      std::string json = "[";
      const auto &btns = kb_->get_buttons();
      for (size_t i = 0; i < btns.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"";
        json += json_escape(btns[i].name);
        json += "\",\"action\":\"";
        json += json_escape(btns[i].action);
        json += "\"}";
      }
      json += "]";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "hosts") {
      // Build slot-to-name map from registered switch_host buttons
      std::map<uint8_t, std::string> slot_names;
      for (const auto &btn : kb_->get_buttons()) {
        if (btn.action.find("switch_host:") == 0) {
          int slot = -1;
          if (sscanf(btn.action.c_str(), "switch_host:%i", &slot) == 1 && slot >= 0) {
            slot_names[(uint8_t) slot] = btn.name;
          }
        }
      }
      std::string json = "{\"active\":";
      json += std::to_string(kb_->active_host_slot());
      json += ",\"slots\":[";
      for (uint8_t i = 0; i < kb_->host_slots(); i++) {
        if (i > 0) json += ",";
        const auto &h = kb_->get_host_slot(i);
        json += "{\"slot\":";
        json += std::to_string(i);
        json += ",\"occupied\":";
        json += h.occupied ? "true" : "false";
        if (h.occupied) {
          char addr_str[18];
          snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                   h.addr[0], h.addr[1], h.addr[2], h.addr[3], h.addr[4], h.addr[5]);
          json += ",\"addr\":\"";
          json += addr_str;
          json += "\"";
        }
        auto it = slot_names.find(i);
        if (it != slot_names.end()) {
          json += ",\"name\":\"";
          json += it->second;
          json += "\"";
        }
        json += "}";
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    // Remaining endpoints — POST only
    if (request->method() != HTTP_POST) {
      send_response(405, "text/plain", "POST only");
      return;
    }

    if (path == "mouse_move") {
      int x = request->hasArg("x") ? atoi(request->arg("x").c_str()) : 0;
      int y = request->hasArg("y") ? atoi(request->arg("y").c_str()) : 0;
      kb_->send_mouse_move((int8_t) x, (int8_t) y);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_click") {
      int btn = request->hasArg("btn") ? atoi(request->arg("btn").c_str()) : 1;
      kb_->send_mouse_click((uint8_t) btn);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_scroll") {
      int amount = request->hasArg("amount") ? atoi(request->arg("amount").c_str()) : 0;
      kb_->send_mouse_scroll((int8_t) amount);
      send_response(200, "text/plain", "OK");

    } else if (path == "string") {
      if (request->hasArg("keys")) {
        std::string keys = request->arg("keys").c_str();
        kb_->send_string(keys);
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "key") {
      int modifier = request->hasArg("modifier") ? atoi(request->arg("modifier").c_str()) : 0;
      int keycode = request->hasArg("keycode") ? atoi(request->arg("keycode").c_str()) : 0;
      kb_->send_key_combo((uint8_t) modifier, (uint8_t) keycode);
      send_response(200, "text/plain", "OK");

    } else if (path == "press") {
      // Press a button by action string
      if (request->hasArg("action")) {
        std::string action = request->arg("action").c_str();
        // Find and press the button by triggering its action directly
        // Reuse the same logic as press_action by calling the component methods
        if (action.find("combo:") == 0) {
          int mod = 0, key = 0;
          if (sscanf(action.c_str(), "combo:%i:%i", &mod, &key) == 2)
            kb_->send_key_combo((uint8_t) mod, (uint8_t) key);
        } else if (action.find("consumer:") == 0) {
          int usage = 0;
          if (sscanf(action.c_str(), "consumer:%i", &usage) == 1)
            kb_->send_consumer((uint16_t) usage);
        } else if (action == "ctrl_alt_del") kb_->send_ctrl_alt_del();
        else if (action == "sleep") kb_->send_sleep();
        else if (action == "shutdown") kb_->send_shutdown();
        else if (action == "hibernate") kb_->send_hibernate();
        else if (action == "power") kb_->send_power();
        else if (action == "play_pause") kb_->send_media_play_pause();
        else if (action == "next_track") kb_->send_media_next();
        else if (action == "prev_track") kb_->send_media_prev();
        else if (action == "stop") kb_->send_media_stop();
        else if (action == "volume_up") kb_->send_volume_up();
        else if (action == "volume_down") kb_->send_volume_down();
        else if (action == "mute") kb_->send_mute();
        else if (action == "left_click") kb_->send_mouse_click(0x01);
        else if (action == "right_click") kb_->send_mouse_click(0x02);
        else if (action == "middle_click") kb_->send_mouse_click(0x04);
        else if (action.find("switch_host:") == 0) {
          int slot = 0;
          if (sscanf(action.c_str(), "switch_host:%i", &slot) == 1)
            kb_->switch_host((uint8_t) slot);
        } else if (action.find("forget_host:") == 0) {
          int slot = 0;
          if (sscanf(action.c_str(), "forget_host:%i", &slot) == 1)
            kb_->forget_host((uint8_t) slot);
        } else kb_->send_string(action);
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "switch_host") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : 0;
      kb_->switch_host((uint8_t) slot);
      send_response(200, "text/plain", "OK");

    } else if (path == "forget_host") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : 0;
      kb_->forget_host((uint8_t) slot);
      send_response(200, "text/plain", "OK");

    } else {
      send_response(404, "text/plain", "Unknown endpoint");
    }
  }

 protected:
  EspidfBleKeyboard *kb_;
};

// ── Setup ──────────────────────────────────────────────────────────

void BleKeyboardWebControl::setup() {
  auto *handler = new BleKbWebHandler(this->keyboard_);
  this->base_->add_handler(handler);
  ESP_LOGI(TAG, "Web control registered at /ble_keyboard");
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome

#endif  // USE_BLE_KEYBOARD_WEB_CONTROL



