#include "led_hmi.h"
#include "board_config.h"

void led_hmi_task(LedHmiSystemState_t current_sys_state)
{
    static uint32_t previous_tick = 0;
    static uint8_t blink_flag = 0;
    static LedHmiSystemState_t last_sys_state = (LedHmiSystemState_t)255;

    uint32_t current_tick = HAL_GetTick();
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
            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
        }
        break;

    case LED_SYS_STATE_INFLATING:
        if (state_changed) {
            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
        }
        if ((current_tick - previous_tick) >= LED_HMI_BLINK_NORMAL_MS) {
            previous_tick = current_tick;
            blink_flag = (uint8_t)!blink_flag;
            HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN,
                              blink_flag ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
        break;

    case LED_SYS_STATE_MEASURING:
        if (state_changed) {
            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
        }
        break;

    case LED_SYS_STATE_ERROR:
        if (state_changed) {
            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
        }
        break;

    case LED_SYS_STATE_EMERGENCY:
        if (state_changed) {
            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);
        }
        if ((current_tick - previous_tick) >= LED_HMI_BLINK_FAST_MS) {
            previous_tick = current_tick;
            blink_flag = (uint8_t)!blink_flag;
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN,
                              blink_flag ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
        break;
    }
}
