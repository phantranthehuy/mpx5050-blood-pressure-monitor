export type ParsedLine =
  | {
      tag: 'S';
      seq: number;
      tMs: number;
      pMmHg: number;
      rc?: number;
      counts?: number;
      fsm?: number;
      pumpPct?: number;
      valvePct?: number;
      btnS?: number;
      btnP?: number;
      btnH?: number;
      /** dp/dt (mmHg/s), Arduino PIO khi có trường thứ 10 sau nút. */
      dpMmHgPerS?: number;
    }
  | { tag: 'A'; code: string }
  | { tag: 'E'; code: string }
  | { tag: 'R'; payload: string }
  | { tag: 'unknown'; raw: string };

/** MCU: `S,seq,t_ms,p_mmHg` hoặc thêm `,rc,counts,fsm,pump,valve,btns` (Arduino PIO), hoặc thêm `,dp_centi` (dp mmHg/s × 100). */
export function parseUartLine(line: string): ParsedLine {
  const trimmed = line.replace(/\r$/, '').trim();
  if (!trimmed) return { tag: 'unknown', raw: '' };

  const first = trimmed[0];
  const body = trimmed.slice(2);

  if (first === 'S') {
    const parts = body.split(',');
    if (parts.length >= 3) {
      const seq = Number(parts[0]);
      const tMs = Number(parts[1]);
      const pMmHg = Number(parts[2]);
      if (Number.isFinite(seq) && Number.isFinite(tMs) && Number.isFinite(pMmHg)) {
        const out: Extract<ParsedLine, { tag: 'S' }> = {
          tag: 'S',
          seq,
          tMs,
          pMmHg,
        };
        if (parts.length >= 4) {
          const rc = Number(parts[3]);
          if (Number.isFinite(rc)) out.rc = rc;
        }
        if (parts.length >= 5) {
          const counts = Number(parts[4]);
          if (Number.isFinite(counts)) out.counts = Math.trunc(counts);
        }
        if (parts.length >= 6) {
          const fsm = Number(parts[5]);
          if (Number.isFinite(fsm)) out.fsm = Math.trunc(fsm);
        }
        if (parts.length >= 7) {
          const pumpPct = Number(parts[6]);
          if (Number.isFinite(pumpPct)) out.pumpPct = Math.trunc(pumpPct);
        }
        if (parts.length >= 8) {
          const valvePct = Number(parts[7]);
          if (Number.isFinite(valvePct)) out.valvePct = Math.trunc(valvePct);
        }
        if (parts.length >= 9) {
          const tail = parts[8]?.trim() ?? '';
          if (/^\d{3}$/.test(tail)) {
            out.btnS = tail[0] === '1' ? 1 : 0;
            out.btnP = tail[1] === '1' ? 1 : 0;
            out.btnH = tail[2] === '1' ? 1 : 0;
          }
        }
        if (parts.length >= 10) {
          const dpCenti = Number(parts[9]);
          if (Number.isFinite(dpCenti)) out.dpMmHgPerS = dpCenti * 0.01;
        }
        return out;
      }
    }
  }

  if (first === 'A') return { tag: 'A', code: body };
  if (first === 'E') return { tag: 'E', code: body };
  if (first === 'R') return { tag: 'R', payload: body.trim() };

  return { tag: 'unknown', raw: trimmed };
}

export interface SerialCallbacks {
  onLine(line: string): void;
  onDisconnect(): void;
}

/** Accumulates chunks into \\n-delimited lines; ignores stray \\r (MCU skips CR). */
export class LineAccumulator {
  private buf = '';

  push(chunk: string): string[] {
    this.buf += chunk;
    const lines = this.buf.split('\n');
    this.buf = lines.pop() ?? '';
    return lines;
  }

  reset() {
    this.buf = '';
  }
}

/** Khớp mặc định firmware `PRESSURE_SAFE_MAX_MMHG` (board_config.h). */
export const SAF_DEFAULT_MMHG = 175;

/** Giới clamp lệnh `SAF` / `SAFH` trên MCU (UART). */
export const SAF_UART_MIN_MMHG = 120;
export const SAF_UART_MAX_MMHG = 300;

export function clampUartSafMmhg(mmHg: number): number {
  return Math.min(SAF_UART_MAX_MMHG, Math.max(SAF_UART_MIN_MMHG, mmHg));
}

export function clampTargetMmhg(mmHg: number, safMax: number = SAF_DEFAULT_MMHG): number {
  const cap = Math.min(SAF_UART_MAX_MMHG, Math.max(SAF_UART_MIN_MMHG, safMax));
  return Math.min(cap, Math.max(0, mmHg));
}

export function formatTargetCommand(mmHg: number, safMax: number = SAF_DEFAULT_MMHG): string {
  return `T,${clampTargetMmhg(mmHg, safMax)}\n`;
}

export function formatAbortCommand(): string {
  return 'ABORT\n';
}

/** Giống nhấn nút START trên board — cần firmware hỗ trợ; sau đó gửi `T,…` trong pha bơm chậm. */
export function formatStartCommand(): string {
  return 'START\n';
}

export function formatSafCommand(mmHg: number): string {
  return `SAF,${clampUartSafMmhg(mmHg)}\n`;
}

export function formatSafHighCommand(mmHg: number): string {
  return `SAFH,${clampUartSafMmhg(mmHg)}\n`;
}

export function formatHighUartCommand(on: boolean): string {
  return `HIGH,${on ? 1 : 0}\n`;
}

/** Khớp clamp firmware `DEFLATE_SLOW_RATE_UART_*` (mmHg/s). */
export const DEFLATE_RATE_UART_MIN = 0.8;
export const DEFLATE_RATE_UART_MAX = 4.0;

export function clampDeflateRateMmhgS(mmHgPerS: number): number {
  return Math.min(DEFLATE_RATE_UART_MAX, Math.max(DEFLATE_RATE_UART_MIN, mmHgPerS));
}

/** MCU: đặt tốc độ xả chậm khi đo (DEFLATE_MEASURE). */
export function formatDeflateRateCommand(mmHgPerS: number): string {
  return `DR,${clampDeflateRateMmhgS(mmHgPerS).toFixed(2)}\n`;
}

/** MCU: kết thúc xả chậm sớm → xả nhanh (chỉ khi đang DEFLATE_MEASURE). */
export function formatEarlyEndCommand(): string {
  return 'EARLYEND\n';
}
