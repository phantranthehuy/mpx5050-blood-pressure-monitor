#ifndef UART_PROTO_H
#define UART_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_proto_init(void);
void uart_proto_poll_rx(void);
void uart_proto_send_sample(uint32_t seq, uint32_t t_ms, float p_mmhg,
                            int rc, int16_t counts);
void uart_proto_send_line(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
