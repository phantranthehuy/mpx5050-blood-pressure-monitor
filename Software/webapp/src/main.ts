import './style.css';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';

import {
  LineAccumulator,
  parseUartLine,
  formatAbortCommand,
  formatStartCommand,
  formatTargetCommand,
  formatSafCommand,
  formatSafHighCommand,
  formatHighUartCommand,
  clampTargetMmhg,
  clampUartSafMmhg,
  SAF_DEFAULT_MMHG,
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
    btnStart: HTMLButtonElement;
    btnStop: HTMLButtonElement;
    manualT: HTMLInputElement;
    inflAmp: HTMLInputElement;
    inflMs: HTMLInputElement;
    margin: HTMLInputElement;
    rs: HTMLInputElement;
    rd: HTMLInputElement;
    fs: HTMLInputElement;
    peakMin: HTMLInputElement;
    cfgSaf: HTMLInputElement;
    cfgSafHigh: HTMLInputElement;
    chkHostHigh: HTMLInputElement;
    btnApplyCfg: HTMLButtonElement;
    earlyCut: HTMLInputElement;
    acQuietThr: HTMLInputElement;
    acQuietMs: HTMLInputElement;
    marginQuiet: HTMLInputElement;
    deflateRate: HTMLInputElement;
    chkEarlyEnd: HTMLInputElement;
    earlyMinPeaks: HTMLInputElement;
    earlyBelowDbp: HTMLInputElement;
    earlyStablePasses: HTMLInputElement;
    earlyStableTol: HTMLInputElement;
    uartStatus: HTMLParagraphElement;
    maaToast: HTMLDivElement;
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
    <button type="button" class="primary" id="btnStart" disabled>Bắt đầu đo</button>
    <button type="button" class="danger" id="btnStop" disabled>Dừng</button>
    <label class="target-inline">Mục tiêu T (mmHg) <input type="number" id="manualT" value="200" min="0" max="400" /></label>
    <label class="ha-inline"><input type="checkbox" id="chkHostHigh" /> Huyết áp cao (<code>HIGH,1</code>)</label>
  </div>
  <p class="small-note proto-note">
    «Bắt đầu đo» gửi <code>START</code> rồi <code>T,…</code> (cần firmware đã cập nhật). MCU trả <code>R,OK,…</code> khi đã parse lệnh.
    Chỉ gửi <code>T,…</code> khi đang IDLE thì chưa bơm — cần <code>START</code> hoặc nút START trên board.
  </p>
  <p class="cmd-hint" id="uartStatus" aria-live="polite">Chưa kết nối.</p>
  <fieldset class="settings" style="margin-top:0.75rem">
    <legend>MCU — UART (SAF / HA)</legend>
    <p class="small-note" style="margin:0 0 0.5rem">
      SAF / HA cũng được lưu cùng bộ cấu hình host (localStorage) khi đổi ô hoặc checkbox.
    </p>
    <label>SAF thường (mmHg) <input type="number" id="cfgSaf" min="120" max="300" step="1" value="175" /></label>
    <label>SAF chế độ HA (mmHg) <input type="number" id="cfgSafHigh" min="120" max="300" step="1" value="175" /></label>
    <button type="button" id="btnApplyCfg" disabled>Gửi SAF / SAFH / HIGH</button>
  </fieldset>
  <p class="small-note" id="annLine">Chưa kết nối.</p>

  <div class="grid-results">
    <div class="card-num"><div class="label">Tâm thu (SYS)</div><div class="value" id="vSys">—</div></div>
    <div class="card-num"><div class="label">Tâm trương (DIA)</div><div class="value" id="vDia">—</div></div>
    <div class="card-num"><div class="label">MAP</div><div class="value" id="vMap">—</div></div>
    <div class="card-num"><div class="label">Nhịp tim</div><div class="value" id="vBpm">—</div></div>
  </div>
  <div class="live-stream-stats" role="status" aria-live="polite">
    <div class="live-stat">
      <span class="live-stat-label">Target đã gửi</span>
      <span class="live-stat-value" id="sentT">—</span>
    </div>
    <div class="live-stat">
      <span class="live-stat-label">Đỉnh bao (đồ thị dưới)</span>
      <span class="live-stat-value" id="peaks">0</span>
    </div>
    <div class="live-stat live-stat--cuff">
      <span class="live-stat-label">Áp hiện tại</span>
      <span class="live-stat-value"><span id="cuff">—</span><span class="live-stat-unit"> mmHg</span></span>
    </div>
  </div>

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
    <p class="small-note" style="margin:0 0 0.5rem">
      Giá trị được lưu tự động trong trình duyệt (<code>localStorage</code>) khi bạn thay đổi; F5 hoặc mở lại trang vẫn giữ (cùng origin).
    </p>
    <label>Ngưỡng dao động bơm chậm <input type="number" id="inflAmp" step="0.1" min="0.2" value="1.2" /></label>
    <label>Duy trì (ms) <input type="number" id="inflMs" step="10" min="50" value="250" /></label>
    <label>Margin T (mmHg) <input type="number" id="margin" step="1" min="10" value="40" /></label>
    <label>r<sub>s</sub> <input type="number" id="rs" step="0.01" min="0.3" max="0.7" value="0.5" /></label>
    <label>r<sub>d</sub> <input type="number" id="rd" step="0.01" min="0.5" max="0.95" value="0.7" /></label>
    <label>F<sub>s</sub> lọc (Hz) <input type="number" id="fs" step="1" min="50" max="200" value="100" /></label>
    <label>Đỉnh tối thiểu (pha xả) <input type="number" id="peakMin" step="0.05" min="0.05" value="0.35" /></label>
    <label>Áp cắt sớm (mmHg, 0=tắt) <input type="number" id="earlyCut" min="0" max="250" step="1" value="0" /></label>
    <label>|AC| &quot;yên&quot; &lt; <input type="number" id="acQuietThr" step="0.05" min="0.05" value="0.35" /></label>
    <label>Duy trì yên (ms) <input type="number" id="acQuietMs" step="10" min="50" value="400" /></label>
    <label>Margin T khi cắt sớm <input type="number" id="marginQuiet" min="2" max="40" step="1" value="6" /></label>
    <label>Tốc độ xả chậm <code>DR</code> (mmHg/s, gửi khi vào pha xả) <input type="number" id="deflateRate" step="0.1" min="0.8" max="4" value="3" /></label>
    <label class="ha-inline"><input type="checkbox" id="chkEarlyEnd" /> Kết thúc sớm <code>EARLYEND</code> (đủ bao + cuff thấp)</label>
    <label>Đỉnh tối thiểu (sớm) <input type="number" id="earlyMinPeaks" min="5" max="40" step="1" value="6" /></label>
    <label>Biên dưới DBP (mmHg) <input type="number" id="earlyBelowDbp" min="5" max="50" step="1" value="15" title="Cuff DC phải ≤ DBP_MAA − giá trị này" /></label>
    <label>Ổn định MAA (lần, 0=tắt) <input type="number" id="earlyStablePasses" min="0" max="50" step="1" value="0" /></label>
    <label>Tol ổn định (mmHg) <input type="number" id="earlyStableTol" min="1" max="15" step="1" value="3" /></label>
  </fieldset>

  <details class="testing-panel">
    <summary><h2 style="display:inline">Checklist tham chiếu TESTING.md</h2></summary>
    <ul>
      <li><strong>COM-T01</strong> — Stream <code>S,…</code> ~100 dòng/giây khi đo (quan sát biểu đồ / log).</li>
      <li><strong>COM-T02</strong> — Sau <code>T,…</code> MCU báo <code>A,INFLATE_MARGIN</code> rồi xả đo.</li>
      <li><strong>SAF-T03</strong> — Gửi <code>T,300</code>: host + firmware clamp theo SAF đã gửi (mặc định 175 mmHg, nút “Bắt đầu đo”).</li>
      <li><strong>SAF-T04</strong> — Nút Dừng gửi <code>ABORT</code> (tương đương STOP phần cứng về xả).</li>
    </ul>
  </details>

  <h2 class="small-note" style="margin-top:1.25rem">Log UART</h2>
  <pre class="log-box" id="log" aria-live="polite"></pre>
  <div id="maaToast" class="maa-toast" role="alert" aria-live="assertive" hidden></div>
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
    btnStart: el<HTMLButtonElement>('btnStart'),
    btnStop: el<HTMLButtonElement>('btnStop'),
    manualT: el<HTMLInputElement>('manualT'),
    inflAmp: el<HTMLInputElement>('inflAmp'),
    inflMs: el<HTMLInputElement>('inflMs'),
    margin: el<HTMLInputElement>('margin'),
    rs: el<HTMLInputElement>('rs'),
    rd: el<HTMLInputElement>('rd'),
    fs: el<HTMLInputElement>('fs'),
    peakMin: el<HTMLInputElement>('peakMin'),
    cfgSaf: el<HTMLInputElement>('cfgSaf'),
    cfgSafHigh: el<HTMLInputElement>('cfgSafHigh'),
    chkHostHigh: el<HTMLInputElement>('chkHostHigh'),
    btnApplyCfg: el<HTMLButtonElement>('btnApplyCfg'),
    earlyCut: el<HTMLInputElement>('earlyCut'),
    acQuietThr: el<HTMLInputElement>('acQuietThr'),
    acQuietMs: el<HTMLInputElement>('acQuietMs'),
    marginQuiet: el<HTMLInputElement>('marginQuiet'),
    deflateRate: el<HTMLInputElement>('deflateRate'),
    chkEarlyEnd: el<HTMLInputElement>('chkEarlyEnd'),
    earlyMinPeaks: el<HTMLInputElement>('earlyMinPeaks'),
    earlyBelowDbp: el<HTMLInputElement>('earlyBelowDbp'),
    earlyStablePasses: el<HTMLInputElement>('earlyStablePasses'),
    earlyStableTol: el<HTMLInputElement>('earlyStableTol'),
    uartStatus: el<HTMLParagraphElement>('uartStatus'),
    maaToast: el<HTMLDivElement>('maaToast'),
  };

  const proc = new MeasurementProcessor();

  return { els, proc };
}

/** Lưu trong trình duyệt (localStorage): tham số xử lý, SAF/HIGH, T, hiển thị đồ thị. */
const HOST_SETTINGS_LS_KEY = 'mpx5050-webapp-host-settings-v1';
const HOST_SETTINGS_VER = 1 as const;

type HostFormEls = ReturnType<typeof mountUi>['els'];

type HostSettingsV1 = {
  v: typeof HOST_SETTINGS_VER;
  manualT: string;
  inflAmp: string;
  inflMs: string;
  margin: string;
  rs: string;
  rd: string;
  fs: string;
  peakMin: string;
  earlyCut: string;
  acQuietThr: string;
  acQuietMs: string;
  marginQuiet: string;
  deflateRate: string;
  chkEarlyEnd: boolean;
  earlyMinPeaks: string;
  earlyBelowDbp: string;
  earlyStablePasses: string;
  earlyStableTol: string;
  cfgSaf: string;
  cfgSafHigh: string;
  chkHostHigh: boolean;
  chkDc: boolean;
  chkAc: boolean;
};

function snapshotHostSettings(els: HostFormEls): HostSettingsV1 {
  return {
    v: HOST_SETTINGS_VER,
    manualT: els.manualT.value,
    inflAmp: els.inflAmp.value,
    inflMs: els.inflMs.value,
    margin: els.margin.value,
    rs: els.rs.value,
    rd: els.rd.value,
    fs: els.fs.value,
    peakMin: els.peakMin.value,
    earlyCut: els.earlyCut.value,
    acQuietThr: els.acQuietThr.value,
    acQuietMs: els.acQuietMs.value,
    marginQuiet: els.marginQuiet.value,
    deflateRate: els.deflateRate.value,
    chkEarlyEnd: els.chkEarlyEnd.checked,
    earlyMinPeaks: els.earlyMinPeaks.value,
    earlyBelowDbp: els.earlyBelowDbp.value,
    earlyStablePasses: els.earlyStablePasses.value,
    earlyStableTol: els.earlyStableTol.value,
    cfgSaf: els.cfgSaf.value,
    cfgSafHigh: els.cfgSafHigh.value,
    chkHostHigh: els.chkHostHigh.checked,
    chkDc: els.chkDc.checked,
    chkAc: els.chkAc.checked,
  };
}

function loadHostSettingsIntoForm(els: HostFormEls): void {
  try {
    const raw = localStorage.getItem(HOST_SETTINGS_LS_KEY);
    if (!raw) return;
    const s = JSON.parse(raw) as Partial<HostSettingsV1>;
    if (s.v !== HOST_SETTINGS_VER) return;
    if (typeof s.manualT === 'string') els.manualT.value = s.manualT;
    if (typeof s.inflAmp === 'string') els.inflAmp.value = s.inflAmp;
    if (typeof s.inflMs === 'string') els.inflMs.value = s.inflMs;
    if (typeof s.margin === 'string') els.margin.value = s.margin;
    if (typeof s.rs === 'string') els.rs.value = s.rs;
    if (typeof s.rd === 'string') els.rd.value = s.rd;
    if (typeof s.fs === 'string') els.fs.value = s.fs;
    if (typeof s.peakMin === 'string') els.peakMin.value = s.peakMin;
    if (typeof s.earlyCut === 'string') els.earlyCut.value = s.earlyCut;
    if (typeof s.acQuietThr === 'string') els.acQuietThr.value = s.acQuietThr;
    if (typeof s.acQuietMs === 'string') els.acQuietMs.value = s.acQuietMs;
    if (typeof s.marginQuiet === 'string') els.marginQuiet.value = s.marginQuiet;
    if (typeof s.deflateRate === 'string') els.deflateRate.value = s.deflateRate;
    if (typeof s.chkEarlyEnd === 'boolean') els.chkEarlyEnd.checked = s.chkEarlyEnd;
    if (typeof s.earlyMinPeaks === 'string') els.earlyMinPeaks.value = s.earlyMinPeaks;
    if (typeof s.earlyBelowDbp === 'string') els.earlyBelowDbp.value = s.earlyBelowDbp;
    if (typeof s.earlyStablePasses === 'string') els.earlyStablePasses.value = s.earlyStablePasses;
    if (typeof s.earlyStableTol === 'string') els.earlyStableTol.value = s.earlyStableTol;
    if (typeof s.cfgSaf === 'string') els.cfgSaf.value = s.cfgSaf;
    if (typeof s.cfgSafHigh === 'string') els.cfgSafHigh.value = s.cfgSafHigh;
    if (typeof s.chkHostHigh === 'boolean') els.chkHostHigh.checked = s.chkHostHigh;
    if (typeof s.chkDc === 'boolean') els.chkDc.checked = s.chkDc;
    if (typeof s.chkAc === 'boolean') els.chkAc.checked = s.chkAc;
  } catch {
    /* private mode / JSON lỗi */
  }
}

let hostSettingsSaveTimer: number | null = null;

function schedulePersistHostSettings(els: HostFormEls): void {
  if (hostSettingsSaveTimer !== null) window.clearTimeout(hostSettingsSaveTimer);
  hostSettingsSaveTimer = window.setTimeout(() => {
    hostSettingsSaveTimer = null;
    try {
      localStorage.setItem(HOST_SETTINGS_LS_KEY, JSON.stringify(snapshotHostSettings(els)));
    } catch {
      /* hết quota */
    }
  }, 350);
}

let maaToastDismissTimer = 0;
let maaToastHideTimer = 0;

function showMaaToast(el: HTMLDivElement, message: string): void {
  el.textContent = message;
  el.hidden = false;
  el.classList.add('maa-toast--visible');
  window.clearTimeout(maaToastDismissTimer);
  window.clearTimeout(maaToastHideTimer);
  maaToastDismissTimer = window.setTimeout(() => {
    el.classList.remove('maa-toast--visible');
    maaToastHideTimer = window.setTimeout(() => {
      el.hidden = true;
    }, 280);
  }, 9000);
}

function appendLog(pre: HTMLPreElement, line: string, maxLines = 80): void {
  const rows = (pre.textContent ?? '').split('\n').filter(Boolean);
  rows.push(line);
  while (rows.length > maxLines) rows.shift();
  pre.textContent = rows.join('\n');
  pre.scrollTop = pre.scrollHeight;
}

async function writeCmd(
  port: SerialPort,
  text: string,
  log: HTMLPreElement,
  statusEl?: HTMLParagraphElement,
): Promise<void> {
  const w = port.writable;
  if (!w) return;
  let writer: WritableStreamDefaultWriter<Uint8Array>;
  try {
    writer = w.getWriter();
  } catch (e) {
    appendLog(log, `[TX getWriter] ${String(e)}`);
    if (statusEl) statusEl.textContent = `Lỗi mở writer UART: ${String(e)}`;
    return;
  }
  try {
    await writer.write(new TextEncoder().encode(text));
    appendLog(log, `→ TX ${text.replace(/\n/g, '\\n')}`);
    if (statusEl) {
      const oneLine = text.replace(/\s+/g, ' ').trim();
      statusEl.textContent = `Đã gửi: ${oneLine} — chờ MCU (dòng R,OK,… trong log).`;
    }
  } catch (e) {
    appendLog(log, `[TX lỗi] ${String(e)}`);
    if (statusEl) statusEl.textContent = `Lỗi gửi UART: ${String(e)}`;
  } finally {
    try {
      writer.releaseLock();
    } catch {
      /* ignore */
    }
  }
}

function createLineHandler(ctx: {
  proc: MeasurementProcessor;
  port: () => SerialPort | null;
  log: HTMLPreElement;
  uartStatus: HTMLParagraphElement;
  onSample: (tMs: number, pMmHg: number) => void;
  clampSaf: () => number;
  /** Một writer Serial tại một thời điểm — xếp hàng mọi TX phát sinh từ dòng `S`. */
  enqueueSerialTx: (task: () => Promise<void>) => void;
}) {
  return function handleParsedLine(line: ParsedLine): void {
    if (line.tag === 'unknown' && line.raw) appendLog(ctx.log, line.raw);
    if (line.tag === 'R') {
      appendLog(ctx.log, `← R,${line.payload}`);
      ctx.uartStatus.textContent = `MCU xác nhận: R,${line.payload}`;
    }
    if (line.tag === 'A') {
      appendLog(ctx.log, `A,${line.code}`);
      ctx.proc.onAnnunciation(line.code);
    }
    if (line.tag === 'E') {
      appendLog(ctx.log, `E,${line.code}`);
      ctx.proc.onEvent(line.code);
    }

    if (line.tag === 'S') {
      if (line.fsm !== undefined) ctx.proc.syncPhaseFromMcuFsm(line.fsm);
      const r = ctx.proc.ingestSample(line.tMs, line.pMmHg);
      ctx.onSample(line.tMs, line.pMmHg);
      const p = ctx.port();
      if (p?.writable && (r.extraTx.length > 0 || r.targetMmHg !== null)) {
        const extra = r.extraTx.slice();
        const tgt = r.targetMmHg;
        const cap = ctx.clampSaf();
        ctx.enqueueSerialTx(async () => {
          const portNow = ctx.port();
          if (!portNow?.writable) return;
          for (const tx of extra) {
            await writeCmd(portNow, tx, ctx.log, ctx.uartStatus);
          }
          if (tgt !== null) {
            await writeCmd(portNow, formatTargetCommand(tgt, cap), ctx.log, ctx.uartStatus);
          }
        });
      }
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
  loadHostSettingsIntoForm(els);

  function effectiveSafFromForm(): number {
    const raw = els.chkHostHigh.checked ? Number(els.cfgSafHigh.value) : Number(els.cfgSaf.value);
    if (!Number.isFinite(raw)) return SAF_DEFAULT_MMHG;
    return clampUartSafMmhg(raw);
  }

  async function pushMcuUartConfig(p: SerialPort): Promise<void> {
    await uartQueueFlush();
    enqueueSerialTx(async () => {
      if (!p.writable) return;
      await writeCmd(p, formatSafCommand(Number(els.cfgSaf.value)), els.log, els.uartStatus);
      await writeCmd(p, formatSafHighCommand(Number(els.cfgSafHigh.value)), els.log, els.uartStatus);
      await writeCmd(p, formatHighUartCommand(els.chkHostHigh.checked), els.log, els.uartStatus);
    });
    await uartQueueFlush();
  }

  let port: SerialPort | null = null;
  /** Tuần tự hóa mọi ghi Web Serial — tránh nhiều `getWriter()` đồng thời (MCU đầy buffer → treo). */
  let uartTxChain: Promise<void> = Promise.resolve();

  function enqueueSerialTx(task: () => Promise<void>): void {
    uartTxChain = uartTxChain.then(task).catch((e) => {
      appendLog(els.log, `[UART TX hàng đợi] ${String(e)}`);
    });
  }

  async function uartQueueFlush(): Promise<void> {
    await uartTxChain.catch(() => {});
  }

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
    schedulePersistHostSettings(els);
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
          // uPlot Axis.Side: Top=0, Right=1, Bottom=2, Left=3 (not the common 0=bottom convention).
          axes: [
            { stroke: '#8b93a5', grid: { stroke: '#2a3140' }, label: 's' },
            {
              stroke: '#8b93a5',
              grid: { stroke: '#2a3140' },
              scale: 'y',
              label: 'mmHg',
              side: 3,
            },
            {
              stroke: '#c9a227',
              grid: { show: false },
              scale: 'ac',
              label: '|AC|',
              side: 1,
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
        [[175, 40], [0, 0]],
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
      xp = [175, 40];
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
    ysDc.push(proc.dcMmHg);
    ysAc.push(Math.abs(proc.bpFiltered));
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
    uartStatus: els.uartStatus,
    onSample: pushSample,
    clampSaf: effectiveSafFromForm,
    enqueueSerialTx,
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
      effectiveSafMmHg: effectiveSafFromForm(),
      earlyCutoffMmHg: Number(els.earlyCut.value),
      acQuietThreshold: Number(els.acQuietThr.value),
      acQuietMs: Number(els.acQuietMs.value),
      marginSmallQuietMmHg: Number(els.marginQuiet.value),
      deflateSlowRateMmHgS: Number(els.deflateRate.value),
      earlyEndEnabled: els.chkEarlyEnd.checked,
      earlyEndMinPeaks: Number(els.earlyMinPeaks.value),
      earlyEndBelowDbpMmHg: Number(els.earlyBelowDbp.value),
      earlyEndStablePasses: Number(els.earlyStablePasses.value),
      earlyEndStableTolMmHg: Number(els.earlyStableTol.value),
    });
    schedulePersistHostSettings(els);
  }

  for (const el of [
    els.inflAmp,
    els.inflMs,
    els.margin,
    els.rs,
    els.rd,
    els.fs,
    els.peakMin,
    els.cfgSaf,
    els.cfgSafHigh,
    els.chkHostHigh,
    els.earlyCut,
    els.acQuietThr,
    els.acQuietMs,
    els.marginQuiet,
    els.deflateRate,
    els.chkEarlyEnd,
    els.earlyMinPeaks,
    els.earlyBelowDbp,
    els.earlyStablePasses,
    els.earlyStableTol,
  ]) {
    el.addEventListener('change', applySettingsFromForm);
  }

  els.manualT.addEventListener('change', () => schedulePersistHostSettings(els));

  els.chkHostHigh.addEventListener('change', async () => {
    if (!port?.writable) return;
    try {
      await pushMcuUartConfig(port);
    } catch (e) {
      appendLog(els.log, `[Cảnh báo gửi HIGH/SAF] ${String(e)}`);
    }
  });

  els.chkDc.addEventListener('change', applySeriesVisibility);
  els.chkAc.addEventListener('change', applySeriesVisibility);

  applySettingsFromForm();

  let lastMaaToastNonce = 0;

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

    if (s.maaFailureNonce > lastMaaToastNonce && s.maaFailureMessage) {
      lastMaaToastNonce = s.maaFailureNonce;
      showMaaToast(els.maaToast, s.maaFailureMessage);
    }

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
      await uartQueueFlush();
    } catch {
      /* ignore */
    }
    uartTxChain = Promise.resolve();
    try {
      await port?.close();
    } catch {
      /* ignore */
    }
    port = null;
    els.btnConn.disabled = false;
    els.btnStart.disabled = true;
    els.btnStop.disabled = true;
    els.btnApplyCfg.disabled = true;
    els.btnConn.textContent = 'Kết nối cổng USB TTL';
    appendLog(els.log, '[Đã ngắt kết nối]');
    els.uartStatus.textContent = 'Đã ngắt kết nối.';
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
    els.btnStart.disabled = false;
    els.btnStop.disabled = false;
    els.btnApplyCfg.disabled = false;
    appendLog(els.log, '[Đã kết nối 115200 8N1]');
    els.uartStatus.textContent = 'Đã kết nối — log dưới cùng: → TX và ← R,OK,…';

    try {
      await pushMcuUartConfig(port);
    } catch (e) {
      appendLog(els.log, `[Cảnh báo gửi SAF/HIGH] ${String(e)}`);
    }

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

  els.btnStop.addEventListener('click', async () => {
    if (!port?.writable) return;
    await uartQueueFlush();
    enqueueSerialTx(async () => {
      const p = port;
      if (!p?.writable) return;
      await writeCmd(p, formatAbortCommand(), els.log, els.uartStatus);
    });
  });

  els.btnStart.addEventListener('click', async () => {
    if (!port?.writable) return;
    const raw = Number(els.manualT.value);
    const cap = effectiveSafFromForm();
    const clamped = clampTargetMmhg(raw, cap);
    await uartQueueFlush();
    enqueueSerialTx(async () => {
      const p = port;
      if (!p?.writable) return;
      await writeCmd(p, formatStartCommand(), els.log, els.uartStatus);
      await writeCmd(p, formatTargetCommand(raw, cap), els.log, els.uartStatus);
    });
    if (raw !== clamped) appendLog(els.log, `[SAF] clamp host → ${clamped} mmHg`);
  });

  els.btnApplyCfg.addEventListener('click', async () => {
    if (!port?.writable) return;
    await pushMcuUartConfig(port);
    applySettingsFromForm();
  });

  resizeOscPlot();
  refreshOscPlot();
}

main();
