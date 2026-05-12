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
}

export class MeasurementProcessor {
  params: ProcessorParams;
  private bp = new BandPassChain(defaultProcessorParams.sampleFs);
  private dc = 0;
  private dcInit = false;

  private phase: PhaseUi = 'idle';
  lastAnnunciation = '';
  lastEvent = '';

  private inflateHighSinceMs: number | null = null;
  private sentTargetThisCycle = false;

  private prev2Bp = 0;
  private prevBp = 0;
  private prevDc = 0;
  private prevTMs = 0;

  private envelopePeaks: OscPeak[] = [];
  /** Copy kept after MEAS_END so IDLE / resetCycle clearing live peaks does not wipe the oscillometric chart. */
  private frozenOscPeaks: OscPeak[] | null = null;

  /** Latest finalized BP (shown until next cycle). */
  displaySys: number | null = null;
  displayDia: number | null = null;
  displayMap: number | null = null;
  displayBpm: number | null = null;

  sentTargetMmHg: number | null = null;

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
    this.sentTargetThisCycle = false;
    this.sentTargetMmHg = null;
    this.envelopePeaks = [];
    this.prev2Bp = this.prevBp = 0;
    this.prevDc = 0;
    this.resetFilters();
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
      this.envelopePeaks = [];
      this.frozenOscPeaks = null;
      this.resetFilters();
    }

    if (code === 'DEFLATE') {
      this.prev2Bp = this.prevBp = 0;
      this.prevDc = this.dc;
      this.envelopePeaks = [];
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
   * @returns target mmHg to send if detection fired this sample.
   */
  ingestSample(tMs: number, pMmHg: number): number | null {
    this.cuffMmHg = pMmHg;

    if (!this.dcInit) {
      this.dc = pMmHg;
      this.dcInit = true;
    } else {
      this.dc = dcSmooth(this.dc, pMmHg, this.params.dcAlpha);
    }

    const filt = this.bp.process(pMmHg);
    this.bpFiltered = filt;
    const rect = Math.abs(filt);

    let sendTarget: number | null = null;

    if (this.phase === 'inflate_slow' && !this.sentTargetThisCycle) {
      const a = rect;
      if (a >= this.params.inflateAmpThreshold) {
        if (this.inflateHighSinceMs === null) this.inflateHighSinceMs = tMs;
        else if (tMs - this.inflateHighSinceMs >= this.params.inflateSustainMs) {
          const estSys = this.dc;
          sendTarget = Math.round(estSys + this.params.marginMmHg);
          this.sentTargetThisCycle = true;
          this.sentTargetMmHg = sendTarget;
        }
      } else {
        this.inflateHighSinceMs = null;
      }
    }

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
    }

    this.prev2Bp = this.prevBp;
    this.prevBp = rect;
    this.prevDc = this.dc;
    this.prevTMs = tMs;

    return sendTarget;
  }

  private finalizeMaa(): void {
    this.frozenOscPeaks = [...this.envelopePeaks];
    const res = runMaa(this.envelopePeaks, this.params.rs, this.params.rd);
    const bpm = medianBpmFromPeaks(this.envelopePeaks);
    if (res) {
      this.displaySys = Math.round(res.sbp);
      this.displayDia = Math.round(res.dbp);
      this.displayMap = Math.round(res.map);
      this.displayBpm = bpm;
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
      dcMmHg: this.dc,
      bpFiltered: this.bpFiltered,
      sys: this.displaySys,
      dia: this.displayDia,
      map: this.displayMap,
      bpm: this.displayBpm,
      sentTargetMmHg: this.sentTargetMmHg,
      peaksCaptured: osc.length,
      oscEnvelopePeaks: osc,
    };
  }
}
