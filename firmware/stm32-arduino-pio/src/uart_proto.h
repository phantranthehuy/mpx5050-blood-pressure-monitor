#ifndef UART_PROTO_H
#define UART_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_proto_init(void);
void uart_proto_poll_rx(void);
/** Stream:
 * S,<seq>,<t_ms>,<p_mmhg>,<rc>,<counts>,<fsm>,<pump_pct>,<valve_pct>,<btn_s><btn_p><btn_h>[,dp_centi]
 * fsm: 0=IDLE 1=INFLATE_SLOW 2=INFLATE_MARGIN 3=DEFLATE 4=FAST_DEFLATE 5=DONE 6=ERROR 7=IDLE_VENT
 * btn: 3 ký tự 0/1 = START STOP HIGH (đã đảo active-low)
 * dp_centi (tuỳ chọn): dp/dt mmHg/s × 100 (âm = áp giảm). */
void uart_proto_send_sample(uint32_t seq, uint32_t t_ms, float p_mmhg,
                            int rc, int16_t counts,
                            int fsm, int pump_pct, int valve_pct,
                            int btn_s, int btn_p, int btn_h, int dp_centi);
void uart_proto_send_line(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
