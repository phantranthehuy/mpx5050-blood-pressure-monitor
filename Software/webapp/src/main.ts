import './style.css';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';

import {
  LineAccumulator,
  parseUartLine,
  formatAbortCommand,
  formatTargetCommand,
  clampTargetMmhg,
  type ParsedLine,
} from './protocol';
import { MeasurementProcessor, type PhaseUi } from './dsp';

const ROLL_SAMPLES = 2500;

/** Vertical markers on oscillometric chart (mmHg cuff axis). */
const oscMarkersRef = {
  sys: null as number | null,
  map: null as number | null,
  dia: null as number | null,
};

function phaseBadgeClass(phase: PhaseUi): string {
  switch (phase) {
    case 'idle':
      return 'badge idle';
    case 'inflate_slow':
      return 'badge inflate';
    case 'inflate_margin':
      return 'badge margin';
    case 'deflate':
      return 'badge deflate';
    case 'fast_deflate':
      return 'badge fast';
    case 'meas_end':
      return 'badge done';
    case 'error':
      return 'badge err';
    default:
      return 'badge idle';
  }
}

function phaseLabelVi(phase: PhaseUi): string {
  const map: Record<PhaseUi, string> = {
    idle: 'Chờ (IDLE)',
    inflate_slow: 'Bơm chậm — nghe dao động',
    inflate_margin: 'Ramp tới target',
    deflate: 'Xả chậm — đo',
    fast_deflate: 'Xả nhanh',
    meas_end: 'Kết thúc đo',
    error: 'Lỗi cảm biến / hở khí',
  };
  return map[phase];
}

function mountUi(): {
  els: {
    phaseBadge: HTMLSpanElement;
    annLine: HTMLSpanElement;
    sys: HTMLSpanElement;
    dia: HTMLSpanElement;
    map: HTMLSpanElement;
    bpm: HTMLSpanElement;
    sentT: HTMLSpanElement;
    peaks: HTMLSpanElement;
    cuff: HTMLSpanElement;
    log: HTMLPreElement;
    chartHost: HTMLDivElement;
    chartOscHost: HTMLDivElement;
    chkDc: HTMLInputElement;
    chkAc: HTMLInputElement;
    oscLegend: HTMLParagraphElement;
    btnConn: HTMLButtonElement;
    btnAbort: HTMLButtonElement;
    btnSendT: HTMLButtonElement;
    manualT: HTMLInputElement;
    inflAmp: HTMLInputElement;
    inflMs: HTMLInputElement;
    margin: HTMLInputElement;
    rs: HTMLInputElement;
    rd: HTMLInputElement;
    fs: HTMLInputElement;
    peakMin: HTMLInputElement;
  };
  proc: MeasurementProcessor;
} {
  const root = document.querySelector<HTMLDivElement>('#app');
  if (!root) throw new Error('#app missing');

  root.innerHTML = `
  <header class="top">
    <h1>Máy đo huyết áp — Web Serial</h1>
    <span id="phaseBadge" class="badge idle">Chờ</span>
  </header>
  <p class="small-note">
    Yêu cầu Chrome hoặc Edge; chạy qua <code>http://localhost</code> hoặc HTTPS.
    Giao thức: <a href="../pc-bridge.md" target="_blank" rel="noreferrer">pc-bridge.md</a>,
    thuật toán: <a href="../algorithm.md" target="_blank" rel="noreferrer">algorithm.md</a>.
  </p>
  <div class="row-controls">
    <button type="button" class="primary" id="btnConn">Kết nối cổng USB TTL</button>
    <button type="button" class="danger" id="btnAbort" disabled>Hủy đo (ABORT)</button>
    <label>SAF test <input type="number" id="manualT" value="300" min="0" max="400" /></label>
    <button type="button" id="btnSendT" disabled>Gửi T,…</button>
  </div>
  <p class="small-note" id="annLine">Chưa kết nối.</p>

  <div class="grid-results">
    <div class="card-num"><div class="label">Tâm thu (SYS)</div><div class="value" id="vSys">—</div></div>
    <div class="card-num"><div class="label">Tâm trương (DIA)</div><div class="value" id="vDia">—</div></div>
    <div class="card-num"><div class="label">MAP</div><div class="value" id="vMap">—</div></div>
    <div class="card-num"><div class="label">Nhịp tim</div><div class="value" id="vBpm">—</div></div>
  </div>
  <p class="small-note">
    Target đã gửi: <span id="sentT">—</span> · Đỉnh bao (đồ thị dưới): <span id="peaks">0</span> ·
    Áp hiện tại: <span id="cuff">—</span> mmHg
  </p>

  <p class="chart-caption">Áp suất vòng bít theo thời gian</p>
  <div class="chart-toolbar">
    <label><input type="checkbox" id="chkDc" /> DC làm mượt</label>
    <label><input type="checkbox" id="chkAc" /> |AC| sau band-pass (trục phải)</label>
  </div>
  <div class="chart-wrap"><div id="chartHost"></div></div>

  <p class="chart-caption">Đồ thị oscillometric — đường bao dao động theo áp cuff</p>
  <p class="small-note" id="oscLegend"></p>
  <div class="chart-wrap"><div id="chartOscHost"></div></div>

  <fieldset class="settings">
    <legend>Tham số xử lý (host)</legend>
    <label>Ngưỡng dao động bơm chậm <input type="number" id="inflAmp" step="0.1" min="0.2" value="1.2" /></label>
    <label>Duy trì (ms) <input type="number" id="inflMs" step="10" min="50" value="250" /></label>
    <label>Margin T (mmHg) <input type="number" id="margin" step="1" min="10" value="40" /></label>
    <label>r<sub>s</sub> <input type="number" id="rs" step="0.01" min="0.3" max="0.7" value="0.5" /></label>
    <label>r<sub>d</sub> <input type="number" id="rd" step="0.01" min="0.5" max="0.95" value="0.7" /></label>
    <label>F<sub>s</sub> lọc (Hz) <input type="number" id="fs" step="1" min="50" max="200" value="100" /></label>
    <label>Đỉnh tối thiểu (pha xả) <input type="number" id="peakMin" step="0.05" min="0.05" value="0.35" /></label>
  </fieldset>

  <details class="testing-panel">
    <summary><h2 style="display:inline">Checklist tham chiếu TESTING.md</h2></summary>
    <ul>
      <li><strong>COM-T01</strong> — Stream <code>S,…</code> ~100 dòng/giây khi đo (quan sát biểu đồ / log).</li>
      <li><strong>COM-T02</strong> — Sau <code>T,…</code> MCU báo <code>A,INFLATE_MARGIN</code> rồi xả đo.</li>
      <li><strong>SAF-T03</strong> — Gửi <code>T,300</code>: firmware clamp 280 mmHg (nút “Gửi T,…” phía trên).</li>
      <li><strong>SAF-T04</strong> — Nút Hủy gửi <code>ABORT</code> (tương đương STOP phần cứng về xả).</li>
    </ul>
  </details>

  <h2 class="small-note" style="margin-top:1.25rem">Log UART</h2>
  <pre class="log-box" id="log"></pre>
  `;

  const el = <T extends HTMLElement>(id: string) => {
    const x = document.getElementById(id);
    if (!x) throw new Error(`#${id} missing`);
    return x as T;
  };

  const els = {
    phaseBadge: el<HTMLSpanElement>('phaseBadge'),
    annLine: el<HTMLSpanElement>('annLine'),
    sys: el<HTMLSpanElement>('vSys'),
    dia: el<HTMLSpanElement>('vDia'),
    map: el<HTMLSpanElement>('vMap'),
    bpm: el<HTMLSpanElement>('vBpm'),
    sentT: el<HTMLSpanElement>('sentT'),
    peaks: el<HTMLSpanElement>('peaks'),
    cuff: el<HTMLSpanElement>('cuff'),
    log: el<HTMLPreElement>('log'),
    chartHost: el<HTMLDivElement>('chartHost'),
    chartOscHost: el<HTMLDivElement>('chartOscHost'),
    chkDc: el<HTMLInputElement>('chkDc'),
    chkAc: el<HTMLInputElement>('chkAc'),
    oscLegend: el<HTMLParagraphElement>('oscLegend'),
    btnConn: el<HTMLButtonElement>('btnConn'),
    btnAbort: el<HTMLButtonElement>('btnAbort'),
    btnSendT: el<HTMLButtonElement>('btnSendT'),
    manualT: el<HTMLInputElement>('manualT'),
    inflAmp: el<HTMLInputElement>('inflAmp'),
    inflMs: el<HTMLInputElement>('inflMs'),
    margin: el<HTMLInputElement>('margin'),
    rs: el<HTMLInputElement>('rs'),
    rd: el<HTMLInputElement>('rd'),
    fs: el<HTMLInputElement>('fs'),
    peakMin: el<HTMLInputElement>('peakMin'),
  };

  const proc = new MeasurementProcessor();

  return { els, proc };
}

function appendLog(pre: HTMLPreElement, line: string, maxLines = 80): void {
  const rows = (pre.textContent ?? '').split('\n').filter(Boolean);
  rows.push(line);
  while (rows.length > maxLines) rows.shift();
  pre.textContent = rows.join('\n');
}

async function writeCmd(port: SerialPort, text: string, log: HTMLPreElement): Promise<void> {
  const w = port.writable;
  if (!w) return;
  const writer = w.getWriter();
  try {
    await writer.write(new TextEncoder().encode(text));
    appendLog(log, `→ TX ${text.replace(/\n/g, '\\n')}`);
  } finally {
    writer.releaseLock();
  }
}

function createLineHandler(ctx: {
  proc: MeasurementProcessor;
  port: () => SerialPort | null;
  log: HTMLPreElement;
  onSample: (tMs: number, pMmHg: number) => void;
}) {
  return function handleParsedLine(line: ParsedLine): void {
    if (line.tag === 'unknown' && line.raw) appendLog(ctx.log, line.raw);
    if (line.tag === 'A') {
      appendLog(ctx.log, `A,${line.code}`);
      ctx.proc.onAnnunciation(line.code);
    }
    if (line.tag === 'E') {
      appendLog(ctx.log, `E,${line.code}`);
      ctx.proc.onEvent(line.code);
    }

    if (line.tag === 'S') {
      const tgt = ctx.proc.ingestSample(line.tMs, line.pMmHg);
      ctx.onSample(line.tMs, line.pMmHg);
      const p = ctx.port();
      if (tgt !== null && p?.writable) void writeCmd(p, formatTargetCommand(tgt), ctx.log);
    }
  };
}

async function readLoop(
  port: SerialPort,
  handleLine: (line: string) => void,
  onDone: () => void,
): Promise<void> {
  const reader = port.readable?.getReader();
  if (!reader) return;
  const dec = new TextDecoder();
  const acc = new LineAccumulator();

  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      const chunk = dec.decode(value, { stream: true });
      for (const ln of acc.push(chunk)) {
        handleLine(ln);
      }
    }
  } finally {
    reader.releaseLock();
    acc.reset();
    onDone();
  }
}

function drawOscBpMarkers(u: uPlot): void {
  const ctx = u.ctx;
  const { left, top, width, height } = u.bbox;

  const drawAt = (mmHg: number | null, color: string) => {
    if (mmHg === null || !Number.isFinite(mmHg)) return;
    const px = u.valToPos(mmHg, 'x', true);
    if (!Number.isFinite(px)) return;
    if (px < left || px > left + width) return;
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.25;
    ctx.setLineDash([5, 4]);
    ctx.beginPath();
    ctx.moveTo(px, top);
    ctx.lineTo(px, top + height);
    ctx.stroke();
    ctx.restore();
  };

  drawAt(oscMarkersRef.sys, '#f87171');
  drawAt(oscMarkersRef.map, '#fbbf24');
  drawAt(oscMarkersRef.dia, '#34d399');
}

function main(): void {
  const { els, proc } = mountUi();

  let port: SerialPort | null = null;

  const xs: number[] = [];
  const ys: number[] = [];
  const ysDc: number[] = [];
  const ysAc: number[] = [];
  let t0Ms: number | null = null;
  let plotHandle: uPlot | null = null;
  let oscPlotHandle: uPlot | null = null;

  function applySeriesVisibility(): void {
    if (!plotHandle) return;
    plotHandle.setSeries(2, { show: els.chkDc.checked }, false);
    plotHandle.setSeries(3, { show: els.chkAc.checked }, false);
    plotHandle.redraw();
  }

  function resizePlot(): void {
    const w = Math.min(els.chartHost.clientWidth || 800, 1000);
    const h = 260;
    if (!plotHandle) {
      plotHandle = new uPlot(
        {
          width: w,
          height: h,
          scales: {
            x: { time: false },
            y: {
              range: (_self, dataMin, dataMax, _key) => [
                Math.floor(dataMin - 5),
                Math.ceil(dataMax + 5),
              ],
            },
            ac: {
              range: (_self, dataMin, dataMax, _key) => [
                Math.min(0, Math.floor(dataMin * 1.1)),
                Math.ceil(dataMax * 1.15 + 0.5),
              ],
            },
          },
          axes: [
            { stroke: '#8b93a5', grid: { stroke: '#2a3140' }, label: 's' },
            {
              stroke: '#8b93a5',
              grid: { stroke: '#2a3140' },
              scale: 'y',
              label: 'mmHg',
              side: 2,
            },
            {
              stroke: '#c9a227',
              grid: { show: false },
              scale: 'ac',
              label: '|AC|',
              side: 3,
            },
          ],
          series: [
            {},
            { label: 'Cuff', stroke: '#7cb8ff', width: 1.5, scale: 'y' },
            {
              label: 'DC cuff',
              stroke: '#93c5fd',
              width: 1,
              dash: [6, 4],
              scale: 'y',
              points: { show: false },
              show: false,
            },
            {
              label: '|AC|',
              stroke: '#fbbf24',
              width: 1,
              scale: 'ac',
              points: { show: false },
              show: false,
            },
          ],
        },
        [xs, ys, ysDc, ysAc],
        els.chartHost,
      );
      applySeriesVisibility();
    } else {
      plotHandle.setSize({ width: w, height: h });
    }
  }

  function resizeOscPlot(): void {
    const w = Math.min(els.chartOscHost.clientWidth || 800, 1000);
    const h = 260;
    if (!oscPlotHandle) {
      oscPlotHandle = new uPlot(
        {
          width: w,
          height: h,
          scales: {
            x: {
              time: false,
              dir: -1,
              range: (_self, dataMin, dataMax, _key) => [
                Math.floor(dataMin - 8),
                Math.ceil(dataMax + 8),
              ],
            },
            y: {
              range: (_self, dataMin, dataMax, _key) => [
                Math.max(0, Math.floor(dataMin - 0.25)),
                Math.ceil(dataMax + 0.35),
              ],
            },
          },
          axes: [
            {
              stroke: '#8b93a5',
              grid: { stroke: '#2a3140' },
              label: 'Áp cuff (mmHg, cao → thấp)',
            },
            { stroke: '#8b93a5', grid: { stroke: '#2a3140' }, label: 'Biên độ bao' },
          ],
          series: [{ label: 'p_cuff' }, { label: 'Đường bao', stroke: '#a78bfa', width: 1.75 }],
          hooks: {
            draw: [
              (u) => {
                drawOscBpMarkers(u);
              },
            ],
          },
        },
        [[280, 40], [0, 0]],
        els.chartOscHost,
      );
    } else {
      oscPlotHandle.setSize({ width: w, height: h });
    }
  }

  function refreshOscPlot(): void {
    const s = proc.snapshot();
    oscMarkersRef.sys = s.sys;
    oscMarkersRef.map = s.map;
    oscMarkersRef.dia = s.dia;

    const sorted = [...s.oscEnvelopePeaks].sort((a, b) => b.pCuff - a.pCuff);
    let xp = sorted.map((p) => p.pCuff);
    let yp = sorted.map((p) => p.amplitude);

    if (xp.length === 1) {
      xp = [xp[0], xp[0]];
      yp = [yp[0], yp[0]];
    }
    if (xp.length === 0) {
      xp = [280, 40];
      yp = [0, 0];
    }

    if (!oscPlotHandle) resizeOscPlot();
    oscPlotHandle?.setData([xp, yp]);

    const parts: string[] = [];
    if (s.sys !== null)
      parts.push('<span class="osc-tag sys">SYS ' + s.sys + '</span>');
    if (s.map !== null)
      parts.push('<span class="osc-tag map">MAP ' + s.map + '</span>');
    if (s.dia !== null)
      parts.push('<span class="osc-tag dia">DBP ' + s.dia + '</span>');
    els.oscLegend.innerHTML =
      parts.length > 0
        ? 'Vạch nét: ' + parts.join(' ') + ' (mmHg cuff)'
        : 'Vạch MAP/SYS/DBP hiển thị sau khi có kết quả đo.';
  }

  window.addEventListener('resize', () => {
    resizePlot();
    resizeOscPlot();
    plotHandle?.setData([xs, ys, ysDc, ysAc]);
    refreshOscPlot();
  });

  let rafPending = false;
  function scheduleRedraw(): void {
    if (rafPending) return;
    rafPending = true;
    requestAnimationFrame(() => {
      rafPending = false;
      plotHandle?.setData([xs, ys, ysDc, ysAc]);
    });
  }

  function pushSample(_tMs: number, p: number): void {
    if (t0Ms === null) t0Ms = _tMs;
    const x = (_tMs - t0Ms) / 1000;
    xs.push(x);
    ys.push(p);
    const snap = proc.snapshot();
    ysDc.push(snap.dcMmHg);
    ysAc.push(Math.abs(snap.bpFiltered));
    while (xs.length > ROLL_SAMPLES) {
      xs.shift();
      ys.shift();
      ysDc.shift();
      ysAc.shift();
    }
    scheduleRedraw();
  }

  const handleParsedLine = createLineHandler({
    proc,
    port: () => port,
    log: els.log,
    onSample: pushSample,
  });

  function applySettingsFromForm(): void {
    proc.setParams({
      inflateAmpThreshold: Number(els.inflAmp.value),
      inflateSustainMs: Number(els.inflMs.value),
      marginMmHg: Number(els.margin.value),
      rs: Number(els.rs.value),
      rd: Number(els.rd.value),
      sampleFs: Number(els.fs.value),
      peakMinAmp: Number(els.peakMin.value),
    });
  }

  for (const el of [
    els.inflAmp,
    els.inflMs,
    els.margin,
    els.rs,
    els.rd,
    els.fs,
    els.peakMin,
  ]) {
    el.addEventListener('change', applySettingsFromForm);
  }

  els.chkDc.addEventListener('change', applySeriesVisibility);
  els.chkAc.addEventListener('change', applySeriesVisibility);

  applySettingsFromForm();

  function refreshLabels(): void {
    const s = proc.snapshot();
    els.phaseBadge.className = phaseBadgeClass(s.phase);
    els.phaseBadge.textContent = phaseLabelVi(s.phase);
    els.annLine.textContent =
      s.lastAnnunciation || s.lastEvent
        ? `MCU: A,${s.lastAnnunciation || '—'} · E,${s.lastEvent || '—'}`
        : 'Đang chờ thông báo A/E…';

    els.sys.textContent = s.sys !== null ? `${s.sys}` : '—';
    els.dia.textContent = s.dia !== null ? `${s.dia}` : '—';
    els.map.textContent = s.map !== null ? `${s.map}` : '—';
    els.bpm.textContent = s.bpm !== null ? `${s.bpm}` : '—';
    els.sentT.textContent = s.sentTargetMmHg !== null ? `${s.sentTargetMmHg}` : '—';
    els.peaks.textContent = `${s.peaksCaptured}`;
    els.cuff.textContent = s.cuffMmHg.toFixed(1);

    refreshOscPlot();
  }

  let pollId = 0;
  function startUiPoll(): void {
    pollId = window.setInterval(refreshLabels, 120);
  }
  function stopUiPoll(): void {
    clearInterval(pollId);
  }

  async function disconnect(): Promise<void> {
    stopUiPoll();
    try {
      await port?.close();
    } catch {
      /* ignore */
    }
    port = null;
    els.btnConn.disabled = false;
    els.btnAbort.disabled = true;
    els.btnSendT.disabled = true;
    els.btnConn.textContent = 'Kết nối cổng USB TTL';
    appendLog(els.log, '[Đã ngắt kết nối]');
  }

  els.btnConn.addEventListener('click', async () => {
    const serial = navigator.serial;
    if (!serial) {
      alert('Trình duyệt không hỗ trợ Web Serial API.');
      return;
    }

    if (port) {
      await disconnect();
      return;
    }

    try {
      port = await serial.requestPort({});
      await port.open({ baudRate: 115200 });
    } catch (e) {
      appendLog(els.log, `[Lỗi mở cổng] ${String(e)}`);
      port = null;
      return;
    }

    t0Ms = null;
    xs.length = 0;
    ys.length = 0;
    ysDc.length = 0;
    ysAc.length = 0;
    proc.resetCycle();
    proc.resetFilters();
    applySettingsFromForm();

    els.btnConn.textContent = 'Ngắt kết nối';
    els.btnAbort.disabled = false;
    els.btnSendT.disabled = false;
    appendLog(els.log, '[Đã kết nối 115200 8N1]');

    resizePlot();
    resizeOscPlot();
    plotHandle?.setData([xs, ys, ysDc, ysAc]);
    refreshOscPlot();
    startUiPoll();

    void readLoop(
      port,
      (ln) => handleParsedLine(parseUartLine(ln)),
      () => {
        appendLog(els.log, '[Đọc serial kết thúc]');
        void disconnect();
      },
    );

    port.addEventListener('disconnect', () => {
      void disconnect();
    });
  });

  els.btnAbort.addEventListener('click', async () => {
    if (!port?.writable) return;
    await writeCmd(port, formatAbortCommand(), els.log);
  });

  els.btnSendT.addEventListener('click', async () => {
    if (!port?.writable) return;
    const raw = Number(els.manualT.value);
    const clamped = clampTargetMmhg(raw);
    await writeCmd(port, formatTargetCommand(raw), els.log);
    if (raw !== clamped) appendLog(els.log, `[SAF] clamp host → ${clamped} mmHg`);
  });

  resizeOscPlot();
  refreshOscPlot();
}

main();
