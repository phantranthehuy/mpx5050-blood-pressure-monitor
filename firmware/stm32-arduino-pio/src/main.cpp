/**
 * Port song song với firmware HAL (`firmware/stm32-hal-cmake/Core/Src/main.c`):
 * TIM3 100 Hz tick, PWM bơm/van 1 kHz qua TIM2 (analogWrite trên PA0/PA1),
 * đọc ADS1115, stream UART, FSM + LED.
 *
 * Lưu ý: PA0=TIM2_CH1, PA1=TIM2_CH2 → analogWrite() chiếm TIM2 cho PWM 1 kHz,
 * nên ngắt 100 Hz phải dùng timer khác (chọn TIM3 vì cũng general-purpose,
 * không vướng chân nào đang sử dụng).
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

/* TIM3 dùng cho ngắt 100 Hz; TIM2 để analogWrite() PWM 1 kHz lên PA0/PA1. */
static HardwareTimer tim_tick(TIM3);

static volatile bool g_tick_100hz = false;

static Ads1115_Handle g_ads;

static void tick_100hz_callback(void)
{
    g_tick_100hz = true;
}

/** Ánh xạ % logic FSM (0=giữ áp … 100=xả max) → % PWM chân van. */
static int valve_logic_to_pin_pct(uint32_t logic_pct)
{
    if (logic_pct > 100u)
        logic_pct = 100u;
    const int seal = (int)VALVE_PIN_AT_LOGIC_SEAL;
    const int vent = (int)VALVE_PIN_AT_LOGIC_VENT;
    int pin = seal + (vent - seal) * (int)logic_pct / 100;
    if (pin < 0)
        pin = 0;
    if (pin > 100)
        pin = 100;
    return pin;
}

static void apply_pwm_outputs(void)
{
    uint32_t pump = bp_fsm_get_pump_pwm_percent();
    uint32_t valve_logic = bp_fsm_get_valve_pwm_percent();
    if (pump > 100u) pump = 100u;
    if (valve_logic > 100u) valve_logic = 100u;
    const int valve_pin = valve_logic_to_pin_pct(valve_logic);
    /* 10-bit: 0–1023 ~ 0–100 % (tương đương HAL ARR=999, compare=pump*10) */
    analogWrite(PIN_PWM_PUMP, (int)((pump * 1023u) / 100u));
    analogWrite(PIN_PWM_VALVE, (int)(((uint32_t)valve_pin * 1023u) / 100u));
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
    Wire.setClock(100000);

    uart_proto_init();

    ads1115_init(&g_ads);

    bp_fsm_init();

    /* PWM bơm/van trên TIM2 (PA0/PA1) — set TRƯỚC khi cấu hình TIM3 tick,
     * vì analogWriteFrequency() trong STM32duino reconfig timer của các pin
     * đã analogWrite. Gọi đúng thứ tự để khỏi đụng vào TIM3. */
    analogWriteFrequency(1000u);
    analogWriteResolution(10);
    /* Ghi PWM theo cùng ánh xạ FSM→chân van (tránh lệch một nhịp với raw 0). */
    apply_pwm_outputs();

    tim_tick.setOverflow(100, HERTZ_FORMAT);
    tim_tick.attachInterrupt(tick_100hz_callback);
    tim_tick.resume();

    uart_proto_send_line("A,IDLE\r\n");
}

static uint32_t g_seq = 0;
static BpState g_prev_state = BP_STATE_IDLE;
static float g_last_valid_pressure_mmhg = 0.0f;

static const char *error_reason_line(BpErrorReason reason)
{
    switch (reason) {
    case BP_ERROR_I2C_SENSOR:
        return "E,SENSOR_I2C\r\n";
    case BP_ERROR_OVERPRESSURE:
        return "E,OVERPRESSURE\r\n";
    case BP_ERROR_LEAK:
        return "E,LEAK\r\n";
    case BP_ERROR_NONE:
    default:
        return "E,ERROR\r\n";
    }
}

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
    int rc = ads1115_read_channel0_counts(&g_ads, &counts);
    if (rc != 0)
        bp_fsm_sensor_i2c_fail();
    else
        bp_fsm_sensor_i2c_ok();

    float p_mmhg = g_last_valid_pressure_mmhg;
    if (rc == 0) {
        p_mmhg = pressure_counts_to_mmhg(counts);
        g_last_valid_pressure_mmhg = p_mmhg;
    }
    uint32_t now = millis();

    bp_fsm_on_tick(now, p_mmhg, start, stop, high);

    /* PWM + LED ngay sau FSM: Serial.write (USB CDC) có thể block lâu khi host không
     * đọc kịp — nếu gửi UART trước apply_pwm thì bơm/van kẹt ở duty tick trước. */
    apply_pwm_outputs();
    led_hmi_task(bp_fsm_led_hmi_state());

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
            uart_proto_send_line(error_reason_line(bp_fsm_get_error_reason()));
        else if (stt == BP_STATE_IDLE_VENT)
            uart_proto_send_line("A,IDLE_VENT\r\n");
        else if (stt == BP_STATE_IDLE && g_prev_state != BP_STATE_IDLE)
            uart_proto_send_line("A,IDLE\r\n");
    }

    float dps = bp_fsm_get_last_dp_dt_mmhg_s();
    int dp_centi = (int)(dps * 100.f + (dps >= 0.f ? 0.5f : -0.5f));
    if (dp_centi > 32000)
        dp_centi = 32000;
    if (dp_centi < -32000)
        dp_centi = -32000;

    uart_proto_send_sample(g_seq++, now, p_mmhg, rc, counts,
                           (int)stt,
                           (int)bp_fsm_get_pump_pwm_percent(),
                           (int)bp_fsm_get_valve_pwm_percent(),
                           start, stop, high, dp_centi);

    g_prev_state = stt;
}
