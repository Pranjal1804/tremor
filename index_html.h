#pragma once

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>EdgeTremor</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body{background:#020617;color:white;font-family:sans-serif;padding:12px;margin:0}
  canvas{border:1px solid #334155;border-radius:8px;display:block;margin-bottom:12px;max-width:100%}
  .row{display:flex;gap:12px;flex-wrap:wrap;margin-top:12px}
  .card{background:#0f172a;border-radius:8px;padding:10px 16px;min-width:100px}
  .label{font-size:11px;color:#94a3b8}
  .val{font-size:22px;font-weight:bold}
  #stateBox{padding:8px 16px;border-radius:6px;display:inline-block;font-weight:bold;margin-bottom:12px}
</style>
</head>
<body>
<h2 style="margin-bottom:8px">EdgeTremor Live</h2>
<div id="stateBox">Connecting...</div>
<canvas id="chart" width="480" height="180"></canvas>
<div class="row">
  <div class="card"><div class="label">X (m/s²)</div><div class="val" id="x">--</div></div>
  <div class="card"><div class="label">Y (m/s²)</div><div class="val" id="y">--</div></div>
  <div class="card"><div class="label">Z (m/s²)</div><div class="val" id="z">--</div></div>
  <div class="card"><div class="label">ZCR</div><div class="val" id="zcr">--</div></div>
  <div class="card"><div class="label">Tremor</div><div class="val" id="tremor">--</div></div>
</div>

<script>
const POINTS = 80;
const bufX = Array(POINTS).fill(0);
const bufY = Array(POINTS).fill(0);
const bufZ = Array(POINTS).fill(0);
const canvas = document.getElementById('chart');
const ctx = canvas.getContext('2d');

function drawChart() {
  const W = canvas.width, H = canvas.height;
  ctx.fillStyle = '#020617';
  ctx.fillRect(0, 0, W, H);

  function drawLine(buf, color) {
    const lo = Math.min(...buf) - 0.5;
    const hi = Math.max(...buf) + 0.5;
    const range = hi - lo || 1;
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    buf.forEach((v, i) => {
      const x = (i / (POINTS - 1)) * W;
      const y = H - ((v - lo) / range) * (H - 4) - 2;
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    });
    ctx.stroke();
  }

  drawLine(bufX, '#38bdf8');
  drawLine(bufY, '#4ade80');
  drawLine(bufZ, '#f472b6');

  [['X','#38bdf8'],['Y','#4ade80'],['Z','#f472b6']].forEach(([l,c],i) => {
    ctx.fillStyle = c;
    ctx.fillRect(8 + i*40, 6, 12, 4);
    ctx.fillStyle = '#fff';
    ctx.font = '11px sans-serif';
    ctx.fillText(l, 24 + i*40, 14);
  });
}

async function fetchData() {
  try {
    const res = await fetch('/data');
    const d = await res.json();
    document.getElementById('x').innerText = d.x.toFixed(2);
    document.getElementById('y').innerText = d.y.toFixed(2);
    document.getElementById('z').innerText = d.z.toFixed(2);
    document.getElementById('zcr').innerText = d.zcr.toFixed(2);
    document.getElementById('tremor').innerText = d.tremor.toFixed(2);
    const box = document.getElementById('stateBox');
    if (d.state) {
      box.innerText = '⚠ TREMOR DETECTED';
      box.style.background = '#7f1d1d';
      box.style.color = '#fca5a5';
    } else {
      box.innerText = '✓ Normal';
      box.style.background = '#14532d';
      box.style.color = '#86efac';
    }
    bufX.push(d.x); bufX.shift();
    bufY.push(d.y); bufY.shift();
    bufZ.push(d.z); bufZ.shift();
    drawChart();
  } catch(e) {
    document.getElementById('stateBox').innerText = 'Connection lost...';
    document.getElementById('stateBox').style.background = '#1e293b';
    document.getElementById('stateBox').style.color = '#94a3b8';
  }
}

setInterval(fetchData, 200);
fetchData();
</script>
</body>
</html>
)rawliteral";