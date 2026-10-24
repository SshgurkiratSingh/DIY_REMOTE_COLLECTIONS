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
#calibBox{display:none;padding:8px;background:#1a1800;border:1px solid #443;border-radius:3px;margin-io:10px 0;text-align:center;font-size:12px;color:#bb3}
#calibBox .prog{height:4px;background:#222;border-radius:2px;margin-top:6px;overflow:hidden}
#calibBox .prog-fill{height:100%;width:0%;background:#aa3;transition:width 0.2s}

/* Review table */
.rv-table{width:100%;border-collapse:collapse;font-size:12px;margin-bottom:12px}
.rv-table td{padding:5px 6px;border-bottom:1px solid #1a1a1a}
.rv-table td:first-child{color:#888}
.rv-table td:last-child{color:#eee;font-family:'SF Mono',Consolas,monospace;text-align:right}

/* Threshold inputs */
.thresh-row{display:flex;align-items:center;gap:6px;margin-bottom:6px;font-size:12px}
.thresh-row .lbl{width:80px;color:#888;flex-shrink:0}
.thresh-row input[type=range]{flex:1;accent-color:#6bf}
.thresh-row .val{width:40px;text-align:right;font-family:'SF Mono',Consolas,monospace;color:#ccc}

/* Telemetry badges */
.stat-box{display:flex;justify-content:space-between;background:#181818;border:1px solid #222;padding:6px 8px;border-radius:3px;margin-bottom:6px;font-size:11px}
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
    <button onclick="tab('pid',this)">PID &amp; Route</button>
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
      <div class="stat-box" style="margin-top:-2px">
        <span>Solved Route: <strong class="mono" id="solvedRoute" style="color:#6bf">-</strong></span>
      </div>

      <table class="s-table">
        <tr><th>Sensor (GPIO)</th><th>Raw</th><th>Level</th><th>Norm</th><th>On Line</th></tr>
        <tr><td class="lbl">S0 - Far Left (39)</td><td class="mono" id="raw0">-</td><td><div class="hbar"><div class="hbar-fill" id="bar0"></div></div></td><td class="mono" id="norm0">-</td><td><span class="ind" id="ind0"></span></td></tr>
        <tr><td class="lbl">S1 - Left (36)</td><td class="mono" id="raw1">-</td><td><div class="hbar"><div class="hbar-fill" id="bar1"></div></div></td><td class="mono" id="norm1">-</td><td><span class="ind" id="ind1"></span></td></tr>
        <tr><td class="lbl">S2 - Centre-Left (35)</td><td class="mono" id="raw2">-</td><td><div class="hbar"><div class="hbar-fill" id="bar2"></div></div></td><td class="mono" id="norm2">-</td><td><span class="ind" id="ind2"></span></td></tr>
        <tr><td class="lbl">S3 - Centre (34)</td><td class="mono" id="raw3">-</td><td><div class="hbar"><div class="hbar-fill" id="bar3"></div></div></td><td class="mono" id="norm3">-</td><td><span class="ind" id="ind3"></span></td></tr>
        <tr><td class="lbl">S4 - Centre-Right (33)</td><td class="mono" id="raw4">-</td><td><div class="hbar"><div class="hbar-fill" id="bar4"></div></div></td><td class="mono" id="norm4">-</td><td><span class="ind" id="ind4"></span></td></tr>
        <tr><td class="lbl">S5 - Right (32)</td><td class="mono" id="raw5">-</td><td><div class="hbar"><div class="hbar-fill" id="bar5"></div></div></td><td class="mono" id="norm5">-</td><td><span class="ind" id="ind5"></span></td></tr>
        <tr style="opacity:0.55"><td class="lbl">S6 - Far Right (4) ADC2</td><td class="mono" id="raw6">0*</td><td><div class="hbar"><div class="hbar-fill" id="bar6"></div></div></td><td class="mono" id="norm6">0*</td><td><span class="ind" id="ind6"></span></td></tr>
        <tr style="opacity:0.55"><td class="lbl">S7 - Far-Far Right (15) ADC2</td><td class="mono" id="raw7">0*</td><td><div class="hbar"><div class="hbar-fill" id="bar7"></div></div></td><td class="mono" id="norm7">0*</td><td><span class="ind" id="ind7"></span></td></tr>
        <tr style="opacity:0.55"><td class="lbl">S8 - Outer Right (2) ADC2</td><td class="mono" id="raw8">0*</td><td><div class="hbar"><div class="hbar-fill" id="bar8"></div></div></td><td class="mono" id="norm8">0*</td><td><span class="ind" id="ind8"></span></td></tr>
      </table>
      <div style="font-size:10px;color:#555;margin-top:2px">* S6-S8 (ADC2) read 0 during calibration mode. They are fully active in running mode (WiFi off).</div>

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
      <div class="sec-title">Calibration (ADC1 sensors S0-S5 only)</div>
      <p style="font-size:12px;color:#777;margin-bottom:8px">Sweep robot across black line and white surface during the 10s window. ADC2 sensors (S6-S8) will inherit S5 thresholds as a default.</p>
      <button class="btn btn-act" id="btnCalib" onclick="startCalibration()">Start Calibration Sweep (10s)</button>
      <div id="calibBox">
        <span>Sweeping... <span class="mono" id="countdown">10</span>s left</span>
        <div class="prog"><div class="prog-fill" id="calibBar"></div></div>
      </div>

      <div style="margin-top:10px">
        <div class="sec-title" style="margin-bottom:6px">Threshold Overrides (all 9 sensors)</div>
        <div id="threshSliders"></div>
        <button class="btn" onclick="applyThresholds()">Apply Thresholds</button>
      </div>

      <div style="margin-top:6px">
        <div class="sec-title" style="margin-bottom:4px">Stored Ranges</div>
        <table class="s-table" style="font-size:11px">
          <tr><th>Sensor</th><th>Min</th><th>Max</th><th>Thresh</th></tr>
          <tr><td class="lbl">S0 FL(39)</td><td class="mono" id="sMin0">-</td><td class="mono" id="sMax0">-</td><td class="mono" id="sTh0">-</td></tr>
          <tr><td class="lbl">S1 L(36)</td><td class="mono" id="sMin1">-</td><td class="mono" id="sMax1">-</td><td class="mono" id="sTh1">-</td></tr>
          <tr><td class="lbl">S2 CL(35)</td><td class="mono" id="sMin2">-</td><td class="mono" id="sMax2">-</td><td class="mono" id="sTh2">-</td></tr>
          <tr><td class="lbl">S3 C(34)</td><td class="mono" id="sMin3">-</td><td class="mono" id="sMax3">-</td><td class="mono" id="sTh3">-</td></tr>
          <tr><td class="lbl">S4 CR(33)</td><td class="mono" id="sMin4">-</td><td class="mono" id="sMax4">-</td><td class="mono" id="sTh4">-</td></tr>
          <tr><td class="lbl">S5 R(32)</td><td class="mono" id="sMin5">-</td><td class="mono" id="sMax5">-</td><td class="mono" id="sTh5">-</td></tr>
          <tr style="opacity:0.6"><td class="lbl">S6 FR(4)*</td><td class="mono" id="sMin6">-</td><td class="mono" id="sMax6">-</td><td class="mono" id="sTh6">-</td></tr>
          <tr style="opacity:0.6"><td class="lbl">S7 FFR(15)*</td><td class="mono" id="sMin7">-</td><td class="mono" id="sMax7">-</td><td class="mono" id="sTh7">-</td></tr>
          <tr style="opacity:0.6"><td class="lbl">S8 OR(2)*</td><td class="mono" id="sMin8">-</td><td class="mono" id="sMax8">-</td><td class="mono" id="sTh8">-</td></tr>
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
      <div class="sec-title">Navigation &amp; Maze Routing</div>
      <div class="field">
        <label>Routing Strategy</label>
        <select id="sel_nav" onchange="toggleRouteBox()">
          <option value="0">Autonomous Left-Hand Rule (LSRB) [Default for unknown maps]</option>
          <option value="1">Autonomous Right-Hand Rule (RSLB)</option>
          <option value="2">Custom / Pre-Programmed Route Sequence</option>
          <option value="3">Standard PID Line Follower</option>
        </select>
        <div class="hint" style="margin-top:4px">LSRB/RSLB: Solves any unknown or changed maze autonomously without hardcoded turns.<br>Custom: Executes specified turn letters (L, R, S).</div>
      </div>
      <div class="field" id="box_customRoute">
        <label>Custom Route Sequence (L = Left, R = Right, S = Straight)</label>
        <input type="text" id="txt_route" maxlength="64" placeholder="e.g. RRLLRLSSLLRRLL" style="width:100%;padding:6px 8px;background:#1a1a1a;border:1px solid #333;border-radius:3px;color:#eee;font-family:'SF Mono',Consolas,monospace;font-size:13px;letter-spacing:1px;text-transform:uppercase">
        <div class="hint">Type the turn sequence you see on the arena track during the 5-min prep window.</div>
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

function toggleRouteBox(){
  const isCustom = $('sel_nav').value === '2';
  $('box_customRoute').style.display = isCustom ? 'block' : 'none';
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
    for(let i=0;i<9;i++){
      const rawEl=$('raw'+i),normEl=$('norm'+i),barEl=$('bar'+i),indEl=$('ind'+i);
      if(rawEl) rawEl.innerText=d.raw[i];
      if(normEl) normEl.innerText=d.norm[i];
      if(barEl){
        barEl.style.width=(d.norm[i]/10)+'%';
        barEl.className=d.on_line[i]?'hbar-fill on':'hbar-fill';
      }
      if(indEl) indEl.className=d.on_line[i]?'ind on':'ind off';
    }
    $('posVal').innerText=d.position;
    $('posDot').style.left=Math.min(Math.max(((d.position+6000)/12000)*100,0),100)+'%';
    $('robotState').innerText=d.status||'-';
    
    if(d.checkpoints!==undefined) $('cpCount').innerText=d.checkpoints;
    if(d.solvedPath) $('solvedRoute').innerText=d.solvedPath;
    if(d.finished){
      $('goalInd').className='ind done';
      $('goalTxt').innerText='COMPLETE';
    }else{
      $('goalInd').className='ind off';
      $('goalTxt').innerText='Seeking';
    }
  }catch(e){}
}

function buildThreshSliders(){
  const wrap=$('threshSliders');
  if(!wrap||wrap.children.length>0)return;
  const names=['S0 FL(39)','S1 L(36)','S2 CL(35)','S3 C(34)','S4 CR(33)','S5 R(32)','S6 FR(4)*','S7 FFR(15)*','S8 OR(2)*'];
  names.forEach((n,i)=>{
    const d=document.createElement('div');
    d.className='thresh-row';
    if(i>=6) d.style.opacity='0.65';
    d.innerHTML='<span class="lbl">'+n+'</span><input type="range" min="0" max="4095" id="sl_th'+i+'" oninput="$(\'vl_th'+i+'\').innerText=this.value"><span class="val" id="vl_th'+i+'">-</span>';
    wrap.appendChild(d);
  });
}

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
    buildThreshSliders();
    for(let i=0;i<9;i++){
      if($('sMin'+i))$('sMin'+i).innerText=d.sensors_min[i];
      if($('sMax'+i))$('sMax'+i).innerText=d.sensors_max[i];
      if($('sTh'+i))$('sTh'+i).innerText=d.sensors_thresh[i];
      if($('sl_th'+i))$('sl_th'+i).value=d.sensors_thresh[i];
      if($('vl_th'+i))$('vl_th'+i).innerText=d.sensors_thresh[i];
    }
    $('rng_kp').value=d.kp;$('num_kp').value=d.kp;
    $('rng_ki').value=d.ki;$('num_ki').value=d.ki;
    $('rng_kd').value=d.kd;$('num_kd').value=d.kd;
    $('rng_base').value=d.baseSpeed;$('num_base').value=d.baseSpeed;
    $('rng_min').value=d.minSpeed;$('num_min').value=d.minSpeed;
    $('rng_max').value=d.maxSpeed;$('num_max').value=d.maxSpeed;
    $('sel_pol').value=d.invertLine;
    $('sel_nav').value=d.navMode;
    if(d.customPath) $('txt_route').value = d.customPath;
    toggleRouteBox();

    let navDesc = 'Autonomous LSRB (Left-Hand Rule)';
    if(d.navMode==1) navDesc = 'Autonomous RSLB (Right-Hand Rule)';
    else if(d.navMode==2) navDesc = 'Custom Route Sequence (' + (d.customPath||'-') + ')';
    else if(d.navMode==3) navDesc = 'Standard PID Line Follower';

    let sRows = '';
    const sLabels=['S0 FL(39)','S1 L(36)','S2 CL(35)','S3 C(34)','S4 CR(33)','S5 R(32)','S6 FR(4)*','S7 FFR(15)*','S8 OR(2)*'];
    for(let i=0;i<9;i++){
      sRows += '<tr><td>'+sLabels[i]+'</td><td>'+d.sensors_min[i]+' - '+d.sensors_max[i]+' (T:'+d.sensors_thresh[i]+')</td></tr>';
    }

    $('rvTable').innerHTML=
      '<tr><td>Calibration</td><td>'+(d.isCalibrated?'Yes':'No')+'</td></tr>'+
      '<tr><td>Strategy</td><td>'+navDesc+'</td></tr>'+
      '<tr><td>Custom Route</td><td>'+(d.customPath||'-')+'</td></tr>'+
      sRows+
      '<tr><td>PID</td><td>'+d.kp+' / '+d.ki+' / '+d.kd+'</td></tr>'+
      '<tr><td>Speed</td><td>'+d.baseSpeed+' ('+d.minSpeed+'-'+d.maxSpeed+')</td></tr>'+
      '<tr><td>Polarity</td><td>'+(d.invertLine==0?'Black on white':'White on black')+'</td></tr>'+
      '<tr><td>Boot #</td><td>'+d.bootCount+'</td></tr>';
  }catch(e){}
}

async function applyThresholds(){
  const b=new URLSearchParams();
  for(let i=0;i<9;i++){
    const el=$('sl_th'+i);
    if(el) b.append('thresh'+i, el.value);
  }
  const r=await fetch('/api/params',{method:'POST',body:b});
  if(r.ok){status('Thresholds saved','ok');loadParams()}
}

async function submitPid(){
  const b=new URLSearchParams({
    kp:$('rng_kp').value,ki:$('rng_ki').value,kd:$('rng_kd').value,
    baseSpeed:$('rng_base').value,minSpeed:$('rng_min').value,maxSpeed:$('rng_max').value,
    invertLine:$('sel_pol').value,
    navMode:$('sel_nav').value,
    customPath:$('txt_route').value
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
    customPath:$('txt_route').value
  });
  for(let i=0;i<9;i++){
    const el=$('sl_th'+i);
    if(el) b.append('thresh'+i, el.value);
  }
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

window.onload=()=>{buildThreshSliders();loadParams();startPoll()};
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
    // API: GET /api/sensors - Live sensor telemetry (ADC1 only — WiFi active)
    // ADC2 sensors (S6-S8) CANNOT be read while WiFi is running.
    // Their slots are reported as 0 in the JSON response.
    // ------------------------------------------------------------------------
    s_server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Read only ADC1 sensors (S0-S5) — safe with active WiFi
        uint16_t rawAll[NUM_SENSORS] = {0};
        rawAll[0] = analogRead(SENSOR_S0_PIN);
        rawAll[1] = analogRead(SENSOR_S1_PIN);
        rawAll[2] = analogRead(SENSOR_S2_PIN);
        rawAll[3] = analogRead(SENSOR_S3_PIN);
        rawAll[4] = analogRead(SENSOR_S4_PIN);
        rawAll[5] = analogRead(SENSOR_S5_PIN);
        // rawAll[6..8] stay 0 — ADC2 not safe here

        // Update calibration sweep if active (ADC1 only)
        if (isCalibrating()) {
            updateCalibration(rawAll, false);
        }

        notifyMotorSafetyFeed();

        // Compute position + status using ADC1-only (activeSensors=6)
        int16_t dummyL = 0, dummyR = 0;
        g_pid.compute(rawAll, ADC1_SENSOR_COUNT, dummyL, dummyR);
        SensorReadings sr = g_pid.getReadings();

        // Build compact JSON with all 9 sensor arrays
        // S6-S8 show raw=0, norm=0, on_line=false in calibration mode
        char json[700];
        snprintf(json, sizeof(json),
                 "{\"raw\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                 "\"norm\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                 "\"on_line\":[%s,%s,%s,%s,%s,%s,%s,%s,%s],"
                 "\"position\":%d,\"status\":\"%s\","
                 "\"checkpoints\":%u,\"finished\":%s,\"solvedPath\":\"%s\","
                 "\"adc2_note\":\"S6-S8 read 0 during calibration (ADC2+WiFi)\"}",
                 sr.raw[0], sr.raw[1], sr.raw[2],
                 sr.raw[3], sr.raw[4], sr.raw[5],
                 sr.raw[6], sr.raw[7], sr.raw[8],
                 sr.norm[0], sr.norm[1], sr.norm[2],
                 sr.norm[3], sr.norm[4], sr.norm[5],
                 sr.norm[6], sr.norm[7], sr.norm[8],
                 sr.onLine[0]?"true":"false", sr.onLine[1]?"true":"false",
                 sr.onLine[2]?"true":"false", sr.onLine[3]?"true":"false",
                 sr.onLine[4]?"true":"false", sr.onLine[5]?"true":"false",
                 sr.onLine[6]?"true":"false", sr.onLine[7]?"true":"false",
                 sr.onLine[8]?"true":"false",
                 sr.position,
                 PIDController::statusToString(sr.status),
                 sr.checkpoints,
                 sr.runFinished ? "true" : "false",
                 sr.solvedPath);

        request->send(200, "application/json", json);
    });

    // ------------------------------------------------------------------------
    // API: GET /api/params - Stored configuration
    // ------------------------------------------------------------------------
    s_server.on("/api/params", HTTP_GET, [](AsyncWebServerRequest *request) {
        char json[900];
        // Inline the 9-sensor arrays directly — no dynamic allocation needed
        snprintf(json, sizeof(json),
                 "{\"isCalibrated\":%u,"
                 "\"sensors_min\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                 "\"sensors_max\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                 "\"sensors_thresh\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                 "\"kp\":%.2f,\"ki\":%.3f,\"kd\":%.2f,"
                 "\"baseSpeed\":%d,\"minSpeed\":%d,\"maxSpeed\":%d,"
                 "\"invertLine\":%u,\"navMode\":%u,\"customPath\":\"%s\",\"bootCount\":%u}",
                 g_params.isCalibrated,
                 g_params.sensors[0].minVal, g_params.sensors[1].minVal,
                 g_params.sensors[2].minVal, g_params.sensors[3].minVal,
                 g_params.sensors[4].minVal, g_params.sensors[5].minVal,
                 g_params.sensors[6].minVal, g_params.sensors[7].minVal,
                 g_params.sensors[8].minVal,
                 g_params.sensors[0].maxVal, g_params.sensors[1].maxVal,
                 g_params.sensors[2].maxVal, g_params.sensors[3].maxVal,
                 g_params.sensors[4].maxVal, g_params.sensors[5].maxVal,
                 g_params.sensors[6].maxVal, g_params.sensors[7].maxVal,
                 g_params.sensors[8].maxVal,
                 g_params.sensors[0].thresh, g_params.sensors[1].thresh,
                 g_params.sensors[2].thresh, g_params.sensors[3].thresh,
                 g_params.sensors[4].thresh, g_params.sensors[5].thresh,
                 g_params.sensors[6].thresh, g_params.sensors[7].thresh,
                 g_params.sensors[8].thresh,
                 g_params.kp, g_params.ki, g_params.kd,
                 g_params.baseSpeed, g_params.minSpeed, g_params.maxSpeed,
                 g_params.invertLine, g_params.navMode, g_params.customPath,
                 getBootCount());

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
        bool     active = isCalibrating();
        uint32_t rem    = getCalibrationRemainingMs();
        uint8_t  pct    = getCalibrationProgressPct();

        uint16_t minArr[NUM_SENSORS], maxArr[NUM_SENSORS];
        getCalibrationSweepValues(minArr, maxArr);

        char json[512];
        snprintf(json, sizeof(json),
                 "{\"active\":%s,\"remaining_ms\":%u,\"progress_pct\":%u,"
                 "\"sweep_min\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
                 "\"sweep_max\":[%u,%u,%u,%u,%u,%u,%u,%u,%u]}",
                 active ? "true" : "false", rem, pct,
                 minArr[0], minArr[1], minArr[2], minArr[3], minArr[4],
                 minArr[5], minArr[6], minArr[7], minArr[8],
                 maxArr[0], maxArr[1], maxArr[2], maxArr[3], maxArr[4],
                 maxArr[5], maxArr[6], maxArr[7], maxArr[8]);

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

        if (request->hasParam("customPath", true)) {
            String r = request->getParam("customPath", true)->value();
            r.toUpperCase();
            String clean = "";
            for (size_t i = 0; i < r.length(); i++) {
                char c = r.charAt(i);
                if (c == 'L' || c == 'R' || c == 'S' || c == 'B') clean += c;
            }
            strncpy(g_params.customPath, clean.c_str(), sizeof(g_params.customPath) - 1);
            g_params.customPath[sizeof(g_params.customPath) - 1] = '\0';
        }

        // Per-sensor threshold override: thresh0..thresh8
        // This allows manual tuning of ADC2 sensor thresholds (S6-S8)
        // that cannot be auto-swept during calibration (WiFi active).
        for (int i = 0; i < NUM_SENSORS; i++) {
            char key[12];
            snprintf(key, sizeof(key), "thresh%d", i);
            if (request->hasParam(key, true)) {
                g_params.sensors[i].thresh = (uint16_t)request->getParam(key, true)->value().toInt();
            }
        }

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
