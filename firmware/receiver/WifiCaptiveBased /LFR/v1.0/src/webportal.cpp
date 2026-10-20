#include "webportal.h"
#include <WiFi.h>
#include "motor.h"
#include "pid.h"

static DNSServer s_dnsServer;
static AsyncWebServer s_server(HTTP_PORT);

static bool s_portalRunning = false;
static bool s_rebootPending = false;
static uint32_t s_rebootTime = 0;

// ============================================================================
//  Embedded HTML / CSS / JS Portal (stored in flash PROGMEM, no emojis)
// ============================================================================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>LFR Calibration</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,system-ui,sans-serif;background:#111;color:#ccc;font-size:14px;padding:0}
code,.mono{font-family:'SF Mono',Consolas,'Courier New',monospace}
a{color:#6bf}

.wrap{max-width:480px;margin:0 auto;padding:8px 10px 40px}

/* Header */
.hdr{padding:10px 12px;border-bottom:1px solid #333;margin-bottom:8px;display:flex;justify-content:space-between;align-items:baseline}
.hdr h1{font-size:15px;font-weight:600;color:#eee;letter-spacing:-0.3px}
.hdr .meta{font-size:11px;color:#777}

/* Tabs */
.tabs{display:flex;border-bottom:1px solid #333;margin-bottom:12px;gap:0}
.tabs button{flex:1;padding:8px 0;background:none;border:none;border-bottom:2px solid transparent;color:#888;font-size:13px;font-weight:500;cursor:pointer}
.tabs button.on{color:#eee;border-bottom-color:#6bf}
.pane{display:none}
.pane.on{display:block}

/* Section */
.sec{margin-bottom:14px}
.sec-title{font-size:12px;font-weight:600;color:#999;text-transform:uppercase;letter-spacing:0.5px;margin-bottom:8px;padding-bottom:4px;border-bottom:1px solid #222}

/* Status bar */
#statusBar{font-size:11px;padding:4px 8px;background:#1a1a1a;border:1px solid #333;border-radius:3px;margin-bottom:10px;color:#888;display:none;text-align:center}
#statusBar.ok{display:block;border-color:#2a4;color:#6d6}
#statusBar.err{display:block;border-color:#a33;color:#d66}
#statusBar.warn{display:block;border-color:#a80;color:#da3}

/* Sensor table */
.s-table{width:100%;border-collapse:collapse;margin-bottom:10px}
.s-table th{font-size:11px;color:#666;font-weight:500;text-align:left;padding:4px 6px;border-bottom:1px solid #222}
.s-table td{padding:5px 6px;border-bottom:1px solid #1a1a1a;font-size:13px}
.s-table .mono{font-size:13px;color:#eee}
.s-table .lbl{color:#888;font-size:12px}
.ind{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px;vertical-align:middle;background:#333}
.ind.on{background:#e44}
.ind.off{background:#3a3}
.ind.done{background:#2a4}

/* Horizontal bar */
.hbar{height:6px;background:#222;border-radius:3px;overflow:hidden;width:100%;max-width:120px}
.hbar-fill{height:100%;background:#556;border-radius:3px;transition:width 0.15s}
.hbar-fill.on{background:#c44}

/* Position bar */
.pos-wrap{margin:10px 0;padding:8px;background:#1a1a1a;border:1px solid #222;border-radius:3px}
.pos-row{display:flex;align-items:center;gap:8px;font-size:12px}
.pos-row .lbl{color:#888;width:55px;flex-shrink:0}
.pos-track{flex:1;height:8px;background:#222;border-radius:4px;position:relative}
.pos-dot{width:10px;height:10px;background:#6bf;border-radius:50%;position:absolute;top:-1px;transform:translateX(-50%);transition:left 0.15s}
.pos-center{position:absolute;left:50%;top:0;bottom:0;width:1px;background:#444}
.pos-val{width:50px;text-align:right;font-family:'SF Mono',Consolas,monospace;color:#eee;font-size:12px}

/* Form */
.field{margin-bottom:10px}
.field label{display:block;font-size:12px;color:#999;margin-bottom:3px}
.field .hint{font-size:11px;color:#555;margin-bottom:4px}
.field input[type=number]{width:80px;padding:5px 6px;background:#1a1a1a;border:1px solid #333;border-radius:3px;color:#eee;font-family:'SF Mono',Consolas,monospace;font-size:13px;text-align:right}
.field input[type=range]{width:100%;accent-color:#6bf;margin:2px 0}
.field select{width:100%;padding:6px 8px;background:#1a1a1a;border:1px solid #333;border-radius:3px;color:#eee;font-size:13px}
.row{display:flex;gap:8px;align-items:center}
.row input[type=range]{flex:1}

/* Buttons */
.btn{display:block;width:100%;padding:8px;border:1px solid #333;border-radius:3px;background:#1a1a1a;color:#ccc;font-size:13px;font-weight:500;cursor:pointer;text-align:center;margin-bottom:6px}
.btn:active{background:#252525}
.btn-act{border-color:#369;color:#8cf}
.btn-go{border-color:#2a4;color:#6d6;background:#0e1a0e}
.btn-stop{border-color:#a33;color:#faa;background:#1a0e0e}
.btn-reset{border-color:#a80;color:#da3}
.btn-sm{display:inline-block;width:auto;padding:6px 12px;font-size:12px}

/* Motor grid */
.m-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-bottom:8px}
.m-grid .btn{margin:0}

/* Calibration progress */
#calibBox{display:none;padding:8px;background:#1a1800;border:1px solid #443;border-radius:3px;margin:10px 0;text-align:center;font-size:12px;color:#bb3}
#calibBox .prog{height:4px;background:#222;border-radius:2px;margin-top:6px;overflow:hidden}
#calibBox .prog-fill{height:100%;width:0%;background:#aa3;transition:width 0.2s}

/* Review table */
.rv-table{width:100%;border-collapse:collapse;font-size:12px;margin-bottom:12px}
.rv-table td{padding:5px 6px;border-bottom:1px solid #1a1a1a}
.rv-table td:first-child{color:#888}
.rv-table td:last-child{color:#eee;font-family:'SF Mono',Consolas,monospace;text-align:right}

/* Threshold inputs */
.thresh-row{display:flex;align-items:center;gap:6px;margin-bottom:6px;font-size:12px}
.thresh-row .lbl{width:50px;color:#888;flex-shrink:0}
.thresh-row input[type=range]{flex:1;accent-color:#6bf}
.thresh-row .val{width:40px;text-align:right;font-family:'SF Mono',Consolas,monospace;color:#ccc}

/* Telemetry badges */
.stat-box{display:flex;justify-content:space-between;background:#181818;border:1px solid #222;padding:6px 8px;border-radius:3px;margin-bottom:8px;font-size:11px}
</style>
</head>
<body>
<div class="wrap">
  <div class="hdr">
    <h1>LFR Calibration</h1>
    <span class="meta">Boot #<span class="mono" id="bootCount">-</span> &middot; 192.168.4.1</span>
  </div>

  <div id="statusBar"></div>

  <div class="tabs">
    <button class="on" onclick="tab('sensors',this)">Sensors</button>
    <button onclick="tab('pid',this)">PID</button>
    <button onclick="tab('motors',this)">Motors</button>
    <button onclick="tab('save',this)">Save</button>
  </div>

  <!-- SENSORS -->
  <div id="p-sensors" class="pane on">
    <div class="sec">
      <div class="sec-title">Live Readings &middot; <span id="robotState" class="mono" style="text-transform:none;letter-spacing:0;color:#6bf">-</span></div>
      
      <div class="stat-box">
        <span>Checkpoints: <strong class="mono" id="cpCount" style="color:#eee">0</strong> / 4</span>
        <span>Goal: <span class="ind" id="goalInd"></span><strong class="mono" id="goalTxt" style="color:#eee">Seeking</strong></span>
      </div>

      <table class="s-table">
        <tr><th>Sensor</th><th>Raw</th><th>Norm</th><th>Level</th><th>Status</th></tr>
        <tr>
          <td class="lbl">Left (34)</td>
          <td class="mono" id="rawL">-</td>
          <td><div class="hbar"><div class="hbar-fill" id="barL"></div></div></td>
          <td class="mono" id="normL">-</td>
          <td><span class="ind" id="indL"></span><span id="stL" style="font-size:11px">-</span></td>
        </tr>
        <tr>
          <td class="lbl">Center (35)</td>
          <td class="mono" id="rawC">-</td>
          <td><div class="hbar"><div class="hbar-fill" id="barC"></div></div></td>
          <td class="mono" id="normC">-</td>
          <td><span class="ind" id="indC"></span><span id="stC" style="font-size:11px">-</span></td>
        </tr>
        <tr>
          <td class="lbl">Right (36)</td>
          <td class="mono" id="rawR">-</td>
          <td><div class="hbar"><div class="hbar-fill" id="barR"></div></div></td>
          <td class="mono" id="normR">-</td>
          <td><span class="ind" id="indR"></span><span id="stR" style="font-size:11px">-</span></td>
        </tr>
      </table>

      <div class="pos-wrap">
        <div class="pos-row">
          <span class="lbl">Position</span>
          <div class="pos-track">
            <div class="pos-center"></div>
            <div class="pos-dot" id="posDot" style="left:50%"></div>
          </div>
          <span class="pos-val" id="posVal">0</span>
        </div>
      </div>
    </div>

    <div class="sec">
      <div class="sec-title">Calibration (5-Min Setup Window)</div>
      <p style="font-size:12px;color:#777;margin-bottom:8px">Place robot on track surface and sweep across black line and white background during the 10s window.</p>
      <button class="btn btn-act" id="btnCalib" onclick="startCalibration()">Start Calibration Sweep (10s)</button>
      <div id="calibBox">
        <span>Sweeping... <span class="mono" id="countdown">10</span>s left</span>
        <div class="prog"><div class="prog-fill" id="calibBar"></div></div>
      </div>

      <div style="margin-top:10px">
        <div class="sec-title" style="margin-bottom:6px">Threshold Overrides</div>
        <div class="thresh-row"><span class="lbl">Left</span><input type="range" min="0" max="4095" id="sl_thL" oninput="thl('L')"><span class="val" id="vl_thL">-</span></div>
        <div class="thresh-row"><span class="lbl">Center</span><input type="range" min="0" max="4095" id="sl_thC" oninput="thl('C')"><span class="val" id="vl_thC">-</span></div>
        <div class="thresh-row"><span class="lbl">Right</span><input type="range" min="0" max="4095" id="sl_thR" oninput="thl('R')"><span class="val" id="vl_thR">-</span></div>
        <button class="btn" onclick="applyThresholds()">Apply Thresholds</button>
      </div>

      <div style="margin-top:6px">
        <div class="sec-title" style="margin-bottom:4px">Stored Ranges</div>
        <table class="s-table" style="font-size:11px">
          <tr><th>Sensor</th><th>Min</th><th>Max</th><th>Threshold</th></tr>
          <tr><td class="lbl">Left</td><td class="mono" id="sMinL">-</td><td class="mono" id="sMaxL">-</td><td class="mono" id="sThL">-</td></tr>
          <tr><td class="lbl">Center</td><td class="mono" id="sMinC">-</td><td class="mono" id="sMaxC">-</td><td class="mono" id="sThC">-</td></tr>
          <tr><td class="lbl">Right</td><td class="mono" id="sMinR">-</td><td class="mono" id="sMaxR">-</td><td class="mono" id="sThR">-</td></tr>
        </table>
      </div>

      <div style="margin-top:10px;border-top:1px solid #222;padding-top:8px">
        <div class="sec-title" style="margin-bottom:4px">Rulebook Quick Reference</div>
        <table class="rv-table" style="font-size:11px">
          <tr><td>Bot Dimensions</td><td>25 x 25 x 25 cm max (10% tol)</td></tr>
          <tr><td>Power Supply</td><td>Max 3S (12.6V DC)</td></tr>
          <tr><td>Start Trigger</td><td>Single physical BOOT button (GPIO 0)</td></tr>
          <tr><td>Line Width</td><td>30mm black line on white surface</td></tr>
          <tr><td>Checkpoints</td><td>4 transverse line checkpoints</td></tr>
          <tr><td>End Target</td><td>Zone B large black square</td></tr>
          <tr><td>Max Run Time</td><td>10 minutes</td></tr>
        </table>
      </div>
    </div>
  </div>

  <!-- PID & NAVIGATION -->
  <div id="p-pid" class="pane">
    <div class="sec">
      <div class="sec-title">Navigation Strategy</div>
      <div class="field">
        <label>Routing Mode</label>
        <select id="sel_nav">
          <option value="1">Official Competition Map (Pre-Programmed Shortest Route)</option>
          <option value="0">Autonomous Maze Solver (Left-Hand Rule / LSRB)</option>
          <option value="2">Standard PID Line Follower</option>
        </select>
        <div class="hint" style="margin-top:4px">Official Map: Uses exact turn sequence for the Rulebook arena layout.<br>LSRB: Fully autonomous exploration for any unknown grid track.</div>
      </div>
    </div>

    <div class="sec">
      <div class="sec-title">PID Gains</div>
      <div class="field">
        <label>Kp (Proportional)</label>
        <div class="hint">Steering strength proportional to error. Higher = more aggressive turns.</div>
        <div class="row"><input type="range" id="rng_kp" min="0" max="100" step="0.5" value="25" oninput="sv('kp')"><input type="number" id="num_kp" min="0" max="100" step="0.5" value="25" onchange="sv('kp')"></div>
      </div>
      <div class="field">
        <label>Ki (Integral)</label>
        <div class="hint">Corrects persistent offset on curves. Anti-windup clamped at 10000.</div>
        <div class="row"><input type="range" id="rng_ki" min="0" max="2" step="0.01" value="0.1" oninput="sv('ki')"><input type="number" id="num_ki" min="0" max="2" step="0.01" value="0.1" onchange="sv('ki')"></div>
      </div>
      <div class="field">
        <label>Kd (Derivative)</label>
        <div class="hint">Dampens oscillation. Prevents overshooting when returning to center.</div>
        <div class="row"><input type="range" id="rng_kd" min="0" max="80" step="0.5" value="15" oninput="sv('kd')"><input type="number" id="num_kd" min="0" max="80" step="0.5" value="15" onchange="sv('kd')"></div>
      </div>
    </div>

    <div class="sec">
      <div class="sec-title">Speed (PWM 0-255)</div>
      <div class="field">
        <label>Base Speed</label>
        <div class="row"><input type="range" id="rng_base" min="0" max="255" value="150" oninput="sv('base')"><input type="number" id="num_base" min="0" max="255" value="150" onchange="sv('base')"></div>
      </div>
      <div class="field">
        <label>Min Speed</label>
        <div class="row"><input type="range" id="rng_min" min="0" max="255" value="0" oninput="sv('min')"><input type="number" id="num_min" min="0" max="255" value="0" onchange="sv('min')"></div>
      </div>
      <div class="field">
        <label>Max Speed</label>
        <div class="row"><input type="range" id="rng_max" min="0" max="255" value="255" oninput="sv('max')"><input type="number" id="num_max" min="0" max="255" value="255" onchange="sv('max')"></div>
      </div>
    </div>

    <div class="sec">
      <div class="sec-title">Line Polarity</div>
      <div class="field">
        <select id="sel_pol">
          <option value="0">Black line on white surface (Rulebook standard)</option>
          <option value="1">White line on black surface</option>
        </select>
      </div>
    </div>

    <button class="btn btn-act" onclick="submitPid()">Apply Settings</button>
  </div>

  <!-- MOTORS -->
  <div id="p-motors" class="pane">
    <div class="sec">
      <div class="sec-title">Motor Test</div>
      <p style="font-size:12px;color:#777;margin-bottom:8px">Verify L298N wiring. Each button runs one motor at the selected speed.</p>
      <div class="field">
        <label>Test Speed: <span class="mono" id="tsLabel">120</span></label>
        <input type="range" id="testSpeed" min="40" max="255" value="120" oninput="document.getElementById('tsLabel').innerText=this.value">
      </div>
      <div class="m-grid">
        <button class="btn" onclick="mt('lf')">Left Fwd</button>
        <button class="btn" onclick="mt('rf')">Right Fwd</button>
        <button class="btn" onclick="mt('lr')">Left Rev</button>
        <button class="btn" onclick="mt('rr')">Right Rev</button>
      </div>
      <button class="btn" onclick="mt('forward')">Both Forward</button>
      <button class="btn btn-stop" onclick="mt('stop')">STOP</button>
    </div>
  </div>

  <!-- SAVE -->
  <div id="p-save" class="pane">
    <div class="sec">
      <div class="sec-title">Current NVS Parameters</div>
      <table class="rv-table" id="rvTable">
        <tr><td colspan="2" style="color:#555">Loading...</td></tr>
      </table>
    </div>
    <button class="btn btn-act" onclick="saveAll()">Save All to NVS</button>
    <button class="btn btn-go" onclick="saveAndRun()">Save &amp; Start LFR (Reboot to Ready State)</button>
    <button class="btn btn-reset" onclick="resetDef()">Reset to Defaults</button>
  </div>
</div>

<script>
let pollId=null,calibId=null;

function $(id){return document.getElementById(id)}
function status(msg,type){const b=$('statusBar');b.innerText=msg;b.className=type;setTimeout(()=>b.style.display='none',3000)}

function tab(name,el){
  document.querySelectorAll('.tabs button').forEach(b=>b.classList.remove('on'));
  document.querySelectorAll('.pane').forEach(p=>p.classList.remove('on'));
  el.classList.add('on');
  $('p-'+name).classList.add('on');
  if(name==='sensors'){startPoll()}else{stopPoll()}
  if(name==='save')loadParams();
}

function sv(k){
  const r=$('rng_'+k),n=$('num_'+k);
  if(document.activeElement===r)n.value=r.value;
  else r.value=n.value;
}

function startPoll(){stopPoll();pollId=setInterval(fetchS,200);fetchS()}
function stopPoll(){if(pollId){clearInterval(pollId);pollId=null}}

async function fetchS(){
  try{
    const r=await fetch('/api/sensors');
    if(!r.ok)return;
    const d=await r.json();
    $('rawL').innerText=d.raw.l;$('rawC').innerText=d.raw.c;$('rawR').innerText=d.raw.r;
    $('normL').innerText=d.norm.l;$('normC').innerText=d.norm.c;$('normR').innerText=d.norm.r;
    $('barL').style.width=(d.norm.l/10)+'%';$('barC').style.width=(d.norm.c/10)+'%';$('barR').style.width=(d.norm.r/10)+'%';
    setInd('L',d.on_line.l);setInd('C',d.on_line.c);setInd('R',d.on_line.r);
    $('posVal').innerText=d.position;
    $('posDot').style.left=Math.min(Math.max(((d.position+1000)/2000)*100,0),100)+'%';
    $('robotState').innerText=d.status||'-';
    
    if(d.checkpoints!==undefined) $('cpCount').innerText=d.checkpoints;
    if(d.finished){
      $('goalInd').className='ind done';
      $('goalTxt').innerText='COMPLETE';
    }else{
      $('goalInd').className='ind off';
      $('goalTxt').innerText='Seeking';
    }
  }catch(e){}
}

function setInd(ch,on){
  const ind=$('ind'+ch),st=$('st'+ch),bar=$('bar'+ch);
  ind.className=on?'ind on':'ind off';
  st.innerText=on?'ON':'off';
  bar.className=on?'hbar-fill on':'hbar-fill';
}

function thl(ch){$('vl_th'+ch).innerText=$('sl_th'+ch).value}

async function startCalibration(){
  try{
    const r=await fetch('/api/calibrate/start',{method:'POST'});
    const d=await r.json();
    if(d.success){
      $('calibBox').style.display='block';$('btnCalib').disabled=true;
      if(calibId)clearInterval(calibId);
      calibId=setInterval(checkCalib,200);
    }
  }catch(e){status('Calibration failed','err')}
}

async function checkCalib(){
  try{
    const r=await fetch('/api/calibrate/status');
    const d=await r.json();
    $('countdown').innerText=Math.ceil(d.remaining_ms/1000);
    $('calibBar').style.width=d.progress_pct+'%';
    if(!d.active){
      clearInterval(calibId);calibId=null;
      $('calibBox').style.display='none';$('btnCalib').disabled=false;
      status('Calibration complete','ok');loadParams();
    }
  }catch(e){}
}

async function loadParams(){
  try{
    const r=await fetch('/api/params');if(!r.ok)return;
    const d=await r.json();
    $('bootCount').innerText=d.bootCount;
    $('sMinL').innerText=d.minL;$('sMaxL').innerText=d.maxL;$('sThL').innerText=d.threshL;
    $('sMinC').innerText=d.minC;$('sMaxC').innerText=d.maxC;$('sThC').innerText=d.threshC;
    $('sMinR').innerText=d.minR;$('sMaxR').innerText=d.maxR;$('sThR').innerText=d.threshR;
    $('sl_thL').value=d.threshL;$('sl_thC').value=d.threshC;$('sl_thR').value=d.threshR;
    thl('L');thl('C');thl('R');
    $('rng_kp').value=d.kp;$('num_kp').value=d.kp;
    $('rng_ki').value=d.ki;$('num_ki').value=d.ki;
    $('rng_kd').value=d.kd;$('num_kd').value=d.kd;
    $('rng_base').value=d.baseSpeed;$('num_base').value=d.baseSpeed;
    $('rng_min').value=d.minSpeed;$('num_min').value=d.minSpeed;
    $('rng_max').value=d.maxSpeed;$('num_max').value=d.maxSpeed;
    $('sel_pol').value=d.invertLine;
    $('sel_nav').value=d.navMode;

    let navDesc = 'Official Competition Map';
    if(d.navMode==0) navDesc = 'Autonomous LSRB Maze Solver';
    else if(d.navMode==2) navDesc = 'Standard PID Line Follower';

    $('rvTable').innerHTML=
      '<tr><td>Calibration</td><td>'+(d.isCalibrated?'Yes':'No')+'</td></tr>'+
      '<tr><td>Routing Strategy</td><td>'+navDesc+'</td></tr>'+
      '<tr><td>Left range</td><td>'+d.minL+' - '+d.maxL+' (T:'+d.threshL+')</td></tr>'+
      '<tr><td>Center range</td><td>'+d.minC+' - '+d.maxC+' (T:'+d.threshC+')</td></tr>'+
      '<tr><td>Right range</td><td>'+d.minR+' - '+d.maxR+' (T:'+d.threshR+')</td></tr>'+
      '<tr><td>PID</td><td>'+d.kp+' / '+d.ki+' / '+d.kd+'</td></tr>'+
      '<tr><td>Speed</td><td>'+d.baseSpeed+' ('+d.minSpeed+'-'+d.maxSpeed+')</td></tr>'+
      '<tr><td>Polarity</td><td>'+(d.invertLine==0?'Black on white':'White on black')+'</td></tr>'+
      '<tr><td>Boot #</td><td>'+d.bootCount+'</td></tr>';
  }catch(e){}
}

async function applyThresholds(){
  const b=new URLSearchParams({threshL:$('sl_thL').value,threshC:$('sl_thC').value,threshR:$('sl_thR').value});
  const r=await fetch('/api/params',{method:'POST',body:b});
  if(r.ok){status('Thresholds saved','ok');loadParams()}
}

async function submitPid(){
  const b=new URLSearchParams({
    kp:$('rng_kp').value,ki:$('rng_ki').value,kd:$('rng_kd').value,
    baseSpeed:$('rng_base').value,minSpeed:$('rng_min').value,maxSpeed:$('rng_max').value,
    invertLine:$('sel_pol').value,
    navMode:$('sel_nav').value
  });
  const r=await fetch('/api/params',{method:'POST',body:b});
  if(r.ok){status('Settings saved','ok');loadParams()}
}

async function mt(action){
  const b=new URLSearchParams({action,speed:$('testSpeed').value});
  await fetch('/api/motor/test',{method:'POST',body:b});
}

async function saveAll(){
  const b=new URLSearchParams({
    kp:$('rng_kp').value,ki:$('rng_ki').value,kd:$('rng_kd').value,
    baseSpeed:$('rng_base').value,minSpeed:$('rng_min').value,maxSpeed:$('rng_max').value,
    invertLine:$('sel_pol').value,
    navMode:$('sel_nav').value,
    threshL:$('sl_thL').value,threshC:$('sl_thC').value,threshR:$('sl_thR').value
  });
  const r=await fetch('/api/params',{method:'POST',body:b});
  if(r.ok){status('All parameters saved to NVS','ok');loadParams()}
}

async function saveAndRun(){
  if(!confirm('Save parameters and reboot to Ready state?'))return;
  status('Rebooting...','warn');
  try{await fetch('/api/saveandrun',{method:'POST'})}catch(e){}
}

async function resetDef(){
  if(!confirm('Reset all parameters to defaults?'))return;
  const r=await fetch('/api/reset',{method:'POST'});
  if(r.ok){status('Reset to defaults','ok');loadParams()}
}

window.onload=()=>{loadParams();startPoll()};
</script>
</body>
</html>
)rawliteral";

// ============================================================================
//  Web Portal Initialization & Handlers
// ============================================================================
void initWebPortal() {
    Serial.println("[PORTAL] Starting WiFi SoftAP and Captive Portal...");

    WiFi.mode(WIFI_AP);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);

    Serial.printf("[PORTAL] AP '%s' active at %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    // Start DNS server on port 53 with wildcard redirection for captive portal
    s_dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    s_dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Serve Captive Portal / Root
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // Captive Portal Detection URLs
    s_server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
    s_server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
    s_server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });
    s_server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });
    s_server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
    s_server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Microsoft NCSI");
    });

    // ------------------------------------------------------------------------
    // API: GET /api/sensors - Live sensor telemetry & match state
    // ------------------------------------------------------------------------
    s_server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        uint16_t rawL = analogRead(SENSOR_LEFT_PIN);
        uint16_t rawC = analogRead(SENSOR_CENTER_PIN);
        uint16_t rawR = analogRead(SENSOR_RIGHT_PIN);

        // Update calibration sweep if active
        if (isCalibrating()) {
            updateCalibration(rawL, rawC, rawR);
        }

        // Feed motor safety watchdog
        notifyMotorSafetyFeed();

        // Calculate normalized values & position
        int16_t dummyL = 0, dummyR = 0;
        g_pid.compute(rawL, rawC, rawR, dummyL, dummyR);
        SensorReadings sr = g_pid.getReadings();

        char json[380];
        snprintf(json, sizeof(json),
                 "{\"raw\":{\"l\":%u,\"c\":%u,\"r\":%u},"
                 "\"norm\":{\"l\":%u,\"c\":%u,\"r\":%u},"
                 "\"on_line\":{\"l\":%s,\"c\":%s,\"r\":%s},"
                 "\"position\":%d,\"status\":\"%s\","
                 "\"checkpoints\":%u,\"finished\":%s}",
                 sr.rawL, sr.rawC, sr.rawR,
                 sr.normL, sr.normC, sr.normR,
                 sr.onLineL ? "true" : "false",
                 sr.onLineC ? "true" : "false",
                 sr.onLineR ? "true" : "false",
                 sr.position,
                 PIDController::statusToString(sr.status),
                 sr.checkpoints,
                 sr.runFinished ? "true" : "false");

        request->send(200, "application/json", json);
    });

    // ------------------------------------------------------------------------
    // API: GET /api/params - Stored configuration
    // ------------------------------------------------------------------------
    s_server.on("/api/params", HTTP_GET, [](AsyncWebServerRequest *request) {
        char json[450];
        snprintf(json, sizeof(json),
                 "{\"isCalibrated\":%u,"
                 "\"minL\":%u,\"maxL\":%u,\"threshL\":%u,"
                 "\"minC\":%u,\"maxC\":%u,\"threshC\":%u,"
                 "\"minR\":%u,\"maxR\":%u,\"threshR\":%u,"
                 "\"kp\":%.2f,\"ki\":%.3f,\"kd\":%.2f,"
                 "\"baseSpeed\":%d,\"minSpeed\":%d,\"maxSpeed\":%d,"
                 "\"invertLine\":%u,\"navMode\":%u,\"bootCount\":%u}",
                 g_params.isCalibrated,
                 g_params.minL, g_params.maxL, g_params.threshL,
                 g_params.minC, g_params.maxC, g_params.threshC,
                 g_params.minR, g_params.maxR, g_params.threshR,
                 g_params.kp, g_params.ki, g_params.kd,
                 g_params.baseSpeed, g_params.minSpeed, g_params.maxSpeed,
                 g_params.invertLine, g_params.navMode, getBootCount());

        request->send(200, "application/json", json);
    });

    // ------------------------------------------------------------------------
    // API: POST /api/calibrate/start - Begin 10s sensor sweep
    // ------------------------------------------------------------------------
    s_server.on("/api/calibrate/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        startCalibration(CALIBRATION_DURATION_MS);
        request->send(200, "application/json", "{\"success\":true,\"duration\":10000}");
    });

    // ------------------------------------------------------------------------
    // API: GET /api/calibrate/status - Calibration sweep progress
    // ------------------------------------------------------------------------
    s_server.on("/api/calibrate/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool active = isCalibrating();
        uint32_t rem = getCalibrationRemainingMs();
        uint8_t pct = getCalibrationProgressPct();

        uint16_t minL, maxL, minC, maxC, minR, maxR;
        getCalibrationSweepValues(minL, maxL, minC, maxC, minR, maxR);

        char json[300];
        snprintf(json, sizeof(json),
                 "{\"active\":%s,\"remaining_ms\":%u,\"progress_pct\":%u,"
                 "\"sweep\":{\"minL\":%u,\"maxL\":%u,\"minC\":%u,\"maxC\":%u,\"minR\":%u,\"maxR\":%u}}",
                 active ? "true" : "false", rem, pct,
                 minL, maxL, minC, maxC, minR, maxR);

        request->send(200, "application/json", json);
    });

    // ------------------------------------------------------------------------
    // API: POST /api/params - Save parameters
    // ------------------------------------------------------------------------
    s_server.on("/api/params", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("kp", true)) g_params.kp = request->getParam("kp", true)->value().toFloat();
        if (request->hasParam("ki", true)) g_params.ki = request->getParam("ki", true)->value().toFloat();
        if (request->hasParam("kd", true)) g_params.kd = request->getParam("kd", true)->value().toFloat();

        if (request->hasParam("baseSpeed", true)) g_params.baseSpeed = request->getParam("baseSpeed", true)->value().toInt();
        if (request->hasParam("minSpeed", true)) g_params.minSpeed = request->getParam("minSpeed", true)->value().toInt();
        if (request->hasParam("maxSpeed", true)) g_params.maxSpeed = request->getParam("maxSpeed", true)->value().toInt();

        if (request->hasParam("invertLine", true)) g_params.invertLine = request->getParam("invertLine", true)->value().toInt();
        if (request->hasParam("navMode", true)) g_params.navMode = request->getParam("navMode", true)->value().toInt();

        if (request->hasParam("threshL", true)) g_params.threshL = request->getParam("threshL", true)->value().toInt();
        if (request->hasParam("threshC", true)) g_params.threshC = request->getParam("threshC", true)->value().toInt();
        if (request->hasParam("threshR", true)) g_params.threshR = request->getParam("threshR", true)->value().toInt();

        saveParameters(g_params);
        g_pid.updateParams(g_params);

        request->send(200, "application/json", "{\"success\":true,\"message\":\"Parameters saved\"}");
    });

    // ------------------------------------------------------------------------
    // API: POST /api/motor/test - Test motor channel
    // ------------------------------------------------------------------------
    s_server.on("/api/motor/test", HTTP_POST, [](AsyncWebServerRequest *request) {
        String action = "stop";
        int16_t speed = 120;

        if (request->hasParam("action", true)) {
            action = request->getParam("action", true)->value();
        }
        if (request->hasParam("speed", true)) {
            speed = (int16_t)request->getParam("speed", true)->value().toInt();
        }

        testMotor(action, speed);
        notifyMotorSafetyFeed();

        request->send(200, "application/json", "{\"success\":true}");
    });

    // ------------------------------------------------------------------------
    // API: POST /api/reset - Factory defaults
    // ------------------------------------------------------------------------
    s_server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        resetParametersToDefaults(g_params);
        saveParameters(g_params);
        g_pid.updateParams(g_params);
        emergencyStop();

        request->send(200, "application/json", "{\"success\":true,\"message\":\"Reset to defaults\"}");
    });

    // ------------------------------------------------------------------------
    // API: POST /api/saveandrun - Save and reboot to running mode
    // ------------------------------------------------------------------------
    s_server.on("/api/saveandrun", HTTP_POST, [](AsyncWebServerRequest *request) {
        saveParameters(g_params);
        emergencyStop();

        // Ensure next boot counter does not land on calibration interval
        uint32_t bc = getBootCount();
        if ((bc + 1) % CALIBRATION_BOOT_INTERVAL == 0) {
            // Set boot counter to 1 so next boot is 2 (running mode)
            resetBootCount();
            incrementBootCount(); // sets to 1
        }

        s_rebootPending = true;
        s_rebootTime = millis() + 1000;

        request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting into Ready State...\"}");
    });

    // Catch-all 404 handler for Captive Portal redirection
    s_server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    s_server.begin();
    s_portalRunning = true;
    Serial.println("[PORTAL] Web server and captive portal started successfully.");
}

void handleWebPortalLoop() {
    if (!s_portalRunning) return;

    // Process DNS requests
    s_dnsServer.processNextRequest();

    // Check deferred reboot
    if (s_rebootPending && millis() >= s_rebootTime) {
        Serial.println("[PORTAL] Executing planned reboot into Running Mode...");
        delay(100);
        ESP.restart();
    }
}

bool isRebootPending() {
    return s_rebootPending;
}

bool isPortalRunning() {
    return s_portalRunning;
}
