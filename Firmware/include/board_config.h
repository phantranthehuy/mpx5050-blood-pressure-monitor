/**
 * Pin map — điều chỉnh nếu schematic khác Blue Pill tiêu chuẩn.
 * UART1: PA9 TX, PA10 RX | I2C1: PB6 SCL, PB7 SDA | TIM3 PWM: PA6 motor, PA7 valve
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

/* --- UART --- */
#define UART_BAUD           115200u

/* --- I2C ADS1115 (địa chỉ ADDR → GND = 0x48) --- */
#define ADS1115_I2C_ADDR    (0x48u << 1) /* HAL dùng 8-bit */

/* --- Buttons: active LOW, internal pull-up --- */
#define BTN_START_PORT      GPIOA
#define BTN_START_PIN       GPIO_PIN_3
#define BTN_STOP_PORT       GPIOA
#define BTN_STOP_PIN        GPIO_PIN_4
#define BTN_HIGH_PORT       GPIOA
#define BTN_HIGH_PIN        GPIO_PIN_5

/* --- LEDs — schematic D401/D402/D403: PB13 / PB14 / PB15 --- */
#define LED_RED_PORT        GPIOB
#define LED_RED_PIN         GPIO_PIN_13
#define LED_GREEN_PORT      GPIOB
#define LED_GREEN_PIN       GPIO_PIN_14
#define LED_YELLOW_PORT     GPIOB
#define LED_YELLOW_PIN      GPIO_PIN_15

/* --- PWM pump / valve (TIM3) --- */
#define PWM_TIM             TIM3
#define PWM_MOTOR_CHANNEL   TIM_CHANNEL_1 /* PA6 */
#define VALVE_CHANNEL       TIM_CHANNEL_2 /* PA7 */

/* --- Safety / physics (hiệu chỉnh thực tế) --- */
#define PRESSURE_SAFE_MAX_MMHG      280.0f
#define LEAK_TIMEOUT_MS             10000u
#define INFLATE_FALLBACK_TARGET_MMHG 175.0f   /* khi không có lệnh T,... */
#define INFLATE_FALLBACK_AFTER_MS   120000u    /* 2 phút chờ lệnh WebApp */
#define TARGET_MARGIN_DEFAULT_MMHG  40.0f     /* WebApp gửi đích = P_sys_est + margin */

#define TARGET_PRESSURE_RATE_MMHG_S  10.0f    /* bơm chậm */

#define DEFLATE_SLOW_RATE_MMHG_S     3.0f     /* servo van */

#define PUMP_PWM_MIN                 5u
#define PUMP_PWM_MAX                 100u

#define VALVE_CLOSED_DUTY            0u       /* 0% = đóng (điều chỉnh nếu NC/NO ngược) */
#define VALVE_FULL_OPEN_DUTY         100u

/**
 * Chuyển đổi MPX5050GP @ 5V → mmHg (xấp xỉ). Hiệu chỉnh OFFSET_MMHG / SCALE với đồng hồ áp.
 * Giả định ADS1115 PGA ±4.096 V → LSB = 125 µV (đặt trong ads1115 driver).
 */
#define PRESSURE_ADC_OFFSET_COUNTS   (-13312) /* placeholder — HIỆU CHỈNH */
#define PRESSURE_ADC_SCALE_MMHG_PER_COUNT (280.0f / 26000.0f) /* placeholder — hiệu chỉnh thực tế */

#endif /* BOARD_CONFIG_H */
