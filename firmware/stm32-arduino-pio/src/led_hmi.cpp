#include "led_hmi.h"
#include "board_pins.h"

extern "C" void led_hmi_task(LedHmiSystemState_t current_sys_state)
{
    static uint32_t previous_tick = 0;
    static uint8_t blink_flag = 0;
    static LedHmiSystemState_t last_sys_state = (LedHmiSystemState_t)255;

    uint32_t current_tick = millis();
    uint8_t state_changed = 0;

    if (current_sys_state != last_sys_state) {
        state_changed = 1;
        last_sys_state = current_sys_state;
        previous_tick = current_tick;
        blink_flag = 1;
    }

    switch (current_sys_state) {
    case LED_SYS_STATE_IDLE:
        if (state_changed) {
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_YELLOW, LOW);
            digitalWrite(PIN_LED_RED, LOW);
        }
        break;

    case LED_SYS_STATE_INFLATING:
        if (state_changed) {
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_RED, LOW);
        }
        if ((current_tick - previous_tick) >= LED_HMI_BLINK_NORMAL_MS) {
            previous_tick = current_tick;
            blink_flag = (uint8_t)!blink_flag;
            digitalWrite(PIN_LED_YELLOW, blink_flag ? HIGH : LOW);
        }
        break;

    case LED_SYS_STATE_MEASURING:
        if (state_changed) {
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_YELLOW, HIGH);
            digitalWrite(PIN_LED_RED, LOW);
        }
        break;

    case LED_SYS_STATE_ERROR:
        if (state_changed) {
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_YELLOW, LOW);
            digitalWrite(PIN_LED_RED, HIGH);
        }
        break;

    case LED_SYS_STATE_EMERGENCY:
        if (state_changed) {
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_YELLOW, LOW);
        }
        if ((current_tick - previous_tick) >= LED_HMI_BLINK_FAST_MS) {
            previous_tick = current_tick;
            blink_flag = (uint8_t)!blink_flag;
            digitalWrite(PIN_LED_RED, blink_flag ? HIGH : LOW);
        }
        break;
    }
}
