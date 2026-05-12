#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <Arduino.h>

/* UART1 PA9/PA10 — Serial mặc định Blue Pill */
#define PIN_I2C_SCL PB6
#define PIN_I2C_SDA PB7

#define PIN_BTN_START PA11
#define PIN_BTN_STOP  PA8
#define PIN_BTN_HIGH  PB12

#define PIN_LED_RED    PB13
#define PIN_LED_GREEN  PB14
#define PIN_LED_YELLOW PB15

#define PIN_PWM_PUMP  PA0
#define PIN_PWM_VALVE PA1

#endif
