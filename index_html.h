const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>EdgeTremor</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body style="background:#020617;color:white;font-family:sans-serif">

<h2>EdgeTremor Live</h2>

<canvas id="chart" width="400" height="200"></canvas>

<p>X: <span id="x">0</span></p>
<p>Y: <span id="y">0</span></p>
<p>Z: <span id="z">0</span></p>
<p>State: <span id="state">Normal</span></p>
<p>ZCR: <span id="zcr">0</span></p>
<p>Tremor: <span id="tremor">0</span></p>

<script>
const ctx = document.getElementById('chart').getContext('2d');

const chart = new Chart(ctx, {
  type: 'line',
  data: {
    labels: Array(50).fill(''),
    datasets: [
      {label:'X', data:Array(50).fill(0)},
      {label:'Y', data:Array(50).fill(0)},
      {label:'Z', data:Array(50).fill(0)}
    ]
  },
  options: {animation:false}
});

async function fetchData(){
  const res = await fetch('/data');
  const d = await res.json();

  document.getElementById('x').innerText = d.x;
  document.getElementById('y').innerText = d.y;
  document.getElementById('z').innerText = d.z;
  document.getElementById('zcr').innerText = d.zcr;
  document.getElementById('tremor').innerText = d.tremor;
  document.getElementById('state').innerText = d.state ? "FREEZE" : "Normal";

  chart.data.datasets[0].data.push(d.x);
  chart.data.datasets[0].data.shift();

  chart.data.datasets[1].data.push(d.y);
  chart.data.datasets[1].data.shift();

  chart.data.datasets[2].data.push(d.z);
  chart.data.datasets[2].data.shift();

  chart.update();
}

setInterval(fetchData, 200);
</script>

</body>
</html>
)rawliteral";
