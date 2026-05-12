/**
 * Port song song với firmware HAL (`firmware/stm32-hal-cmake/Core/Src/main.c`):
 * TIM2 100 Hz, PWM bơm/van 1 kHz (analogWrite), đọc ADS1115, stream UART, FSM + LED.
 */
#include <Arduino.h>
#include <HardwareTimer.h>
#include <Wire.h>

extern "C" {
#include "board_config.h"
#include "bp_fsm.h"
#include "pressure.h"
}

#include "ads1115.h"
#include "board_pins.h"
#include "led_hmi.h"
#include "uart_proto.h"

static HardwareTimer tim2(TIM2);

static volatile bool g_tick_100hz = false;

static Ads1115_Handle g_ads;

static void tim2_100hz_callback(void)
{
    g_tick_100hz = true;
}

static void apply_pwm_outputs(void)
{
    uint32_t pump = bp_fsm_get_pump_pwm_percent();
    uint32_t valve = bp_fsm_get_valve_pwm_percent();
    if (pump > 100u) pump = 100u;
    if (valve > 100u) valve = 100u;
    /* 10-bit: 0–1023 ~ 0–100 % (tương đương HAL ARR=999, compare=pump*10) */
    analogWrite(PIN_PWM_PUMP, (int)((pump * 1023u) / 100u));
    analogWrite(PIN_PWM_VALVE, (int)((valve * 1023u) / 100u));
}

void setup(void)
{
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_BTN_STOP, INPUT_PULLUP);
    pinMode(PIN_BTN_HIGH, INPUT_PULLUP);

    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);

    Wire.setSCL(PIN_I2C_SCL);
    Wire.setSDA(PIN_I2C_SDA);
    Wire.begin();
    Wire.setClock(400000);

    uart_proto_init();

    ads1115_init(&g_ads);

    bp_fsm_init();

    tim2.setOverflow(100, HERTZ_FORMAT);
    tim2.attachInterrupt(tim2_100hz_callback);
    tim2.resume();

    analogWriteFrequency(1000u);
    analogWriteResolution(10);
    analogWrite(PIN_PWM_PUMP, 0);
    analogWrite(PIN_PWM_VALVE, 0);

    uart_proto_send_line("A,IDLE\r\n");
}

static uint32_t g_seq = 0;
static BpState g_prev_state = BP_STATE_IDLE;

void loop(void)
{
    uart_proto_poll_rx();

    if (!g_tick_100hz)
        return;
    g_tick_100hz = false;

    int start = digitalRead(PIN_BTN_START) == LOW ? 1 : 0;
    int stop = digitalRead(PIN_BTN_STOP) == LOW ? 1 : 0;
    int high = digitalRead(PIN_BTN_HIGH) == LOW ? 1 : 0;

    int16_t counts = 0;
    if (ads1115_read_channel0_counts(&g_ads, &counts) != 0)
        bp_fsm_sensor_i2c_fail();
    else
        bp_fsm_sensor_i2c_ok();

    float p_mmhg = pressure_counts_to_mmhg(counts);
    uint32_t now = millis();

    bp_fsm_on_tick(now, p_mmhg, start, stop, high);

    BpState stt = bp_fsm_get_state();
    /* Gửi A/E trước S trong cùng tick đổi trạng thái — dễ thấy trên monitor/plotter. */
    if (stt != g_prev_state) {
        if (stt == BP_STATE_INFLATE_SLOW_LISTEN)
            uart_proto_send_line("A,INFLATE_SLOW\r\n");
        else if (stt == BP_STATE_INFLATE_TO_MARGIN)
            uart_proto_send_line("A,INFLATE_MARGIN\r\n");
        else if (stt == BP_STATE_DEFLATE_MEASURE)
            uart_proto_send_line("A,DEFLATE\r\n");
        else if (stt == BP_STATE_FAST_DEFLATE)
            uart_proto_send_line("A,FAST_DEFLATE\r\n");
        else if (stt == BP_STATE_DONE)
            uart_proto_send_line("E,MEAS_END\r\n");
        else if (stt == BP_STATE_ERROR)
            uart_proto_send_line("E,SENSOR_OR_LEAK\r\n");
        else if (stt == BP_STATE_IDLE && g_prev_state != BP_STATE_IDLE)
            uart_proto_send_line("A,IDLE\r\n");
    }

    uart_proto_send_sample(g_seq++, now, p_mmhg);

    apply_pwm_outputs();
    led_hmi_task(bp_fsm_led_hmi_state());

    g_prev_state = stt;
}
