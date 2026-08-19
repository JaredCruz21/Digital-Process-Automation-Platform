const inputs = {
  mashTemp: document.querySelector("#mashTemp"),
  mashDuration: document.querySelector("#mashDuration"),
  boilDuration: document.querySelector("#boilDuration"),
  fillVolume: document.querySelector("#fillVolume")
};

const outputs = {
  mashTemp: document.querySelector("#mashTempOut"),
  mashDuration: document.querySelector("#mashDurationOut"),
  boilDuration: document.querySelector("#boilDurationOut"),
  fillVolume: document.querySelector("#fillVolumeOut"),
  stage: document.querySelector("#metricStage"),
  eta: document.querySelector("#metricEta"),
  duty: document.querySelector("#metricDuty"),
  confidence: document.querySelector("#metricConfidence"),
  chartTemp: document.querySelector("#chartTempLabel"),
  readoutStage: document.querySelector("#readoutStage"),
  readoutTemp: document.querySelector("#readoutTemp"),
  readoutVolume: document.querySelector("#readoutVolume")
};

const vesselCanvas = document.querySelector("#vesselCanvas");
const vesselCtx = vesselCanvas.getContext("2d");
const tempChart = document.querySelector("#tempChart");
const chartCtx = tempChart.getContext("2d");
const stateItems = Array.from(document.querySelectorAll("#stateList li"));

const stages = [
  { name: "Fill", duration: 12 },
  { name: "Strike", duration: 38 },
  { name: "Mash", duration: 60 },
  { name: "Boil", duration: 78 },
  { name: "Cooling", duration: 32 },
  { name: "Fermentation", duration: 42 }
];

let running = false;
let modelTime = 0;
let lastFrame = performance.now();
let series = [];

function params() {
  return {
    mashTemp: Number(inputs.mashTemp.value),
    mashDuration: Number(inputs.mashDuration.value),
    boilDuration: Number(inputs.boilDuration.value),
    fillVolume: Number(inputs.fillVolume.value)
  };
}

function syncOutputs() {
  const p = params();
  outputs.mashTemp.value = p.mashTemp.toFixed(1);
  outputs.mashDuration.value = p.mashDuration.toFixed(0);
  outputs.boilDuration.value = p.boilDuration.toFixed(0);
  outputs.fillVolume.value = p.fillVolume.toFixed(2);
  stages[2].duration = p.mashDuration;
  stages[3].duration = p.boilDuration + 18;
}

function totalDuration() {
  return stages.reduce((sum, stage) => sum + stage.duration, 0);
}

function currentStageAt(time) {
  let cursor = 0;
  for (const stage of stages) {
    if (time <= cursor + stage.duration) {
      return {
        ...stage,
        elapsed: time - cursor,
        progress: Math.max(0, Math.min(1, (time - cursor) / stage.duration))
      };
    }
    cursor += stage.duration;
  }
  return { ...stages[stages.length - 1], elapsed: stages.at(-1).duration, progress: 1 };
}

function modelAt(time) {
  const p = params();
  const stage = currentStageAt(time);
  const strikeTemp = p.mashTemp + 2;
  let temp = 20;
  let setpoint = 0;
  let volume = p.fillVolume;
  let duty = 0;
  let confidence = "Collecting data";

  if (stage.name === "Fill") {
    temp = 20 + stage.progress * 2;
    volume = p.fillVolume * stage.progress;
    duty = 0;
  } else if (stage.name === "Strike") {
    temp = 22 + (strikeTemp - 22) * (1 - Math.exp(-3.4 * stage.progress));
    setpoint = strikeTemp;
    duty = temp < strikeTemp - 0.25 ? 100 : 0;
    confidence = stage.progress > 0.35 ? "ETA locked" : "Learning heat rate";
  } else if (stage.name === "Mash") {
    temp = p.mashTemp + Math.sin(stage.progress * Math.PI * 8) * 0.35;
    setpoint = p.mashTemp;
    duty = 35 + Math.max(-12, Math.min(18, (p.mashTemp - temp) * 35));
    confidence = "Stable hold";
  } else if (stage.name === "Boil") {
    const heatProgress = Math.min(1, stage.progress / 0.24);
    temp = p.mashTemp + (100 - p.mashTemp) * (1 - Math.exp(-4 * heatProgress));
    if (stage.progress > 0.24) temp = 100 + Math.sin(stage.progress * Math.PI * 10) * 0.5;
    setpoint = 100;
    duty = stage.progress < 0.24 ? 100 : 72.5;
    volume = p.fillVolume - 0.45 * stage.progress;
    confidence = stage.progress > 0.24 ? "Boil confirmed" : "Estimating boil";
  } else if (stage.name === "Cooling") {
    temp = 23 + (100 - 23) * Math.exp(-4.2 * stage.progress);
    setpoint = 25;
    duty = 0;
    volume = p.fillVolume - 0.55;
    confidence = temp <= 28 ? "Pitch temp near" : "Cooling model active";
  } else {
    temp = 20 + Math.sin(stage.progress * Math.PI * 18) * 0.6;
    setpoint = 20;
    duty = 0;
    volume = p.fillVolume - 0.55;
    confidence = "Trend ready";
  }

  return {
    stage: stage.name,
    progress: stage.progress,
    temp,
    setpoint,
    volume,
    duty: Math.max(0, Math.min(100, duty)),
    confidence,
    remaining: Math.max(0, totalDuration() - time)
  };
}

function formatTime(minutes) {
  const m = Math.max(0, Math.round(minutes));
  const hours = Math.floor(m / 60);
  const mins = m % 60;
  return `${String(hours).padStart(2, "0")}:${String(mins).padStart(2, "0")}`;
}

function updateMetrics(sample) {
  outputs.stage.textContent = sample.stage;
  outputs.eta.textContent = formatTime(sample.remaining);
  outputs.duty.textContent = `${Math.round(sample.duty)}%`;
  outputs.confidence.textContent = sample.confidence;
  outputs.chartTemp.textContent = `${sample.temp.toFixed(1)} C`;
  outputs.readoutStage.textContent = sample.stage;
  outputs.readoutTemp.textContent = `${sample.temp.toFixed(1)} C`;
  outputs.readoutVolume.textContent = `${sample.volume.toFixed(2)} gal`;

  stateItems.forEach((item) => {
    item.classList.toggle("active", item.dataset.stage === sample.stage);
  });
}

function drawVessel(sample) {
  const w = vesselCanvas.width;
  const h = vesselCanvas.height;
  vesselCtx.clearRect(0, 0, w, h);

  vesselCtx.fillStyle = "rgba(255,255,255,0.08)";
  for (let i = 0; i < 9; i++) {
    vesselCtx.fillRect(i * 120 - ((modelTime * 4) % 120), 0, 2, h);
  }

  const tankX = w * 0.24;
  const tankY = h * 0.15;
  const tankW = w * 0.52;
  const tankH = h * 0.66;
  const fill = Math.max(0.08, Math.min(0.96, sample.volume / 8));

  vesselCtx.strokeStyle = "rgba(255,255,255,0.78)";
  vesselCtx.lineWidth = 8;
  vesselCtx.beginPath();
  vesselCtx.roundRect(tankX, tankY, tankW, tankH, 42);
  vesselCtx.stroke();

  const liquidH = tankH * fill;
  const liquidY = tankY + tankH - liquidH;
  const heat = Math.max(0, Math.min(1, (sample.temp - 20) / 80));
  const gradient = vesselCtx.createLinearGradient(0, liquidY, 0, tankY + tankH);
  gradient.addColorStop(0, `rgba(${70 + heat * 185}, ${145 - heat * 40}, ${170 - heat * 120}, 0.86)`);
  gradient.addColorStop(1, `rgba(${35 + heat * 190}, ${95 - heat * 25}, ${135 - heat * 80}, 0.95)`);
  vesselCtx.fillStyle = gradient;
  vesselCtx.beginPath();
  vesselCtx.roundRect(tankX + 10, liquidY, tankW - 20, liquidH - 10, 32);
  vesselCtx.fill();

  vesselCtx.strokeStyle = sample.duty > 0 ? "rgba(255, 205, 87, 0.95)" : "rgba(255,255,255,0.28)";
  vesselCtx.lineWidth = 10;
  vesselCtx.beginPath();
  vesselCtx.moveTo(tankX + tankW * 0.18, tankY + tankH + 38);
  for (let x = 0; x < tankW * 0.64; x += 34) {
    vesselCtx.lineTo(tankX + tankW * 0.18 + x, tankY + tankH + 38 + Math.sin(x / 18) * 16);
  }
  vesselCtx.stroke();

  vesselCtx.fillStyle = "rgba(255,255,255,0.82)";
  vesselCtx.font = "700 22px system-ui, sans-serif";
  vesselCtx.fillText("RTD", tankX + tankW + 32, tankY + 82);
  vesselCtx.fillText("Level sensor", tankX + tankW + 32, tankY + tankH - 68);

  vesselCtx.strokeStyle = "rgba(255,255,255,0.55)";
  vesselCtx.lineWidth = 3;
  vesselCtx.beginPath();
  vesselCtx.moveTo(tankX + tankW, tankY + 75);
  vesselCtx.lineTo(tankX + tankW + 24, tankY + 75);
  vesselCtx.moveTo(tankX + tankW, tankY + tankH - 75);
  vesselCtx.lineTo(tankX + tankW + 24, tankY + tankH - 75);
  vesselCtx.stroke();
}

function drawChart() {
  const w = tempChart.width;
  const h = tempChart.height;
  chartCtx.clearRect(0, 0, w, h);
  chartCtx.fillStyle = "#fbfcfa";
  chartCtx.fillRect(0, 0, w, h);

  chartCtx.strokeStyle = "#d7ded9";
  chartCtx.lineWidth = 1;
  for (let i = 0; i <= 5; i++) {
    const y = 28 + i * ((h - 60) / 5);
    chartCtx.beginPath();
    chartCtx.moveTo(44, y);
    chartCtx.lineTo(w - 20, y);
    chartCtx.stroke();
  }

  const points = series.slice(-180);
  if (points.length < 2) return;

  function toX(index) {
    return 44 + index * ((w - 70) / Math.max(1, points.length - 1));
  }

  function toY(temp) {
    const min = 15;
    const max = 105;
    return h - 32 - ((temp - min) / (max - min)) * (h - 64);
  }

  chartCtx.strokeStyle = "#b76031";
  chartCtx.lineWidth = 4;
  chartCtx.beginPath();
  points.forEach((point, index) => {
    const x = toX(index);
    const y = toY(point.temp);
    if (index === 0) chartCtx.moveTo(x, y);
    else chartCtx.lineTo(x, y);
  });
  chartCtx.stroke();

  chartCtx.strokeStyle = "#315f9b";
  chartCtx.lineWidth = 2;
  chartCtx.setLineDash([8, 8]);
  chartCtx.beginPath();
  points.forEach((point, index) => {
    const x = toX(index);
    const y = toY(point.setpoint || point.temp);
    if (index === 0) chartCtx.moveTo(x, y);
    else chartCtx.lineTo(x, y);
  });
  chartCtx.stroke();
  chartCtx.setLineDash([]);

  chartCtx.fillStyle = "#5b6661";
  chartCtx.font = "700 12px system-ui, sans-serif";
  chartCtx.fillText("105 C", 6, 34);
  chartCtx.fillText("15 C", 12, h - 28);
}

function tick(now) {
  const dt = Math.min(0.25, (now - lastFrame) / 1000);
  lastFrame = now;
  if (running) {
    modelTime += dt * 18;
    if (modelTime > totalDuration()) {
      running = false;
      modelTime = totalDuration();
    }
  }

  const sample = modelAt(modelTime);
  if (series.length === 0 || running) {
    series.push({ temp: sample.temp, setpoint: sample.setpoint });
  }
  updateMetrics(sample);
  drawVessel(sample);
  drawChart();
  requestAnimationFrame(tick);
}

document.querySelector("#runSimulation").addEventListener("click", () => {
  running = true;
  if (modelTime >= totalDuration()) {
    modelTime = 0;
    series = [];
  }
});

document.querySelector("#pauseSimulation").addEventListener("click", () => {
  running = false;
});

document.querySelector("#resetSimulation").addEventListener("click", () => {
  running = false;
  modelTime = 0;
  series = [];
});

Object.values(inputs).forEach((input) => {
  input.addEventListener("input", () => {
    syncOutputs();
    series = [];
  });
});

syncOutputs();
series.push({ temp: 20, setpoint: 0 });
requestAnimationFrame(tick);
