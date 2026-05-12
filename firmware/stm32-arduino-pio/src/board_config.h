/**
 * Hằng số ứng dụng (áp suất, timeout, PWM %) — dùng chung bp_fsm / pressure.
 * Map chân nằm trong board_pins.h (Arduino).
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

#define UART_BAUD 115200u

/* ADS1115 ADDR→GND = 0x48 (Wire dùng địa chỉ 7-bit) */
#define ADS1115_I2C_ADDR_7BIT ((uint8_t)0x48u)

#define PRESSURE_SAFE_MAX_MMHG           280.0f
#define LEAK_TIMEOUT_MS                  10000u
#define INFLATE_FALLBACK_TARGET_MMHG     175.0f
#define INFLATE_FALLBACK_AFTER_MS        120000u
#define TARGET_MARGIN_DEFAULT_MMHG       40.0f

#define TARGET_PRESSURE_RATE_MMHG_S      10.0f
#define DEFLATE_SLOW_RATE_MMHG_S         3.0f

#define PUMP_PWM_MIN                     5u
#define PUMP_PWM_MAX                     100u

/* --- Soft-start chống brownout do dòng inrush motor bơm ---
 * Trong PUMP_SOFTSTART_MS đầu tiên sau khi rời IDLE, kẹp PWM ≤
 * PUMP_SOFTSTART_CAP_PCT để dòng đỉnh không kéo sụt rail VCC.
 * Đồng thời mọi thay đổi pump_pwm trong các state INFLATE bị cap
 * theo PUMP_SLOPE_PER_TICK_MAX (đơn vị %/tick @100Hz). */
#define PUMP_SOFTSTART_MS                500u
#define PUMP_SOFTSTART_CAP_PCT           30u
#define PUMP_SLOPE_PER_TICK_MAX          1

#define VALVE_CLOSED_DUTY                0u
#define VALVE_FULL_OPEN_DUTY             100u

#define PRESSURE_ADC_OFFSET_COUNTS       (-13312)
#define PRESSURE_ADC_SCALE_MMHG_PER_COUNT (280.0f / 26000.0f)

#endif
