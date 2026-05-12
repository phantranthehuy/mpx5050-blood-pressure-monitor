#include "bp_fsm.h"
#include "board_config.h"

#ifndef BP_I2C_FAIL_THRESH
#define BP_I2C_FAIL_THRESH 8u
#endif

static BpState state = BP_STATE_IDLE;

static float pressure_prev = 0.f;
static uint32_t prev_tick_ms = 0;

static float inflate_target_mmhg = INFLATE_FALLBACK_TARGET_MMHG;
static uint8_t host_target_pending = 0;

static uint32_t inflate_enter_ms = 0;
static float pressure_at_inflate_start = 0.f;

static uint32_t i2c_fail_streak = 0;
static uint32_t done_since_ms = 0;

static uint8_t pump_pwm = 0;
static uint8_t valve_pwm = VALVE_CLOSED_DUTY;

static uint8_t s_btn_high = 0;
static uint8_t g_host_high = 0;
static float g_saf_mmhg = PRESSURE_SAFE_MAX_MMHG;
static float g_saf_high_mmhg = PRESSURE_SAFE_MAX_MMHG;
static uint8_t fast_deflate_emergency_led = 0;
static uint8_t uart_start_req = 0;

static void enter_fast_deflate(uint8_t emergency_led)
{
    state = BP_STATE_FAST_DEFLATE;
    pump_pwm = 0;
    valve_pwm = VALVE_FULL_OPEN_DUTY;
    fast_deflate_emergency_led = emergency_led ? 1u : 0u;
}

static void enter_error(void)
{
    state = BP_STATE_ERROR;
    pump_pwm = 0;
    valve_pwm = VALVE_FULL_OPEN_DUTY;
}

void bp_fsm_init(void)
{
    state = BP_STATE_IDLE;
    pump_pwm = 0;
    valve_pwm = VALVE_CLOSED_DUTY;
    host_target_pending = 0;
    i2c_fail_streak = 0;
    fast_deflate_emergency_led = 0;
    uart_start_req = 0;
}

void bp_fsm_host_request_uart_start(void)
{
    uart_start_req = 1u;
}

BpState bp_fsm_get_state(void)
{
    return state;
}

uint8_t bp_fsm_get_pump_pwm_percent(void)
{
    return pump_pwm;
}

uint8_t bp_fsm_get_valve_pwm_percent(void)
{
    return valve_pwm;
}

void bp_fsm_sensor_i2c_fail(void)
{
    i2c_fail_streak++;
}

void bp_fsm_sensor_i2c_ok(void)
{
    i2c_fail_streak = 0;
}

static float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static float effective_safe_cap_mmhg(void)
{
    uint8_t hi = ((s_btn_high != 0u) || (g_host_high != 0u)) ? 1u : 0u;
    return hi ? g_saf_high_mmhg : g_saf_mmhg;
}

void bp_fsm_host_set_saf_mmhg(float mmhg)
{
    mmhg = clampf(mmhg, PRESSURE_SAFE_UART_MIN_MMHG, PRESSURE_SAFE_UART_MAX_MMHG);
    g_saf_mmhg = mmhg;
}

void bp_fsm_host_set_saf_high_mmhg(float mmhg)
{
    mmhg = clampf(mmhg, PRESSURE_SAFE_UART_MIN_MMHG, PRESSURE_SAFE_UART_MAX_MMHG);
    g_saf_high_mmhg = mmhg;
}

void bp_fsm_host_set_high_uart(uint8_t on)
{
    g_host_high = on ? 1u : 0u;
}

void bp_fsm_host_set_target_mmhg(float target_mmhg)
{
    float cap = effective_safe_cap_mmhg();
    if (target_mmhg > cap) target_mmhg = cap;
    if (target_mmhg < 30.f) target_mmhg = 30.f;
    inflate_target_mmhg = target_mmhg;
    host_target_pending = 1;
}

void bp_fsm_host_abort_measure(void)
{
    enter_fast_deflate(1u);
}

void bp_fsm_on_tick(uint32_t now_ms, float pressure_mmhg,
                    int start_pressed, int stop_pressed, int high_pressed)
{
    s_btn_high = high_pressed ? 1u : 0u;
    const float saf_cap = effective_safe_cap_mmhg();

    if (i2c_fail_streak >= BP_I2C_FAIL_THRESH && state != BP_STATE_IDLE &&
        state != BP_STATE_DONE && state != BP_STATE_ERROR) {
        enter_error();
        return;
    }

    if (stop_pressed && state != BP_STATE_IDLE && state != BP_STATE_DONE && state != BP_STATE_ERROR) {
        enter_fast_deflate(1u);
        return;
    }

    if (pressure_mmhg >= saf_cap) {
        enter_fast_deflate(1u);
        return;
    }

    float dt_s = 0.01f;
    if (prev_tick_ms != 0u && now_ms > prev_tick_ms) {
        dt_s = (float)(now_ms - prev_tick_ms) * 0.001f;
        if (dt_s < 0.001f) dt_s = 0.001f;
    }

    float dp_dt = 0.f;
    if (prev_tick_ms != 0u)
        dp_dt = (pressure_mmhg - pressure_prev) / dt_s;

    pressure_prev = pressure_mmhg;
    prev_tick_ms = now_ms;

    switch (state) {
    case BP_STATE_IDLE:
        pump_pwm = 0;
        valve_pwm = VALVE_CLOSED_DUTY;
        if (start_pressed || uart_start_req) {
            uart_start_req = 0;
            state = BP_STATE_INFLATE_SLOW_LISTEN;
            inflate_enter_ms = now_ms;
            pressure_at_inflate_start = pressure_mmhg;
            host_target_pending = 0;
            float fb = INFLATE_FALLBACK_TARGET_MMHG +
                       (((s_btn_high != 0u) || (g_host_high != 0u)) ? 15.f : 0.f);
            inflate_target_mmhg = clampf(fb, 40.f, saf_cap - 5.f);
            pump_pwm = PUMP_PWM_MIN + 10u;
            valve_pwm = VALVE_CLOSED_DUTY;
        }
        break;

    case BP_STATE_INFLATE_SLOW_LISTEN: {
        valve_pwm = VALVE_CLOSED_DUTY;

        if (host_target_pending) {
            host_target_pending = 0;
            state = BP_STATE_INFLATE_TO_MARGIN;
            pump_pwm = (uint8_t)clampf((float)pump_pwm + 25.f, (float)PUMP_PWM_MIN, (float)PUMP_PWM_MAX);
            break;
        }

        if ((now_ms - inflate_enter_ms) > INFLATE_FALLBACK_AFTER_MS) {
            state = BP_STATE_INFLATE_TO_MARGIN;
            pump_pwm = (uint8_t)clampf((float)pump_pwm + 20.f, (float)PUMP_PWM_MIN, (float)PUMP_PWM_MAX);
            break;
        }

        if ((now_ms - inflate_enter_ms) > LEAK_TIMEOUT_MS) {
            if (pressure_mmhg < pressure_at_inflate_start + 10.f) {
                enter_error();
                break;
            }
        }

        float err = TARGET_PRESSURE_RATE_MMHG_S - dp_dt;
        int adj = (int)(err * 2.f);
        int np = (int)pump_pwm + adj;
        np = (int)clampf((float)np, (float)PUMP_PWM_MIN, (float)PUMP_PWM_MAX);
        pump_pwm = (uint8_t)np;

        if (dp_dt < 2.f && (now_ms - inflate_enter_ms) > 3000u)
            pump_pwm = (uint8_t)clampf((float)pump_pwm + 1.f, (float)PUMP_PWM_MIN, (float)PUMP_PWM_MAX);

        break;
    }

    case BP_STATE_INFLATE_TO_MARGIN: {
        valve_pwm = VALVE_CLOSED_DUTY;

        if (pressure_mmhg >= inflate_target_mmhg - 2.f) {
            state = BP_STATE_DEFLATE_MEASURE;
            pump_pwm = 0;
            valve_pwm = 35u;
            break;
        }

        float err = (inflate_target_mmhg - pressure_mmhg);
        uint8_t fast = (uint8_t)clampf(55.f + err * 1.5f, 35.f, (float)PUMP_PWM_MAX);
        pump_pwm = fast;

        break;
    }

    case BP_STATE_DEFLATE_MEASURE: {
        pump_pwm = 0;
        float target_drop = DEFLATE_SLOW_RATE_MMHG_S;
        float err = dp_dt + target_drop;
        int vadj = (int)(err * 3.f);
        int vv = (int)valve_pwm + vadj;
        vv = (int)clampf((float)vv, 18.f, 85.f);
        valve_pwm = (uint8_t)vv;

        if (pressure_mmhg <= 40.f)
            enter_fast_deflate(0u);
        break;
    }

    case BP_STATE_FAST_DEFLATE:
        pump_pwm = 0;
        valve_pwm = VALVE_FULL_OPEN_DUTY;
        if (pressure_mmhg <= 15.f) {
            state = BP_STATE_DONE;
            done_since_ms = now_ms;
        }
        break;

    case BP_STATE_DONE:
        pump_pwm = 0;
        valve_pwm = VALVE_CLOSED_DUTY;
        if ((now_ms - done_since_ms) > 800u)
            state = BP_STATE_IDLE;
        break;

    case BP_STATE_ERROR:
        pump_pwm = 0;
        valve_pwm = VALVE_FULL_OPEN_DUTY;
        if (start_pressed) {
            bp_fsm_init();
        }
        break;
    }
}

LedHmiSystemState_t bp_fsm_led_hmi_state(void)
{
    switch (state) {
    case BP_STATE_IDLE:
        return LED_SYS_STATE_IDLE;
    case BP_STATE_INFLATE_SLOW_LISTEN:
    case BP_STATE_INFLATE_TO_MARGIN:
        return LED_SYS_STATE_INFLATING;
    case BP_STATE_DEFLATE_MEASURE:
        return LED_SYS_STATE_MEASURING;
    case BP_STATE_FAST_DEFLATE:
        return fast_deflate_emergency_led ? LED_SYS_STATE_EMERGENCY : LED_SYS_STATE_MEASURING;
    case BP_STATE_DONE:
        return LED_SYS_STATE_IDLE;
    case BP_STATE_ERROR:
        return LED_SYS_STATE_ERROR;
    default:
        return LED_SYS_STATE_IDLE;
    }
}
