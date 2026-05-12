/**
 * Hiển thị LED theo [LED_Algorithm.md](LED_Algorithm.md) — schematic D401/D402/D403: PB13/PB14/PB15.
 * Ánh xạ trạng thái máy (plan firmware):
 * - IDLE              → STATE_IDLE
 * - INFLATING         → INFLATE_SLOW_LISTEN + INFLATE_TO_MARGIN
 * - MEASURING         → DEFLATE_MEASURE; hoặc FAST_DEFLATE sau xả bình thường
 * - ERROR             → ERROR (hở khí / I2C…)
 * - EMERGENCY         → FAST_DEFLATE do STOP, quá áp (SAF), hoặc lệnh ABORT host
 */
#ifndef LED_HMI_H
#define LED_HMI_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef enum {
    LED_SYS_STATE_IDLE = 0,
    LED_SYS_STATE_INFLATING,
    LED_SYS_STATE_MEASURING,
    LED_SYS_STATE_ERROR,
    LED_SYS_STATE_EMERGENCY,
} LedHmiSystemState_t;

#define LED_HMI_BLINK_NORMAL_MS 500u
#define LED_HMI_BLINK_FAST_MS   150u

/** Gọi trong vòng lặp chính (≥ tick hiện tại); không dùng HAL_Delay. */
void led_hmi_task(LedHmiSystemState_t current_sys_state);

#endif
