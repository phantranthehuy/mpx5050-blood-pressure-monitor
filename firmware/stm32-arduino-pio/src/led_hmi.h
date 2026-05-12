/**
 * LED HMI — cùng trạng thái logic với bản HAL (LED_Algorithm.md).
 */
#ifndef LED_HMI_H
#define LED_HMI_H

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

#ifdef __cplusplus
extern "C" {
#endif

void led_hmi_task(LedHmiSystemState_t current_sys_state);

#ifdef __cplusplus
}
#endif

#endif
