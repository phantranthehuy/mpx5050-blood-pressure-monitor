import { formatDeflateRateCommand, formatEarlyEndCommand } from './protocol';

/** Butterworth biquad coefficients (normalized). */

export type BiquadCoeffs = { b0: number; b1: number; b2: number; a1: number; a2: number };

function lowpassCoeffs(fc: number, fs: number, Q = 1 / Math.SQRT2): BiquadCoeffs {
  const w0 = (2 * Math.PI * fc) / fs;
  const cos = Math.cos(w0);
  const sin = Math.sin(w0);
  const alpha = sin / (2 * Q);
  const b0 = (1 - cos) / 2;
  const b1 = 1 - cos;
  const b2 = (1 - cos) / 2;
  const a0 = 1 + alpha;
  const a1 = -2 * cos;
  const a2 = 1 - alpha;
  return {
    b0: b0 / a0,
    b1: b1 / a0,
    b2: b2 / a0,
    a1: a1 / a0,
    a2: a2 / a0,
  };
}

function highpassCoeffs(fc: number, fs: number, Q = 1 / Math.SQRT2): BiquadCoeffs {
  const w0 = (2 * Math.PI * fc) / fs;
  const cos = Math.cos(w0);
  const sin = Math.sin(w0);
  const alpha = sin / (2 * Q);
  const b0 = (1 + cos) / 2;
  const b1 = -(1 + cos);
  const b2 = (1 + cos) / 2;
  const a0 = 1 + alpha;
  const a1 = -2 * cos;
  const a2 = 1 - alpha;
  return {
    b0: b0 / a0,
    b1: b1 / a0,
    b2: b2 / a0,
    a1: a1 / a0,
    a2: a2 / a0,
  };
}

export class Biquad {
  z1 = 0;
  z2 = 0;
  readonly c: BiquadCoeffs;

  constructor(c: BiquadCoeffs) {
    this.c = c;
  }

  process(x: number): number {
    const y = this.c.b0 * x + this.z1;
    this.z1 = this.c.b1 * x - this.c.a1 * y + this.z2;
    this.z2 = this.c.b2 * x - this.c.a2 * y;
    return y;
  }

  reset(): void {
    this.z1 = 0;
    this.z2 = 0;
  }
}

/** Cascade HP 0.5 Hz + LP 5 Hz @ nominal Fs ≈ 100 Hz (per pc-bridge.md). */
export class BandPassChain {
  readonly hp: Biquad;
  readonly lp: Biquad;

  constructor(fs = 100) {
    this.hp = new Biquad(highpassCoeffs(0.5, fs));
    this.lp = new Biquad(lowpassCoeffs(5, fs));
  }

  process(sample: number): number {
    return this.lp.process(this.hp.process(sample));
  }

  reset(): void {
    this.hp.reset();
    this.lp.reset();
  }
}

/** Slow cuff baseline (DC component). */
export function dcSmooth(prev: number, raw: number, alpha: number): number {
  return prev + alpha * (raw - prev);
}

export interface OscPeak {
  pCuff: number;
  amplitude: number;
  tMs: number;
}

export interface MaaResult {
  map: number;
  sbp: number;
  dbp: number;
  aMax: number;
}

/** Find peaks on filtered oscillation; pair with smoothed cuff pressure. */
export function appendOscillationPeak(
  prev2Bp: number,
  prevBp: number,
  bp: number,
  prevDc: number,
  prevT: number,
  minAmp: number,
): OscPeak | null {
  if (prevBp > minAmp && prevBp >= prev2Bp && prevBp >= bp) {
    return { pCuff: prevDc, amplitude: prevBp, tMs: prevT };
  }
  return null;
}

/** Maximum amplitude algorithm on envelope samples sorted by cuff pressure descending. */
export function runMaa(
  peaks: OscPeak[],
  rs: number,
  rd: number,
): MaaResult | null {
  if (peaks.length < 5) return null;

  const sorted = [...peaks].sort((a, b) => b.pCuff - a.pCuff);
  let imax = 0;
  let aMax = sorted[0].amplitude;
  for (let i = 1; i < sorted.length; i++) {
    if (sorted[i].amplitude > aMax) {
      aMax = sorted[i].amplitude;
      imax = i;
    }
  }

  const map = sorted[imax].pCuff;
  const targetSys = rs * aMax;
  const targetDia = rd * aMax;

  let sbp = map;
  let dbp = map;

  let bestSysDiff = Infinity;
  for (let i = imax - 1; i >= 0; i--) {
    const diff = Math.abs(sorted[i].amplitude - targetSys);
    if (diff < bestSysDiff) {
      bestSysDiff = diff;
      sbp = sorted[i].pCuff;
    }
  }

  let bestDiaDiff = Infinity;
  for (let i = imax + 1; i < sorted.length; i++) {
    const diff = Math.abs(sorted[i].amplitude - targetDia);
    if (diff < bestDiaDiff) {
      bestDiaDiff = diff;
      dbp = sorted[i].pCuff;
    }
  }

  return { map, sbp, dbp, aMax };
}

export function medianBpmFromPeaks(peaks: OscPeak[]): number | null {
  if (peaks.length < 3) return null;
  const dt: number[] = [];
  const sortedT = [...peaks].sort((a, b) => a.tMs - b.tMs);
  for (let i = 1; i < sortedT.length; i++) {
    const d = sortedT[i].tMs - sortedT[i - 1].tMs;
    if (d > 50 && d < 2000) dt.push(d);
  }
  if (!dt.length) return null;
  dt.sort((a, b) => a - b);
  const mid = dt[Math.floor(dt.length / 2)]!;
  const bpm = 60000 / mid;
  if (bpm < 35 || bpm > 220) return null;
  return Math.round(bpm);
}

export interface ProcessorParams {
  sampleFs: number;
  inflateAmpThreshold: number;
  inflateSustainMs: number;
  marginMmHg: number;
  dcAlpha: number;
  peakMinAmp: number;
  rs: number;
  rd: number;
  /** Trần clamp lệnh `T,...` (đồng bộ MCU / form SAF). */
  effectiveSafMmHg: number;
  /** 0 = tắt. Khi áp cuff ≥ giá trị này và |AC| yên đủ lâu → gửi T sớm. */
  earlyCutoffMmHg: number;
  acQuietThreshold: number;
  acQuietMs: number;
  marginSmallQuietMmHg: number;
  /** Tốc độ xả chậm gửi MCU (`DR,...`) khi vào pha xả đo (mmHg/s). */
  deflateSlowRateMmHgS: number;
  /** Bật gửi `EARLYEND` khi đủ bao + cuff đủ thấp (và ổn định MAA nếu cấu hình). */
  earlyEndEnabled: boolean;
  /** Số đỉnh bao tối thiểu trước khi xét kết thúc sớm. */
  earlyEndMinPeaks: number;
  /** Cuff (DC làm mượt) phải ≤ DBP_MAA − giá trị này (mmHg). */
  earlyEndBelowDbpMmHg: number;
  /** 0 = không yêu cầu ổn định; ≥1 = số lần liên tiếp MAA gần nhau trong ngưỡng tol. */
  earlyEndStablePasses: number;
  /** Ngưỡng |ΔSBP| và |ΔDBP| cho ổn định MAA (mmHg). */
  earlyEndStableTolMmHg: number;
}

export interface IngestSampleResult {
  targetMmHg: number | null;
  /** Lệnh UART đầy đủ (đã có \\n), gửi theo thứ tự sau mẫu S. */
  extraTx: string[];
}

export const defaultProcessorParams: ProcessorParams = {
  sampleFs: 100,
  inflateAmpThreshold: 1.2,
  inflateSustainMs: 250,
  marginMmHg: 40,
  dcAlpha: 0.08,
  peakMinAmp: 0.35,
  rs: 0.5,
  rd: 0.7,
  effectiveSafMmHg: 175,
  earlyCutoffMmHg: 0,
  acQuietThreshold: 0.35,
  acQuietMs: 400,
  marginSmallQuietMmHg: 6,
  deflateSlowRateMmHgS: 3.0,
  earlyEndEnabled: false,
  earlyEndMinPeaks: 6,
  earlyEndBelowDbpMmHg: 15,
  earlyEndStablePasses: 0,
  earlyEndStableTolMmHg: 3,
};

export type PhaseUi =
  | 'idle'
  | 'inflate_slow'
  | 'inflate_margin'
  | 'deflate'
  | 'fast_deflate'
  | 'meas_end'
  | 'error';

/** Maps MCU announcement payloads from main.c / pc-bridge.md */
export function annunciationToPhase(code: string): PhaseUi | null {
  switch (code) {
    case 'IDLE':
      return 'idle';
    case 'INFLATE_SLOW':
      return 'inflate_slow';
    case 'INFLATE_MARGIN':
      return 'inflate_margin';
    case 'DEFLATE':
      return 'deflate';
    case 'FAST_DEFLATE':
      return 'fast_deflate';
    default:
      return null;
  }
}

export function eventToPhase(code: string): PhaseUi | null {
  if (code === 'MEAS_END') return 'meas_end';
  if (code === 'SENSOR_OR_LEAK') return 'error';
  return null;
}

export interface MeasurementSnapshot {
  phase: PhaseUi;
  lastAnnunciation: string;
  lastEvent: string;
  cuffMmHg: number;
  dcMmHg: number;
  bpFiltered: number;
  sys: number | null;
  dia: number | null;
  map: number | null;
  bpm: number | null;
  sentTargetMmHg: number | null;
  peaksCaptured: number;
  /** Envelope peaks for oscillometric chart: live during deflate; frozen after MEAS_END until INFLATE_SLOW. */
  oscEnvelopePeaks: OscPeak[];
  /** Tăng mỗi lần MAA thất bại; host dùng để chỉ hiện toast một lần / lần thất bại. */
  maaFailureNonce: number;
  /** Thông điệp lỗi MAA gần nhất (null nếu chưa thất bại hoặc lần gần nhất thành công). */
  maaFailureMessage: string | null;
}

export class MeasurementProcessor {
  params: ProcessorParams;
  private bp = new BandPassChain(defaultProcessorParams.sampleFs);
  /** DC làm mượt (mmHg); cập nhật mỗi mẫu — dùng cho đồ thị mà không gọi `snapshot()` (tránh copy mảng đỉnh bao ~100 Hz). */
  dcMmHg = 0;
  private dcInit = false;

  private phase: PhaseUi = 'idle';
  lastAnnunciation = '';
  lastEvent = '';

  private inflateHighSinceMs: number | null = null;
  private acQuietStartMs: number | null = null;
  private sentTargetThisCycle = false;

  private prev2Bp = 0;
  private prevBp = 0;
  private prevDc = 0;
  private prevTMs = 0;

  private envelopePeaks: OscPeak[] = [];
  /** Copy kept after MEAS_END so IDLE / resetCycle clearing live peaks does not wipe the oscillometric chart. */
  private frozenOscPeaks: OscPeak[] | null = null;

  /** Last `fsm` from extended `S,…` line (uart_proto.h); used to recover phase if A,/E, lines are dropped. */
  private prevMcuFsm: number | null = null;

  /** Latest finalized BP (shown until next cycle). */
  displaySys: number | null = null;
  displayDia: number | null = null;
  displayMap: number | null = null;
  displayBpm: number | null = null;

  private maaFinalized = false;
  maaFailureNonce = 0;
  maaFailureMessage: string | null = null;

  sentTargetMmHg: number | null = null;

  /** Đã gửi `DR,...` trong chu kỳ đo hiện tại (pha xả). */
  private sentDeflateRateUart = false;
  /** Đã gửi `EARLYEND` trong chu kỳ đo hiện tại. */
  private earlyEndUartSent = false;
  private earlyStableCount = 0;
  private earlyPrevSbp: number | null = null;
  private earlyPrevDbp: number | null = null;

  cuffMmHg = 0;
  bpFiltered = 0;

  constructor(params?: Partial<ProcessorParams>) {
    this.params = { ...defaultProcessorParams, ...params };
    this.bp = new BandPassChain(this.params.sampleFs);
  }

  setParams(p: Partial<ProcessorParams>): void {
    const fsChanged = p.sampleFs !== undefined && p.sampleFs !== this.params.sampleFs;
    this.params = { ...this.params, ...p };
    if (fsChanged) {
      this.bp = new BandPassChain(this.params.sampleFs);
      this.resetFilters();
    }
  }

  resetCycle(): void {
    this.inflateHighSinceMs = null;
    this.acQuietStartMs = null;
    this.sentTargetThisCycle = false;
    this.sentTargetMmHg = null;
    this.envelopePeaks = [];
    this.prev2Bp = this.prevBp = 0;
    this.prevDc = 0;
    this.prevMcuFsm = null;
    this.maaFinalized = false;
    this.maaFailureMessage = null;
    this.resetDeflateUartAndEarly();
    this.resetFilters();
  }

  private resetDeflateUartAndEarly(): void {
    this.sentDeflateRateUart = false;
    this.earlyEndUartSent = false;
    this.earlyStableCount = 0;
    this.earlyPrevSbp = null;
    this.earlyPrevDbp = null;
  }

  resetFilters(): void {
    this.bp.reset();
    this.dcInit = false;
  }

  onAnnunciation(code: string): void {
    this.lastAnnunciation = code;
    const ph = annunciationToPhase(code);
    if (ph) this.phase = ph;

    if (code === 'IDLE') {
      this.resetCycle();
      this.phase = 'idle';
    }

    if (code === 'INFLATE_SLOW') {
      this.sentTargetThisCycle = false;
      this.sentTargetMmHg = null;
      this.inflateHighSinceMs = null;
      this.acQuietStartMs = null;
      this.envelopePeaks = [];
      this.frozenOscPeaks = null;
      this.maaFinalized = false;
      this.displaySys = null;
      this.displayDia = null;
      this.displayMap = null;
      this.displayBpm = null;
      this.maaFailureMessage = null;
      this.resetDeflateUartAndEarly();
      this.resetFilters();
    }

    if (code === 'DEFLATE') {
      this.prev2Bp = this.prevBp = 0;
      this.prevDc = this.dcMmHg;
      this.envelopePeaks = [];
    }

    if (code === 'INFLATE_MARGIN') {
      this.acQuietStartMs = null;
    }
  }

  onEvent(code: string): void {
    this.lastEvent = code;
    const ph = eventToPhase(code);
    if (ph === 'meas_end') {
      this.phase = 'meas_end';
      this.finalizeMaa();
    }
    if (ph === 'error') {
      this.phase = 'error';
      this.envelopePeaks = [];
    }
  }

  /**
   * MCU `fsm` in extended sample lines: 0=IDLE 1=INFLATE_SLOW 2=INFLATE_MARGIN 3=DEFLATE 4=FAST_DEFLATE 5=DONE 6=ERROR.
   * Mirrors critical `onAnnunciation` / `onEvent` side effects when UART A,/E, lines are missing.
   */
  syncPhaseFromMcuFsm(fsm?: number): void {
    if (fsm === undefined || !Number.isFinite(fsm)) return;
    const f = Math.trunc(fsm);
    if (f < 0 || f > 6) return;
    if (this.prevMcuFsm === f) return;
    const prev = this.prevMcuFsm;

    switch (f) {
      case 0:
        if (this.phase !== 'idle') {
          this.resetCycle();
          this.phase = 'idle';
        }
        break;
      case 1:
        if (prev !== 1) {
          this.sentTargetThisCycle = false;
          this.sentTargetMmHg = null;
          this.inflateHighSinceMs = null;
          this.acQuietStartMs = null;
          this.envelopePeaks = [];
          this.frozenOscPeaks = null;
          this.maaFinalized = false;
          this.displaySys = null;
          this.displayDia = null;
          this.displayMap = null;
          this.displayBpm = null;
          this.maaFailureMessage = null;
          this.resetDeflateUartAndEarly();
          this.resetFilters();
        }
        this.phase = 'inflate_slow';
        break;
      case 2:
        if (prev !== 2) this.acQuietStartMs = null;
        this.phase = 'inflate_margin';
        break;
      case 3:
        if (this.phase !== 'deflate') {
          this.prev2Bp = this.prevBp = 0;
          this.prevDc = this.dcMmHg;
          this.envelopePeaks = [];
        }
        this.phase = 'deflate';
        break;
      case 4:
        this.phase = 'fast_deflate';
        break;
      case 5:
        if (this.phase === 'deflate' || this.phase === 'fast_deflate') {
          this.finalizeMaa();
          this.phase = 'meas_end';
        }
        break;
      case 6:
        this.phase = 'error';
        this.envelopePeaks = [];
        this.resetDeflateUartAndEarly();
        break;
      default:
        break;
    }
    this.prevMcuFsm = f;
  }

  private shouldSendEarlyEnd(): boolean {
    if (!this.params.earlyEndEnabled || this.earlyEndUartSent) return false;
    const n = this.envelopePeaks.length;
    if (n < this.params.earlyEndMinPeaks) {
      this.earlyStableCount = 0;
      this.earlyPrevSbp = null;
      this.earlyPrevDbp = null;
      return false;
    }
    const res = runMaa(this.envelopePeaks, this.params.rs, this.params.rd);
    if (!res) {
      this.earlyStableCount = 0;
      this.earlyPrevSbp = null;
      this.earlyPrevDbp = null;
      return false;
    }
    const cuff = this.dcMmHg;
    if (cuff > res.dbp - this.params.earlyEndBelowDbpMmHg) {
      this.earlyStableCount = 0;
      this.earlyPrevSbp = null;
      this.earlyPrevDbp = null;
      return false;
    }

    const needStable = this.params.earlyEndStablePasses;
    if (needStable <= 0) return true;

    const tol = this.params.earlyEndStableTolMmHg;
    if (this.earlyPrevSbp === null) {
      this.earlyPrevSbp = res.sbp;
      this.earlyPrevDbp = res.dbp;
      this.earlyStableCount = 1;
      return false;
    }
    const stable =
      Math.abs(res.sbp - this.earlyPrevSbp) <= tol &&
      Math.abs(res.dbp - this.earlyPrevDbp) <= tol;
    if (stable) {
      this.earlyStableCount++;
      this.earlyPrevSbp = res.sbp;
      this.earlyPrevDbp = res.dbp;
      return this.earlyStableCount >= needStable;
    }
    this.earlyStableCount = 1;
    this.earlyPrevSbp = res.sbp;
    this.earlyPrevDbp = res.dbp;
    return false;
  }

  /**
   * @returns lệnh `T,...` nếu cần, và các lệnh UART bổ sung (`DR`, `EARLYEND`).
   */
  ingestSample(tMs: number, pMmHg: number): IngestSampleResult {
    this.cuffMmHg = pMmHg;

    if (!this.dcInit) {
      this.dcMmHg = pMmHg;
      this.dcInit = true;
    } else {
      this.dcMmHg = dcSmooth(this.dcMmHg, pMmHg, this.params.dcAlpha);
    }

    const filt = this.bp.process(pMmHg);
    this.bpFiltered = filt;
    const rect = Math.abs(filt);

    let sendTarget: number | null = null;

    if (
      !this.sentTargetThisCycle &&
      (this.phase === 'inflate_slow' || this.phase === 'inflate_margin')
    ) {
      const ec = this.params.earlyCutoffMmHg;
      if (ec > 0 && pMmHg >= ec) {
        if (rect < this.params.acQuietThreshold) {
          if (this.acQuietStartMs === null) this.acQuietStartMs = tMs;
          else if (tMs - this.acQuietStartMs >= this.params.acQuietMs) {
            const cap = this.params.effectiveSafMmHg;
            let tgt = Math.round(this.dcMmHg + this.params.marginSmallQuietMmHg);
            tgt = Math.min(cap, tgt);
            tgt = Math.max(tgt, Math.round(pMmHg + 3));
            tgt = Math.max(30, tgt);
            sendTarget = tgt;
          }
        } else {
          this.acQuietStartMs = null;
        }
      } else {
        this.acQuietStartMs = null;
      }
    }

    if (sendTarget === null && this.phase === 'inflate_slow' && !this.sentTargetThisCycle) {
      const a = rect;
      if (a >= this.params.inflateAmpThreshold) {
        if (this.inflateHighSinceMs === null) this.inflateHighSinceMs = tMs;
        else if (tMs - this.inflateHighSinceMs >= this.params.inflateSustainMs) {
          const estSys = this.dcMmHg;
          sendTarget = Math.round(
            Math.min(estSys + this.params.marginMmHg, this.params.effectiveSafMmHg),
          );
        }
      } else {
        this.inflateHighSinceMs = null;
      }
    }

    if (sendTarget !== null) {
      this.sentTargetThisCycle = true;
      this.sentTargetMmHg = sendTarget;
    }

    const extraTx: string[] = [];

    if (this.phase === 'deflate') {
      const pk = appendOscillationPeak(
        this.prev2Bp,
        this.prevBp,
        rect,
        this.prevDc,
        this.prevTMs,
        this.params.peakMinAmp,
      );
      if (pk) this.envelopePeaks.push(pk);

      if (!this.sentDeflateRateUart) {
        this.sentDeflateRateUart = true;
        extraTx.push(formatDeflateRateCommand(this.params.deflateSlowRateMmHgS));
      }
      if (this.shouldSendEarlyEnd()) {
        this.earlyEndUartSent = true;
        extraTx.push(formatEarlyEndCommand());
      }
    }

    this.prev2Bp = this.prevBp;
    this.prevBp = rect;
    this.prevDc = this.dcMmHg;
    this.prevTMs = tMs;

    return { targetMmHg: sendTarget, extraTx };
  }

  private finalizeMaa(): void {
    if (this.maaFinalized) return;
    this.maaFinalized = true;
    this.frozenOscPeaks = [...this.envelopePeaks];
    const n = this.envelopePeaks.length;
    const res = runMaa(this.envelopePeaks, this.params.rs, this.params.rd);
    const bpm = medianBpmFromPeaks(this.envelopePeaks);
    if (res) {
      this.maaFailureMessage = null;
      this.displaySys = Math.round(res.sbp);
      this.displayDia = Math.round(res.dbp);
      this.displayMap = Math.round(res.map);
      this.displayBpm = bpm;
    } else {
      this.displaySys = null;
      this.displayDia = null;
      this.displayMap = null;
      this.displayBpm = null;
      this.maaFailureNonce += 1;
      this.maaFailureMessage = `Không tính được huyết áp (MAA): chỉ có ${n} đỉnh bao, cần ≥5. Thử giảm «Đỉnh tối thiểu (pha xả)» hoặc kiểm tra tín hiệu / pha xả.`;
    }
  }

  private oscPeaksForUi(): OscPeak[] {
    if (this.phase === 'deflate') return [...this.envelopePeaks];
    return this.frozenOscPeaks ? [...this.frozenOscPeaks] : [];
  }

  snapshot(): MeasurementSnapshot {
    const osc = this.oscPeaksForUi();
    return {
      phase: this.phase,
      lastAnnunciation: this.lastAnnunciation,
      lastEvent: this.lastEvent,
      cuffMmHg: this.cuffMmHg,
      dcMmHg: this.dcMmHg,
      bpFiltered: this.bpFiltered,
      sys: this.displaySys,
      dia: this.displayDia,
      map: this.displayMap,
      bpm: this.displayBpm,
      sentTargetMmHg: this.sentTargetMmHg,
      peaksCaptured: osc.length,
      oscEnvelopePeaks: osc,
      maaFailureNonce: this.maaFailureNonce,
      maaFailureMessage: this.maaFailureMessage,
    };
  }
}
