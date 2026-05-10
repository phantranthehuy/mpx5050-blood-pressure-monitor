#ifndef UART_PROTO_H
#define UART_PROTO_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/** Khởi tạo RX interrupt (1 byte). TX dùng blocking trong tick để đơn giản. */
void uart_proto_init(UART_HandleTypeDef *huart);

/** Gọi mỗi tick để gửi một mẫu stream */
void uart_proto_send_sample(uint32_t seq, uint32_t t_ms, float p_mmhg);

/** Gửi khung kết thúc / trạng thái */
void uart_proto_send_line(const char *fmt, ...);

/** Gọi từ HAL_UART_RxCpltCallback */
void uart_proto_rx_callback(UART_HandleTypeDef *huart);

#endif
