#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <Arduino.h>

/* UART1 PA9/PA10 — Serial mặc định Blue Pill */
#define PIN_I2C_SCL PB6
#define PIN_I2C_SDA PB7

#define PIN_BTN_START PA3
#define PIN_BTN_STOP  PA4
#define PIN_BTN_HIGH  PA5

#define PIN_LED_RED    PB13
#define PIN_LED_GREEN  PB14
#define PIN_LED_YELLOW PB15

#define PIN_PWM_PUMP  PA6
#define PIN_PWM_VALVE PA7

#endif
