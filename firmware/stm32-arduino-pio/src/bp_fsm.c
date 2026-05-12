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

static uint8_t high_mode = 0;
/** 1 khi vào FAST_DEFLATE do STOP / quá áp / ABORT host → LED_EMERGENCY nhấp nháy nhanh. */
static uint8_t fast_deflate_emergency_led = 0;

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
    high_mode = 0;
    fast_deflate_emergency_led = 0;
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

void bp_fsm_host_set_target_mmhg(float target_mmhg)
{
    if (target_mmhg > PRESSURE_SAFE_MAX_MMHG) target_mmhg = PRESSURE_SAFE_MAX_MMHG;
    if (target_mmhg < 30.f) target_mmhg = 30.f;
    inflate_target_mmhg = target_mmhg;
    host_target_pending = 1;
}

void bp_fsm_host_abort_measure(void)
{
    enter_fast_deflate(1u);
}

static float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* Ramp pump_pwm về phía `target` với 2 giới hạn:
 *   (1) slope cap ±PUMP_SLOPE_PER_TICK_MAX %/tick → tránh đỉnh dòng motor;
 *   (2) trong PUMP_SOFTSTART_MS đầu sau khi vào INFLATE, kẹp ≤ PUMP_SOFTSTART_CAP_PCT.
 * Trả về giá trị mới đã ghi vào pump_pwm. */
static uint8_t ramp_pump_to(int target_pct, uint32_t now_ms)
{
    if (target_pct < (int)PUMP_PWM_MIN) target_pct = (int)PUMP_PWM_MIN;
    if (target_pct > (int)PUMP_PWM_MAX) target_pct = (int)PUMP_PWM_MAX;

    int cur = (int)pump_pwm;
    int delta = target_pct - cur;
    int slope = (int)PUMP_SLOPE_PER_TICK_MAX;
    if (delta > slope) delta = slope;
    if (delta < -slope) delta = -slope;
    int np = cur + delta;

    if ((now_ms - inflate_enter_ms) < PUMP_SOFTSTART_MS) {
        if (np > (int)PUMP_SOFTSTART_CAP_PCT)
            np = (int)PUMP_SOFTSTART_CAP_PCT;
    }
    if (np < (int)PUMP_PWM_MIN) np = (int)PUMP_PWM_MIN;
    if (np > (int)PUMP_PWM_MAX) np = (int)PUMP_PWM_MAX;
    pump_pwm = (uint8_t)np;
    return pump_pwm;
}

void bp_fsm_on_tick(uint32_t now_ms, float pressure_mmhg,
                    int start_pressed, int stop_pressed, int high_pressed)
{
    high_mode = high_pressed ? 1u : 0u;

    if (i2c_fail_streak >= BP_I2C_FAIL_THRESH && state != BP_STATE_IDLE &&
        state != BP_STATE_DONE && state != BP_STATE_ERROR) {
        enter_error();
        return;
    }

    /* SAF-01 Emergency stop → LED_EMERGENCY */
    if (stop_pressed && state != BP_STATE_IDLE && state != BP_STATE_DONE && state != BP_STATE_ERROR) {
        enter_fast_deflate(1u);
        return;
    }

    /* SAF-02 over-pressure (280 mmHg testcase) */
    if (pressure_mmhg >= PRESSURE_SAFE_MAX_MMHG) {
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
        if (start_pressed) {
            state = BP_STATE_INFLATE_SLOW_LISTEN;
            inflate_enter_ms = now_ms;
            pressure_at_inflate_start = pressure_mmhg;
            host_target_pending = 0;
            float fb = INFLATE_FALLBACK_TARGET_MMHG + (high_mode ? 15.f : 0.f);
            inflate_target_mmhg = clampf(fb, 40.f, PRESSURE_SAFE_MAX_MMHG - 5.f);
            /* Soft-start: bắt đầu ở PUMP_PWM_MIN, các tick sau ramp_pump_to() sẽ
             * nhích lên theo slope cap. Tránh kick 15 % gây sụt áp tức thời. */
            pump_pwm = PUMP_PWM_MIN;
            valve_pwm = VALVE_CLOSED_DUTY;
        }
        break;

    case BP_STATE_INFLATE_SLOW_LISTEN: {
        valve_pwm = VALVE_CLOSED_DUTY;

        /* Chờ WebApp gửi T,... → chuyển margin (slope cap vẫn áp khi sang state mới) */
        if (host_target_pending) {
            host_target_pending = 0;
            state = BP_STATE_INFLATE_TO_MARGIN;
            ramp_pump_to((int)pump_pwm + 25, now_ms);
            break;
        }

        /* Fallback: timeout chờ lệnh */
        if ((now_ms - inflate_enter_ms) > INFLATE_FALLBACK_AFTER_MS) {
            state = BP_STATE_INFLATE_TO_MARGIN;
            ramp_pump_to((int)pump_pwm + 20, now_ms);
            break;
        }

        /* SAF-03 leak: không tăng áp trong LEAK_TIMEOUT_MS */
        if ((now_ms - inflate_enter_ms) > LEAK_TIMEOUT_MS) {
            if (pressure_mmhg < pressure_at_inflate_start + 10.f) {
                enter_error();
                break;
            }
        }

        /* Servo ~TARGET_PRESSURE_RATE_MMHG_S — target tuyệt đối, ramp_pump_to() cap slope */
        float err = TARGET_PRESSURE_RATE_MMHG_S - dp_dt;
        int target = (int)pump_pwm + (int)(err * 2.f);
        ramp_pump_to(target, now_ms);

        /* Quá chậm trong thời gian dài → nâng nhẹ (vẫn qua slope cap) */
        if (dp_dt < 2.f && (now_ms - inflate_enter_ms) > 3000u)
            ramp_pump_to((int)pump_pwm + 1, now_ms);

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

        /* Bơm nhanh hơn slow-listen, nhưng vẫn qua slope cap để không nhảy 55 % ngay */
        float err = (inflate_target_mmhg - pressure_mmhg);
        int target = (int)clampf(55.f + err * 1.5f, 35.f, (float)PUMP_PWM_MAX);
        ramp_pump_to(target, now_ms);

        /* Không vượt SAF — đã check đầu tick */
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
