#pragma once
// Phone web page, served by the ESP32 at http://192.168.4.1
// Everything is inline: the phone has no internet while on the timer's Wi-Fi.

const char PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0f172a">
<title>Shot Timer</title>
<style>
:root{
  --bg:#f1f5f9; --card:#ffffff; --text:#0f172a; --muted:#64748b; --line:#e2e8f0;
  --idle:#64748b; --wait:#d97706; --ready:#16a34a; --run:#2563eb; --bad:#dc2626;
}
@media (prefers-color-scheme: dark){
  :root{ --bg:#0b1120; --card:#151e31; --text:#f1f5f9; --muted:#94a3b8; --line:#26324a; }
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--text);
  font:16px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
main{max-width:520px;margin:0 auto;padding:12px 16px 32px}
header{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:12px}
h1{font-size:18px;margin:0;white-space:nowrap}
.pills{display:flex;gap:6px}
.pill{display:flex;align-items:center;gap:6px;font-size:13px;color:var(--muted);white-space:nowrap;
  background:var(--card);border:1px solid var(--line);border-radius:99px;padding:4px 10px}
.dot{width:9px;height:9px;border-radius:50%;background:var(--muted)}
.dot.on{background:var(--ready)} .dot.off{background:var(--bad)} .dot.hit{background:var(--wait)}

.hero{border-radius:20px;padding:18px 16px 20px;color:#fff;text-align:center;
  background:var(--idle);transition:background .2s}
.hero.wait{background:var(--wait)} .hero.ready{background:var(--ready)}
.hero.run{background:var(--run)} .hero.bad{background:var(--bad)}
.label{font-size:20px;font-weight:700;letter-spacing:.04em;text-transform:uppercase}
.sub{font-size:14px;opacity:.9;min-height:20px}
.big{font-size:min(34vw,170px);font-weight:800;line-height:1;margin:6px 0 10px;
  font-variant-numeric:tabular-nums}
.bar{height:10px;border-radius:99px;background:rgba(255,255,255,.3);overflow:hidden}
.bar>div{height:100%;background:#fff;width:100%}

.wide{display:block;width:100%;margin-top:14px}
button{font:inherit;border:0;border-radius:14px;padding:16px 12px;font-weight:700;cursor:pointer;
  background:var(--card);color:var(--text);border:1px solid var(--line)}
button:active{transform:scale(.98)}
button:disabled{opacity:.4}
.hidden{display:none!important}

.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:14px 16px;margin-top:14px}
.card h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);margin:0 0 10px}
.stepper{display:flex;align-items:center;justify-content:space-between;gap:10px}
.stepper button{width:56px;height:56px;padding:0;font-size:28px;border-radius:50%}
.stepper .val{font-size:40px;font-weight:800;font-variant-numeric:tabular-nums}
.stepper .val small{font-size:16px;color:var(--muted);font-weight:600}
.toggle{display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-top:1px solid var(--line)}
.toggle:first-of-type{border-top:0}
.toggle input{width:22px;height:22px;accent-color:var(--run)}
.note{font-size:13px;color:var(--muted);margin-top:6px}

.links{display:flex;gap:10px;margin-top:10px}
.links button{flex:1;padding:12px;font-weight:600;font-size:14px}

#unlock{position:fixed;inset:0;background:rgba(15,23,42,.92);display:flex;flex-direction:column;
  align-items:center;justify-content:center;gap:14px;padding:24px;text-align:center;color:#fff;z-index:9}
#unlock button{background:var(--ready);color:#fff;border:0;font-size:22px;padding:20px 40px}
#unlock p{max-width:320px;opacity:.85;margin:0}
</style>
</head>
<body>
<div id="unlock">
  <div style="font-size:48px">⚽</div>
  <button id="unlockBtn">Start</button>
  <p>Tap to turn on the phone sound. Turn your volume up (on iPhone: ringer switch on).</p>
</div>

<main>
  <header>
    <h1>⚽ Shot Timer</h1>
    <div class="pills">
      <div class="pill"><span class="dot" id="sensorDot"></span><span id="sensorTxt">Sensor</span></div>
      <div class="pill"><span class="dot" id="connDot"></span><span id="connTxt">…</span></div>
    </div>
  </header>

  <section class="hero" id="hero">
    <div class="label" id="label">Connecting…</div>
    <div class="sub" id="sub"></div>
    <div class="big" id="big">–</div>
    <div class="bar"><div id="barFill"></div></div>
  </section>


  <section class="card">
    <h2>Seconds to score</h2>
    <div class="stepper">
      <button id="minus" aria-label="Less">−</button>
      <div class="val"><span id="durVal">5</span> <small>s</small></div>
      <button id="plus" aria-label="More">+</button>
    </div>
    <div class="note" id="lockNote">You can also turn the knob on the box.</div>
  </section>

  <section class="card">
    <h2>Options</h2>
    <label class="toggle">Re-arm automatically <input type="checkbox" id="optRearm"></label>
    <label class="toggle">Beep on the last 3 seconds <input type="checkbox" id="optTicks"></label>
    <label class="toggle">Buzzer on the box <input type="checkbox" id="optBuzz"></label>
    <label class="toggle">Sound on this phone <input type="checkbox" id="optPhone"></label>
    <div class="links">
      <button data-cmd="test">Test buzzer</button>
      <button id="testPhone">Test phone sound</button>
    </div>
  </section>

  <button id="startBtn" class="wide" data-cmd="start">Start by hand</button>
</main>

<script>
const $ = id => document.getElementById(id);
let S = null, remAt = 0, lastEv = null, lastTickSec = 99, online = false;
let phoneSound = true;
try { phoneSound = localStorage.getItem('phoneSound') !== '0'; } catch (e) {}
$('optPhone').checked = phoneSound;

/* ---------------- sound (Web Audio, no files needed) ---------------- */
let ctx = null;
function audio() {
  if (!ctx) { const A = window.AudioContext || window.webkitAudioContext; if (A) ctx = new A(); }
  if (ctx && ctx.state === 'suspended') ctx.resume();
  return ctx;
}
function tone(freq, dur, type, vol, when) {
  const c = audio(); if (!c || !phoneSound) return;
  const t = c.currentTime + (when || 0);
  const o = c.createOscillator(), g = c.createGain();
  o.type = type || 'sine'; o.frequency.value = freq;
  g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(vol, t + 0.01);
  g.gain.setValueAtTime(vol, t + Math.max(0.02, dur - 0.04));
  g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  o.connect(g); g.connect(c.destination);
  o.start(t); o.stop(t + dur + 0.05);
}
const SOUND = {
  start:   () => tone(880, 0.15, 'square', 0.3),
  tick:    () => tone(660, 0.08, 'square', 0.3),
  timeout: () => { tone(233, 1.1, 'sawtooth', 0.35); tone(311, 1.1, 'sawtooth', 0.35); buzz([400, 100, 400]); },
};
function buzz(p) { if (phoneSound && navigator.vibrate) navigator.vibrate(p); }

$('unlockBtn').onclick = () => { audio(); tone(880, 0.08, 'sine', 0.2); $('unlock').classList.add('hidden'); };

/* ---------------- talking to the box ---------------- */
async function req(url, opts) {
  const ac = new AbortController();
  const t = setTimeout(() => ac.abort(), 1500);
  try {
    const r = await fetch(url, Object.assign({ cache: 'no-store', signal: ac.signal }, opts || {}));
    if (!r.ok) throw new Error(r.status);
    const s = await r.json();
    setOnline(true);
    apply(s);
    return s;
  } finally { clearTimeout(t); }
}
async function poll() {
  try { await req('/state'); } catch (e) { setOnline(false); }
  setTimeout(poll, 150);
}
function cmd(c) { req('/cmd?c=' + c, { method: 'POST' }).catch(() => setOnline(false)); }
function setting(k, v) { req('/settings?' + k + '=' + v, { method: 'POST' }).catch(() => setOnline(false)); }

function setOnline(on) {
  online = on;
  $('connDot').className = 'dot ' + (on ? 'on' : 'off');
  $('connTxt').textContent = on ? 'Online' : 'Offline';
}

function apply(s) {
  S = s; remAt = performance.now();
  if (lastEv !== null && s.ev !== lastEv) {
    if (s.evt === 'start') lastTickSec = 99;
    if (SOUND[s.evt]) SOUND[s.evt]();
  }
  lastEv = s.ev;
  renderStatic();
}

/* ---------------- drawing ---------------- */
function heroInfo() {
  if (!online && !S) return ['wait', 'Connecting…', 'Join the Wi-Fi “FootballTimer”'];
  switch (S.st) {
    case 'idle':    return ['', 'Paused', 'Press the knob on the box to arm'];
    case 'armed':   return S.ready ? ['ready', 'Ready', 'Dribble past the sensor']
                                   : ['wait', 'Waiting', 'Clear the sensor…'];
    case 'running': return ['run', 'Go!', 'Score!'];
    default:        return ['bad', 'Time up', S.set.rearm ? 'Re-arming…' : 'Press the knob on the box to arm'];
  }
}

function renderStatic() {
  if (!S) return;
  const [cls, label, sub] = heroInfo();
  $('hero').className = 'hero ' + cls;
  $('label').textContent = label;
  $('sub').textContent = sub;

  const running = S.st === 'running';
  $('startBtn').disabled = running;

  $('durVal').textContent = S.set.dur;
  $('minus').disabled = running || S.set.dur <= 1;
  $('plus').disabled = running || S.set.dur >= S.max;
  $('lockNote').textContent = running ? 'Locked while the clock runs.' : 'You can also turn the knob on the box (1–' + S.max + ' s).';

  $('optRearm').checked = S.set.rearm;
  $('optTicks').checked = S.set.ticks;
  $('optBuzz').checked = S.set.buzz;

  $('sensorDot').className = 'dot ' + (S.sensor ? 'hit' : 'on');
  $('sensorTxt').textContent = S.sensor ? 'Blocked' : 'Clear';
}

function frame() {
  if (S) {
    let rem = S.dur;
    if (S.st === 'running') {
      rem = Math.max(0, S.rem - (performance.now() - remAt));
      const sec = Math.ceil(rem / 1000);
      if (sec < lastTickSec) {
        if (S.set.ticks && sec <= 3 && rem < S.dur - 200 && sec > 0) SOUND.tick();
        lastTickSec = sec;
      }
    } else if (S.st === 'done') {
      rem = 0;
    }
    $('big').textContent = (rem / 1000).toFixed(1);
    $('barFill').style.width = (100 * rem / S.dur) + '%';
  }
  requestAnimationFrame(frame);
}

/* ---------------- buttons ---------------- */
document.addEventListener('click', e => {
  const b = e.target.closest('[data-cmd]');
  if (b && !b.disabled) { audio(); cmd(b.dataset.cmd); }
});
$('minus').onclick = () => S && setting('dur', S.set.dur - 1);
$('plus').onclick  = () => S && setting('dur', S.set.dur + 1);
$('optRearm').onchange = e => setting('rearm', e.target.checked ? 1 : 0);
$('optTicks').onchange = e => setting('ticks', e.target.checked ? 1 : 0);
$('optBuzz').onchange  = e => setting('buzz',  e.target.checked ? 1 : 0);
$('optPhone').onchange = e => {
  phoneSound = e.target.checked;
  try { localStorage.setItem('phoneSound', phoneSound ? '1' : '0'); } catch (err) {}
};
$('testPhone').onclick = () => { audio(); SOUND.timeout(); };

setOnline(false);
poll();
requestAnimationFrame(frame);
</script>
</body>
</html>
)rawliteral";
