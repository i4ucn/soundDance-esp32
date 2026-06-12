#pragma once

#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>iot101 声音实验台</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #101114;
      --panel: #1b1d22;
      --line: #343841;
      --text: #f5f1e8;
      --muted: #a7a29a;
      --green: #25c2a0;
      --yellow: #f1b54a;
      --red: #ef6b73;
      --blue: #6aa8ff;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font: 15px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    main {
      width: min(1120px, calc(100vw - 28px));
      margin: 0 auto;
      padding: 18px 0 24px;
    }
    header.top {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 14px;
      margin-bottom: 12px;
    }
    h1 { margin: 0; font-size: clamp(22px, 4vw, 34px); letter-spacing: 0; }
    .status { color: var(--muted); display: flex; align-items: center; gap: 8px; }
    .dot { width: 10px; height: 10px; border-radius: 50%; background: var(--yellow); }
    .ok .dot { background: var(--green); }
    .bad .dot { background: var(--red); }
    .controls, .metrics, .grid { display: grid; gap: 12px; }
    .controls {
      grid-template-columns: auto auto minmax(180px, 1fr) minmax(130px, 160px);
      align-items: center;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      padding: 10px;
      margin-bottom: 12px;
    }
    button, input { font: inherit; }
    button {
      height: 38px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #242730;
      color: var(--text);
      padding: 0 14px;
      cursor: pointer;
    }
    button:hover { border-color: var(--green); }
    label { color: var(--muted); font-size: 13px; min-width: 0; }
    input[type="range"] { width: 100%; accent-color: var(--green); }
    input[type="number"] {
      width: 100%;
      height: 38px;
      margin-top: 4px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #121419;
      color: var(--text);
      padding: 0 9px;
    }
    .metrics { grid-template-columns: repeat(4, minmax(0, 1fr)); margin-bottom: 12px; }
    .metric, .panel {
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      overflow: hidden;
    }
    .metric { padding: 13px; min-height: 88px; }
    .metric span { display: block; color: var(--muted); font-size: 13px; margin-bottom: 8px; }
    .metric strong { font-size: clamp(25px, 5vw, 40px); line-height: 1; letter-spacing: 0; }
    .metric small { color: var(--muted); margin-left: 4px; }
    .grid { grid-template-columns: 1.3fr 1fr; }
    .wide { grid-column: 1 / -1; }
    .panel header {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      padding: 10px 13px;
      border-bottom: 1px solid var(--line);
    }
    .panel h2 { margin: 0; font-size: 15px; }
    .hint { color: var(--muted); font-size: 12px; text-align: right; }
    canvas { display: block; width: 100%; height: 255px; background: #14161b; }
    @media (max-width: 760px) {
      header.top, .controls, .metrics, .grid { grid-template-columns: 1fr; display: grid; }
      .status { justify-content: flex-start; }
    }
  </style>
</head>
<body>
  <main>
    <header class="top">
      <h1>iot101 声音实验台</h1>
      <div id="status" class="status"><span class="dot"></span><span id="statusText">连接中</span></div>
    </header>

    <section class="controls">
      <button id="pauseBtn" type="button">暂停</button>
      <button id="reconnectBtn" type="button">重连</button>
      <label>显示增益 <span id="gainValue">1.0x</span>
        <input id="gain" type="range" min="0.2" max="8" step="0.1" value="1">
      </label>
      <label>dB 校准偏移
        <input id="dbOffset" type="number" min="60" max="150" step="0.5" value="120">
      </label>
    </section>

    <section class="metrics">
      <div class="metric"><span>估算声级</span><strong id="dbSpl">--</strong><small>dB</small></div>
      <div class="metric"><span>数字强度</span><strong id="dbFs">--</strong><small>dBFS</small></div>
      <div class="metric"><span>RMS</span><strong id="rms">--</strong></div>
      <div class="metric"><span>主峰</span><strong id="peakHz">--</strong><small>Hz</small></div>
    </section>

    <section class="grid">
      <article class="panel">
        <header><h2>时域波形</h2><span id="waveHint" class="hint">--</span></header>
        <canvas id="wave"></canvas>
      </article>
      <article class="panel">
        <header><h2>频谱</h2><span id="spectrumHint" class="hint">--</span></header>
        <canvas id="spectrum"></canvas>
      </article>
      <article class="panel wide">
        <header><h2>声级走势</h2><span id="historyHint" class="hint">--</span></header>
        <canvas id="history"></canvas>
      </article>
    </section>
  </main>

  <script>
    const $ = (id) => document.getElementById(id);
    const state = {
      ws: null,
      paused: false,
      gain: 1,
      dbOffset: 120,
      history: [],
      frames: 0,
      lastFpsAt: performance.now()
    };

    function clamp(v, min, max) { return Math.max(min, Math.min(max, v)); }

    function setStatus(text, kind) {
      const el = $("status");
      $("statusText").textContent = text;
      el.className = "status" + (kind ? " " + kind : "");
    }

    function fit(canvas) {
      const rect = canvas.getBoundingClientRect();
      const dpr = Math.min(devicePixelRatio || 1, 2);
      const w = Math.max(1, Math.round(rect.width * dpr));
      const h = Math.max(1, Math.round(rect.height * dpr));
      if (canvas.width !== w || canvas.height !== h) {
        canvas.width = w;
        canvas.height = h;
      }
      return { w, h };
    }

    function grid(ctx, w, h) {
      ctx.strokeStyle = "#292d35";
      ctx.lineWidth = 1;
      ctx.beginPath();
      for (let i = 1; i < 4; i++) {
        const y = Math.round(h * i / 4) + 0.5;
        ctx.moveTo(0, y); ctx.lineTo(w, y);
      }
      for (let i = 1; i < 8; i++) {
        const x = Math.round(w * i / 8) + 0.5;
        ctx.moveTo(x, 0); ctx.lineTo(x, h);
      }
      ctx.stroke();
    }

    function clear(canvas) {
      const ctx = canvas.getContext("2d");
      const { w, h } = fit(canvas);
      ctx.fillStyle = "#14161b";
      ctx.fillRect(0, 0, w, h);
      grid(ctx, w, h);
      return { ctx, w, h };
    }

    function drawWave(samples) {
      const { ctx, w, h } = clear($("wave"));
      ctx.strokeStyle = "rgba(245,241,232,.22)";
      ctx.beginPath();
      ctx.moveTo(0, h / 2);
      ctx.lineTo(w, h / 2);
      ctx.stroke();

      const g = ctx.createLinearGradient(0, 0, w, 0);
      g.addColorStop(0, "#25c2a0");
      g.addColorStop(.55, "#f1b54a");
      g.addColorStop(1, "#ef6b73");
      ctx.strokeStyle = g;
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (let x = 0; x < w; x++) {
        const i = Math.floor(x / Math.max(1, w - 1) * (samples.length - 1));
        const y = h / 2 - clamp(samples[i], -1, 1) * h * 0.42;
        if (x === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    function spectrum(samples, sampleRate) {
      const bins = 64;
      const out = new Float32Array(bins);
      let peakHz = 0;
      let peakPower = 0;
      for (let b = 0; b < bins; b++) {
        const k = b + 1;
        let re = 0;
        let im = 0;
        for (let n = 0; n < samples.length; n++) {
          const win = 0.5 - 0.5 * Math.cos(2 * Math.PI * n / (samples.length - 1));
          const a = 2 * Math.PI * k * n / samples.length;
          re += samples[n] * win * Math.cos(a);
          im -= samples[n] * win * Math.sin(a);
        }
        const mag = Math.sqrt(re * re + im * im) / samples.length;
        out[b] = 20 * Math.log10(mag + 0.000001);
        if (mag > peakPower) {
          peakPower = mag;
          peakHz = k * sampleRate / samples.length;
        }
      }
      return { out, peakHz };
    }

    function drawSpectrum(values) {
      const { ctx, w, h } = clear($("spectrum"));
      const step = w / values.length;
      for (let i = 0; i < values.length; i++) {
        const norm = clamp((values[i] + 90) / 70, 0, 1);
        const bh = Math.max(1, norm * (h - 18));
        ctx.fillStyle = `hsl(${170 - norm * 120} 75% 56%)`;
        ctx.fillRect(i * step + 1, h - bh, Math.max(2, step - 2), bh);
      }
    }

    function drawHistory() {
      const { ctx, w, h } = clear($("history"));
      if (state.history.length < 2) return;
      ctx.strokeStyle = "#f1b54a";
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (let i = 0; i < state.history.length; i++) {
        const x = i / (state.history.length - 1) * w;
        const y = h - clamp((state.history[i] - 30) / 75, 0, 1) * (h - 18) - 9;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    function render(samples, meta) {
      if (state.paused) return;
      const dbSpl = meta.dbfs + state.dbOffset;
      state.history.push(dbSpl);
      if (state.history.length > 240) state.history.shift();

      const spec = spectrum(samples, meta.sampleRate);
      drawWave(samples);
      drawSpectrum(spec.out);
      drawHistory();

      $("dbSpl").textContent = dbSpl.toFixed(1);
      $("dbFs").textContent = meta.dbfs.toFixed(1);
      $("rms").textContent = meta.rms.toFixed(4);
      $("peakHz").textContent = Math.round(spec.peakHz);
      $("waveHint").textContent = `${samples.length} 点 · ${meta.sampleRate} Hz`;
      $("spectrumHint").textContent = `0-${Math.round(meta.sampleRate / 2)} Hz`;
      $("historyHint").textContent = `${state.history.length} 帧`;

      state.frames++;
      const now = performance.now();
      if (now - state.lastFpsAt > 1000) {
        setStatus(`${state.frames} fps`, "ok");
        state.frames = 0;
        state.lastFpsAt = now;
      }
    }

    function onFrame(buffer) {
      if (!(buffer instanceof ArrayBuffer) || buffer.byteLength < 28) return;
      const bytes = new Uint8Array(buffer);
      if (String.fromCharCode(bytes[0], bytes[1], bytes[2], bytes[3]) !== "AUD1") return;
      const view = new DataView(buffer);
      const count = view.getUint16(4, true);
      const sampleRate = view.getUint32(8, true);
      const rms = view.getFloat32(16, true);
      const dbfs = view.getFloat32(20, true);
      const offset = 28;
      if (buffer.byteLength < offset + count * 2) return;
      const samples = new Float32Array(count);
      for (let i = 0; i < count; i++) {
        samples[i] = view.getInt16(offset + i * 2, true) / 32768 * state.gain;
      }
      render(samples, { sampleRate, rms, dbfs });
    }

    function wsUrl() {
      const q = new URLSearchParams(location.search);
      const host = q.get("host") || location.hostname || "192.168.4.1";
      return `ws://${host}:81/`;
    }

    function connect() {
      if (state.ws) state.ws.close();
      setStatus("连接中", "");
      const ws = new WebSocket(wsUrl());
      ws.binaryType = "arraybuffer";
      state.ws = ws;
      ws.onopen = () => setStatus("已连接", "ok");
      ws.onmessage = (event) => onFrame(event.data);
      ws.onerror = () => setStatus("连接错误", "bad");
      ws.onclose = () => setTimeout(() => {
        if (state.ws === ws) connect();
      }, 1400);
    }

    $("pauseBtn").onclick = () => {
      state.paused = !state.paused;
      $("pauseBtn").textContent = state.paused ? "继续" : "暂停";
    };
    $("reconnectBtn").onclick = connect;
    $("gain").oninput = (event) => {
      state.gain = Number(event.target.value);
      $("gainValue").textContent = `${state.gain.toFixed(1)}x`;
    };
    $("dbOffset").oninput = (event) => {
      state.dbOffset = Number(event.target.value) || 120;
    };
    window.onresize = drawHistory;
    connect();
  </script>
</body>
</html>
)HTML";
