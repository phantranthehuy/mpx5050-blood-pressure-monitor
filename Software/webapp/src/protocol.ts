export type ParsedLine =
  | { tag: 'S'; seq: number; tMs: number; pMmHg: number }
  | { tag: 'A'; code: string }
  | { tag: 'E'; code: string }
  | { tag: 'unknown'; raw: string };

/** Matches Firmware uart_proto_send_sample snprintf format `S,%lu,%lu,%.2f`. */
export function parseUartLine(line: string): ParsedLine {
  const trimmed = line.replace(/\r$/, '').trim();
  if (!trimmed) return { tag: 'unknown', raw: '' };

  const first = trimmed[0];
  const body = trimmed.slice(2);

  if (first === 'S') {
    const m = /^(\d+),(\d+),([\d.-]+)$/.exec(body);
    if (m) {
      return {
        tag: 'S',
        seq: Number(m[1]),
        tMs: Number(m[2]),
        pMmHg: Number(m[3]),
      };
    }
  }

  if (first === 'A') return { tag: 'A', code: body };
  if (first === 'E') return { tag: 'E', code: body };

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

export const SAF_MAX_MMHG = 280;

export function clampTargetMmhg(mmHg: number): number {
  return Math.min(SAF_MAX_MMHG, Math.max(0, mmHg));
}

export function formatTargetCommand(mmHg: number): string {
  return `T,${clampTargetMmhg(mmHg)}\n`;
}

export function formatAbortCommand(): string {
  return 'ABORT\n';
}
