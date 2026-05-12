#ifndef BP_FSM_H
#define BP_FSM_H

#include "led_hmi.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BP_STATE_IDLE = 0,
    BP_STATE_INFLATE_SLOW_LISTEN,
    BP_STATE_INFLATE_TO_MARGIN,
    BP_STATE_DEFLATE_MEASURE,
    BP_STATE_FAST_DEFLATE,
    BP_STATE_DONE,
    BP_STATE_ERROR,
    BP_STATE_IDLE_VENT
} BpState;

typedef enum {
    BP_ERROR_NONE = 0,
    BP_ERROR_I2C_SENSOR,
    BP_ERROR_OVERPRESSURE,
    BP_ERROR_LEAK
} BpErrorReason;

void bp_fsm_init(void);

/** Gọi ~100 Hz với áp suất mmHg hiện tại; nút 1 = nhấn (active low đã đảo). */
void bp_fsm_on_tick(uint32_t now_ms, float pressure_mmhg,
                    int start_pressed, int stop_pressed, int high_pressed);

/** WebApp: mục tiêu áp cuff (thường P_sys_est + 40). MCU clamp ≤ SAFE_MAX. */
void bp_fsm_host_set_target_mmhg(float target_mmhg);

void bp_fsm_host_abort_measure(void);

/** UART / WebApp: yêu cầu bắt đầu đo giống nhấn nút START trên board (chỉ khi IDLE/DONE). */
void bp_fsm_host_request_uart_start(void);

/** UART: đặt trần an toàn thường / HA (clamp theo `PRESSURE_SAFE_UART_*`). */
void bp_fsm_host_set_saf_mmhg(float mmhg);
void bp_fsm_host_set_saf_high_mmhg(float mmhg);
void bp_fsm_host_set_high_uart(uint8_t on);

/** UART `DR,<mmHg/s>`: tốc độ xả chậm khi đo (clamp theo board_config). */
void bp_fsm_host_set_deflate_rate_mmhg_s(float mmhg_per_s);

/** UART `EARLYEND`: kết thúc xả chậm sớm → xả nhanh tới áp thấp (chỉ trong DEFLATE_MEASURE). */
void bp_fsm_host_request_early_measure_done(void);

/** Đếm lỗi I2C liên tiếp — main reset khi đọc OK */
void bp_fsm_sensor_i2c_fail(void);
void bp_fsm_sensor_i2c_ok(void);

BpState bp_fsm_get_state(void);
BpErrorReason bp_fsm_get_error_reason(void);
uint8_t bp_fsm_get_pump_pwm_percent(void);
uint8_t bp_fsm_get_valve_pwm_percent(void);
/** dp/dt (mmHg/s) mẫu gần nhất — log UART / chẩn đoán xả. */
float bp_fsm_get_last_dp_dt_mmhg_s(void);

/** Ánh xạ FSM → LedHmiSystemState_t (LED_Algorithm.md). */
LedHmiSystemState_t bp_fsm_led_hmi_state(void);

#ifdef __cplusplus
}
#endif

#endif
