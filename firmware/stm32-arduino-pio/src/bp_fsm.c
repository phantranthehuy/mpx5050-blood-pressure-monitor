#include "bp_fsm.h"
#include "board_config.h"
#include <math.h>

#ifndef BP_I2C_FAIL_THRESH
#define BP_I2C_FAIL_THRESH 8u
#endif

static BpState state = BP_STATE_IDLE;
static BpErrorReason error_reason = BP_ERROR_NONE;

static float pressure_prev = 0.f;
static uint32_t prev_tick_ms = 0;

static float inflate_target_mmhg = INFLATE_FALLBACK_TARGET_MMHG;
static uint8_t host_target_pending = 0;

static uint32_t inflate_enter_ms = 0;
static float pressure_at_inflate_start = 0.f;

static uint32_t i2c_fail_streak = 0;
static uint32_t done_since_ms = 0;
static uint32_t overpressure_since_ms = 0;
static uint32_t leak_suspect_since_ms = 0;

static uint8_t pump_pwm = 0;
static uint8_t valve_pwm = VALVE_CLOSED_DUTY;

/** Nút HIGH phần cứng (cập nhật mỗi tick). */
static uint8_t s_btn_high = 0;
static uint8_t s_stop_pressed_prev = 0;
/** Chế độ HA từ host (UART `HIGH,1`). */
static uint8_t g_host_high = 0;
static float g_saf_mmhg = PRESSURE_SAFE_MAX_MMHG;
static float g_saf_high_mmhg = PRESSURE_SAFE_MAX_MMHG;
/** 1: host gửi lệnh START qua UART — xử lý trong IDLE giống nút START phần cứng. */
static uint8_t uart_start_req = 0;

/** Host hủy đo: tick sau chuyển về xả chậm (không dùng FAST_DEFLATE). */
static uint8_t s_host_abort_req = 0;

/** DEFLATE_MEASURE → DONE: đếm tick liên tiếp áp ≤ MEASURE_END_PRESSURE_MMHG. */
static uint8_t s_deflate_done_streak = 0;

/** dp/dt (mmHg/s) mẫu gần nhất — cho UART log / chẩn đoán. */
static float s_last_dp_dt_mmhg_s = 0.f;

/** Servo xả chậm: EMA(dp_dt) + tích phân err để bù khi lâu không đạt rate. */
static float s_deflate_dp_ema = 0.f;
static float s_deflate_spd_int_mmhg = 0.f;
static uint8_t s_deflate_servo_seeded = 0;

static void reset_deflate_measure_servo(void)
{
    s_deflate_dp_ema = 0.f;
    s_deflate_spd_int_mmhg = 0.f;
    s_deflate_servo_seeded = 0;
}

/** 1: FAST_DEFLATE do nút STOP; 0: do host EARLYEND (LED vẫn vàng đo). */
static uint8_t s_fast_deflate_emergency = 0;

/** millis lúc vào FAST_DEFLATE / ERROR (xả van max) — áp dụng thời gian tối thiểu xả. */
static uint32_t s_fast_deflate_enter_ms;
static uint32_t s_error_enter_ms;

static float g_deflate_slow_rate_mmhg_s = DEFLATE_SLOW_RATE_MMHG_S;

static uint8_t s_host_early_done_req = 0;

/** Van mở tối đa; emergency=1 → LED đỏ (STOP), 0 → LED vàng (EARLYEND). */
static void enter_fast_deflate(uint8_t emergency_led, uint32_t now_ms)
{
    state = BP_STATE_FAST_DEFLATE;
    pump_pwm = 0;
    valve_pwm = VALVE_FULL_OPEN_DUTY;
    s_fast_deflate_emergency = emergency_led;
    s_fast_deflate_enter_ms = now_ms;
}

/** Chỉ nút STOP: xả nhanh (van mở tối đa). */
static void enter_stop_fast_deflate(uint32_t now_ms)
{
    enter_fast_deflate(1u, now_ms);
}

static void enter_error(BpErrorReason reason, uint32_t now_ms)
{
    state = BP_STATE_ERROR;
    error_reason = reason;
    pump_pwm = 0;
    valve_pwm = VALVE_FULL_OPEN_DUTY;
    s_error_enter_ms = now_ms;
}

static void enter_deflate_measure(void)
{
    state = BP_STATE_DEFLATE_MEASURE;
    pump_pwm = 0;
    valve_pwm = (uint8_t)VALVE_DEFLATE_INITIAL_LOGIC_PCT;
    s_deflate_done_streak = 0;
    reset_deflate_measure_servo();
}

static void enter_idle_vent(void)
{
    state = BP_STATE_IDLE_VENT;
    pump_pwm = 0;
    valve_pwm = VALVE_FULL_OPEN_DUTY;
}

void bp_fsm_init(void)
{
    state = BP_STATE_IDLE;
    error_reason = BP_ERROR_NONE;
    pump_pwm = 0;
    valve_pwm = VALVE_CLOSED_DUTY;
    host_target_pending = 0;
    i2c_fail_streak = 0;
    overpressure_since_ms = 0;
    leak_suspect_since_ms = 0;
    uart_start_req = 0;
    s_stop_pressed_prev = 0;
    s_host_abort_req = 0;
    s_deflate_done_streak = 0;
    g_deflate_slow_rate_mmhg_s = DEFLATE_SLOW_RATE_MMHG_S;
    s_host_early_done_req = 0;
    s_fast_deflate_emergency = 0;
    s_fast_deflate_enter_ms = 0;
    s_error_enter_ms = 0;
    s_last_dp_dt_mmhg_s = 0.f;
}

void bp_fsm_host_request_uart_start(void)
{
    uart_start_req = 1u;
}

BpState bp_fsm_get_state(void)
{
    return state;
}

BpErrorReason bp_fsm_get_error_reason(void)
{
    return error_reason;
}

uint8_t bp_fsm_get_pump_pwm_percent(void)
{
    return pump_pwm;
}

uint8_t bp_fsm_get_valve_pwm_percent(void)
{
    return valve_pwm;
}

float bp_fsm_get_last_dp_dt_mmhg_s(void)
{
    return s_last_dp_dt_mmhg_s;
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

/** Trần % logic van khi xả chậm đo: cuff ≥ p_hi → cap_hi; dưới p_hi hai đoạn nối tại BREAK để mở nhanh hơn khi cuff < ~160 mmHg (bám mmHg/s). */
static float deflate_measure_max_valve_logic_pct(float cuff_mmhg)
{
    const float p_hi = DEFLATE_SLOW_CAP_PRESSURE_MMHG_HIGH;
    const float p_lo = DEFLATE_SLOW_CAP_PRESSURE_MMHG_LOW;
    const float brk = DEFLATE_SLOW_CAP_CURVE_BREAK_MMHG;
    const float cap_hi = (float)DEFLATE_SLOW_VALVE_PCT_CAP_HIGH;
    const float cap_lo = (float)DEFLATE_SLOW_VALVE_PCT_CAP_LOW;
    const float v_brk = (float)DEFLATE_SLOW_VALVE_PCT_AT_BREAK;

    if (cuff_mmhg >= p_hi)
        return cap_hi;
    if (cuff_mmhg <= p_lo)
        return cap_lo;
    if (brk <= p_lo || brk >= p_hi)
        return cap_hi + (cap_lo - cap_hi) * (p_hi - cuff_mmhg) / (p_hi - p_lo);
    if (cuff_mmhg >= brk) {
        const float d = p_hi - brk;
        if (d < 1e-3f)
            return v_brk;
        return cap_hi + (v_brk - cap_hi) * (p_hi - cuff_mmhg) / d;
    }
    const float d2 = brk - p_lo;
    if (d2 < 1e-3f)
        return cap_lo;
    return v_brk + (cap_lo - v_brk) * (brk - cuff_mmhg) / d2;
}

/** Tiền khấng % logic van: mở sẵn theo áp cuff + vmax để bám mmHg/s; PI chỉ bù lệch. */
static float deflate_measure_feedforward_valve_pct(float cuff_mmhg, float vmax, float target_rate_mmhg_s)
{
    const float p_hi = DEFLATE_SLOW_CAP_PRESSURE_MMHG_HIGH;
    const float ref_lo = DEFLATE_SLOW_FF_PRESSURE_REF_LO;
    const float brk = DEFLATE_SLOW_CAP_CURVE_BREAK_MMHG;
    const float vmin = (float)VALVE_DEFLATE_LOGIC_MIN_PCT;
    float span = vmax - vmin;
    if (span < 1.f)
        span = 1.f;

    float pos = (p_hi - cuff_mmhg) / (p_hi - ref_lo);
    if (pos < 0.f)
        pos = 0.f;
    if (pos > 1.f)
        pos = 1.f;
    if (cuff_mmhg >= p_hi)
        pos = fmaxf(pos, DEFLATE_SLOW_FF_POS_MIN_ABOVE_P_HI);

    /* Sàn pos mượt (smoothstep) theo DEFLATE_SLOW_FF_SMOOTH_FLOOR_* — dải hẹp quanh ~170 mmHg
     * để không xả chậm dần từ 170 trở xuống; nối mượt với ramp dưới BREAK. */
    const float c_sf0 = DEFLATE_SLOW_FF_SMOOTH_FLOOR_CUFF_HI_MMHG;
    const float c_sf1 = DEFLATE_SLOW_FF_SMOOTH_FLOOR_CUFF_LO_MMHG;
    const float sf_a = DEFLATE_SLOW_FF_SMOOTH_FLOOR_AT_HI;
    const float sf_b = DEFLATE_SLOW_FF_SMOOTH_FLOOR_AT_LO;
    float pos_floor_smooth = sf_a;
    if (cuff_mmhg < c_sf0) {
        float spanf = c_sf0 - c_sf1;
        if (spanf < 1.f)
            spanf = 1.f;
        float u = (c_sf0 - cuff_mmhg) / spanf;
        if (u > 1.f)
            u = 1.f;
        if (u < 0.f)
            u = 0.f;
        float w = u * u * (3.f - 2.f * u);
        pos_floor_smooth = sf_a + w * (sf_b - sf_a);
    }
    pos = fmaxf(pos, pos_floor_smooth);

    if (cuff_mmhg < brk) {
        float ramp_mm = DEFLATE_SLOW_FF_FLOOR_RAMP_MM;
        if (ramp_mm < 5.f)
            ramp_mm = 5.f;
        float ur = (brk - cuff_mmhg) / ramp_mm;
        if (ur > 1.f)
            ur = 1.f;
        if (ur < 0.f)
            ur = 0.f;
        float wr = ur * ur * (3.f - 2.f * ur);
        float blended = pos_floor_smooth * (1.f - wr) + wr * DEFLATE_SLOW_FF_POS_FLOOR_BELOW_BREAK;
        pos = fmaxf(pos, blended);
    }

    /* krate: hơi nhạy hơn khi target 6–10 mmHg/s để FF mở đủ theo setpoint. */
    float krate = 0.91f + 0.075f * (target_rate_mmhg_s * (1.f / 3.f));
    if (krate > 1.14f)
        krate = 1.14f;
    if (krate < 0.86f)
        krate = 0.86f;

    float v = vmin + span * pos * DEFLATE_SLOW_FF_FRAC_OF_SPAN * krate;
    if (v > vmax)
        v = vmax;
    if (v < vmin)
        v = vmin;
    return v;
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
    s_host_abort_req = 1u;
}

void bp_fsm_host_set_deflate_rate_mmhg_s(float mmhg_per_s)
{
    if (mmhg_per_s < DEFLATE_SLOW_RATE_UART_MIN_MMHG_S)
        mmhg_per_s = DEFLATE_SLOW_RATE_UART_MIN_MMHG_S;
    if (mmhg_per_s > DEFLATE_SLOW_RATE_UART_MAX_MMHG_S)
        mmhg_per_s = DEFLATE_SLOW_RATE_UART_MAX_MMHG_S;
    g_deflate_slow_rate_mmhg_s = mmhg_per_s;
}

void bp_fsm_host_request_early_measure_done(void)
{
    s_host_early_done_req = 1u;
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
    s_btn_high = high_pressed ? 1u : 0u;
    uint8_t stop_now = stop_pressed ? 1u : 0u;
    uint8_t stop_edge = (stop_now != 0u && s_stop_pressed_prev == 0u) ? 1u : 0u;
    s_stop_pressed_prev = stop_now;
    const float saf_cap = effective_safe_cap_mmhg();

    if (i2c_fail_streak >= BP_I2C_FAIL_THRESH && state != BP_STATE_IDLE && state != BP_STATE_IDLE_VENT &&
        state != BP_STATE_DONE && state != BP_STATE_ERROR) {
        enter_error(BP_ERROR_I2C_SENSOR, now_ms);
        return;
    }

    /* Khi đang chờ ở IDLE: STOP/ABORT toggle mở van tay, bấm lại thì đóng về IDLE. */
    if (stop_edge && state == BP_STATE_IDLE) {
        enter_idle_vent();
        return;
    }
    if (stop_edge && state == BP_STATE_IDLE_VENT) {
        state = BP_STATE_IDLE;
        pump_pwm = 0;
        valve_pwm = VALVE_CLOSED_DUTY;
        return;
    }

    /* SAF-01 STOP → FAST_DEFLATE (xả nhanh), duy nhất lối vào FAST_DEFLATE. */
    if (stop_pressed && state != BP_STATE_IDLE && state != BP_STATE_IDLE_VENT &&
        state != BP_STATE_DONE && state != BP_STATE_ERROR) {
        enter_stop_fast_deflate(now_ms);
        return;
    }

    /* Host: đủ bao MAA + cuff đủ thấp → xả nhanh phần đuôi (không coi là STOP khẩn cấp). */
    if (s_host_early_done_req) {
        s_host_early_done_req = 0;
        if (state == BP_STATE_DEFLATE_MEASURE)
            enter_fast_deflate(0u, now_ms);
    }

    /* SAF-02 quá áp: debounce nhiễu ADC/I2C ngắn, nhưng vẫn xả ngay nếu vượt xa trần. */
    if (state != BP_STATE_ERROR && state != BP_STATE_IDLE_VENT) {
        if (pressure_mmhg >= saf_cap + SAF_OVERPRESSURE_IMMEDIATE_MARGIN_MMHG) {
            enter_error(BP_ERROR_OVERPRESSURE, now_ms);
            return;
        }
        if (pressure_mmhg >= saf_cap) {
            if (overpressure_since_ms == 0u)
                overpressure_since_ms = now_ms;
            if ((now_ms - overpressure_since_ms) >= SAF_OVERPRESSURE_DEBOUNCE_MS) {
                enter_error(BP_ERROR_OVERPRESSURE, now_ms);
                return;
            }
        } else {
            overpressure_since_ms = 0u;
        }
    }

    /* Host ABORT: giống giữ nút STOP phần cứng — xả nhanh (SAF-T04 / pc-bridge.md). */
    if (s_host_abort_req) {
        s_host_abort_req = 0;
        if (state == BP_STATE_IDLE) {
            enter_idle_vent();
            return;
        }
        if (state == BP_STATE_IDLE_VENT) {
            state = BP_STATE_IDLE;
            pump_pwm = 0;
            valve_pwm = VALVE_CLOSED_DUTY;
            return;
        }
        if (state != BP_STATE_DONE && state != BP_STATE_ERROR)
            enter_stop_fast_deflate(now_ms);
    }

    float dt_s = 0.01f;
    if (prev_tick_ms != 0u && now_ms > prev_tick_ms) {
        dt_s = (float)(now_ms - prev_tick_ms) * 0.001f;
        if (dt_s < 0.001f) dt_s = 0.001f;
    }

    float dp_dt = 0.f;
    if (prev_tick_ms != 0u)
        dp_dt = (pressure_mmhg - pressure_prev) / dt_s;
    s_last_dp_dt_mmhg_s = dp_dt;

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
            leak_suspect_since_ms = 0;
            host_target_pending = 0;
            g_deflate_slow_rate_mmhg_s = DEFLATE_SLOW_RATE_MMHG_S;
            float fb = INFLATE_FALLBACK_TARGET_MMHG +
                       (((s_btn_high != 0u) || (g_host_high != 0u)) ? 15.f : 0.f);
            inflate_target_mmhg = clampf(fb, 40.f, saf_cap - 5.f);
            /* Soft-start: bắt đầu ở PUMP_PWM_MIN, các tick sau ramp_pump_to() sẽ
             * nhích lên theo slope cap. Tránh kick 15 % gây sụt áp tức thời. */
            pump_pwm = PUMP_PWM_MIN;
            valve_pwm = VALVE_CLOSED_DUTY;
        }
        break;

    case BP_STATE_IDLE_VENT:
        pump_pwm = 0;
        valve_pwm = VALVE_FULL_OPEN_DUTY;
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

        /* Fallback: nếu host chưa gửi T nhưng đã đạt target mặc định thì bắt đầu xả đo. */
        if (pressure_mmhg >= inflate_target_mmhg - 2.f) {
            enter_deflate_measure();
            break;
        }

        /* Fallback phụ: timeout chờ lệnh thì chuyển sang bơm tới target mặc định. */
        if ((now_ms - inflate_enter_ms) > INFLATE_FALLBACK_AFTER_MS) {
            state = BP_STATE_INFLATE_TO_MARGIN;
            ramp_pump_to((int)pump_pwm + 20, now_ms);
            break;
        }

        /* SAF-03 leak: chỉ báo lỗi nếu áp thấp bất thường kéo dài, tránh nhiễu một mẫu. */
        if ((now_ms - inflate_enter_ms) > LEAK_TIMEOUT_MS) {
            if (pressure_mmhg < pressure_at_inflate_start + LEAK_MIN_RISE_MMHG) {
                if (leak_suspect_since_ms == 0u)
                    leak_suspect_since_ms = now_ms;
                if ((now_ms - leak_suspect_since_ms) >= LEAK_CONFIRM_MS) {
                    enter_error(BP_ERROR_LEAK, now_ms);
                    break;
                }
            } else {
                leak_suspect_since_ms = 0u;
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
            enter_deflate_measure();
            break;
        }

        /* Bơm nhanh hơn slow-listen, nhưng vẫn qua slope cap để không nhảy 55 % ngay */
        float err = (inflate_target_mmhg - pressure_mmhg);
        int target = (int)clampf(20.f + err * 1.2f, 35.f, (float)PUMP_PWM_MAX);
        ramp_pump_to(target, now_ms);

        /* Không vượt SAF — đã check đầu tick */
        break;
    }

    case BP_STATE_DEFLATE_MEASURE: {
        pump_pwm = 0;
        float target_drop = g_deflate_slow_rate_mmhg_s;

        float vmax = deflate_measure_max_valve_logic_pct(pressure_mmhg);
        if (vmax > (float)VALVE_DEFLATE_LOGIC_MAX_PCT)
            vmax = (float)VALVE_DEFLATE_LOGIC_MAX_PCT;

        float v_ff = deflate_measure_feedforward_valve_pct(pressure_mmhg, vmax, target_drop);

        if (!s_deflate_servo_seeded) {
            s_deflate_dp_ema = dp_dt;
            s_deflate_servo_seeded = 1;
        } else {
            const float a = DEFLATE_SLOW_DP_EMA_ALPHA;
            s_deflate_dp_ema = a * dp_dt + (1.f - a) * s_deflate_dp_ema;
        }

        float err_f = s_deflate_dp_ema + target_drop;

        const uint8_t valve_prev = valve_pwm;

        if (err_f > DEFLATE_SLOW_INTEGRAL_ERR_ON_MMHGS) {
            s_deflate_spd_int_mmhg += err_f * dt_s;
            if (s_deflate_spd_int_mmhg > DEFLATE_SLOW_INTEGRAL_CAP_MMHG)
                s_deflate_spd_int_mmhg = DEFLATE_SLOW_INTEGRAL_CAP_MMHG;
        } else if (err_f < -DEFLATE_SLOW_INTEGRAL_ERR_OFF_MMHGS) {
            /* Gần vmax, nhiễu dp âm ngắn khiến err_f < 0 liên tục co ∫ → mất bù, van không lên tới
             * trần → áp “đứng” ~130–150 mmHg. Chỉ decay khi van chưa gần mở tối đa hoặc xả quá nhanh rõ (|err| lớn). */
            if (err_f < -1.0f || (float)valve_prev < vmax - 2.5f)
                s_deflate_spd_int_mmhg *= DEFLATE_SLOW_INTEGRAL_DECAY;
        }

        float err_gain = DEFLATE_SLOW_VALVE_ERR_GAIN;
        if (pressure_mmhg < DEFLATE_SLOW_ERR_GAIN_LOWPRESS_BELOW_MMHG)
            err_gain *= DEFLATE_SLOW_VALVE_ERR_GAIN_LOWPRESS_MULT;

        float v_pi = err_f * err_gain + DEFLATE_SLOW_INTEGRAL_KI * s_deflate_spd_int_mmhg;
        v_pi *= DEFLATE_SLOW_PI_ON_FF_SCALE;

        float v_des = v_ff + v_pi;
        v_des = clampf(v_des, (float)VALVE_DEFLATE_LOGIC_MIN_PCT, vmax);

        int vstep = (int)VALVE_DEFLATE_MAX_STEP_PER_TICK;
        if (pressure_mmhg < DEFLATE_SLOW_ERR_GAIN_LOWPRESS_BELOW_MMHG &&
            (int)DEFLATE_SLOW_VALVE_MAX_STEP_LOWPRESS > vstep)
            vstep = (int)DEFLATE_SLOW_VALVE_MAX_STEP_LOWPRESS;
        if (s_deflate_spd_int_mmhg >= DEFLATE_SLOW_INTEGRAL_BOOST_THRESH_MMHG &&
            (int)DEFLATE_SLOW_VALVE_MAX_STEP_BOOST > vstep)
            vstep = (int)DEFLATE_SLOW_VALVE_MAX_STEP_BOOST;

        /* Vùng cuff giữa: kẹp bướm PWM/tick để bớt giật; 4 %/tick khiến van bám v_des chậm → áp tụt chậm dần <170. */
        if (pressure_mmhg < 178.f && pressure_mmhg > 96.f) {
            const int mid_vstep_cap = 6;
            if (vstep > mid_vstep_cap)
                vstep = mid_vstep_cap;
        }

        float v_prev = (float)valve_pwm;
        float v_next = clampf(v_des, v_prev - (float)vstep, v_prev + (float)vstep);
        valve_pwm = (uint8_t)v_next;

        /* Đo bình thường: xả chậm đến khi áp đủ thấp → DONE (không qua FAST_DEFLATE). */
        if (pressure_mmhg <= MEASURE_END_PRESSURE_MMHG) {
            if (s_deflate_done_streak < 250u)
                s_deflate_done_streak++;
        } else {
            s_deflate_done_streak = 0;
        }
        if (s_deflate_done_streak >= DEFLATE_MEASURE_DONE_DEBOUNCE_TICKS) {
            state = BP_STATE_DONE;
            done_since_ms = now_ms;
            pump_pwm = 0;
            valve_pwm = VALVE_CLOSED_DUTY;
            s_deflate_done_streak = 0;
        }
        break;
    }

    case BP_STATE_FAST_DEFLATE:
        pump_pwm = 0;
        valve_pwm = VALVE_FULL_OPEN_DUTY;
        if (pressure_mmhg <= FAST_DEFLATE_END_PRESSURE_MMHG &&
            (now_ms - s_fast_deflate_enter_ms) >= FAST_DEFLATE_MIN_DURATION_MS) {
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
        if (start_pressed || uart_start_req) {
            uart_start_req = 0;
            bp_fsm_init();
            break;
        }
        /* Áp đủ thấp + đủ thời gian xả khẩn cấp tối thiểu → về IDLE (START/UART thoát ngay ở trên). */
        if (pressure_mmhg <= FAST_DEFLATE_END_PRESSURE_MMHG &&
            (now_ms - s_error_enter_ms) >= BP_ERROR_VENT_MIN_DURATION_MS)
            bp_fsm_init();
        break;
    }
}

LedHmiSystemState_t bp_fsm_led_hmi_state(void)
{
    switch (state) {
    case BP_STATE_IDLE:
        return LED_SYS_STATE_IDLE;
    case BP_STATE_IDLE_VENT:
        return LED_SYS_STATE_EMERGENCY;
    case BP_STATE_INFLATE_SLOW_LISTEN:
    case BP_STATE_INFLATE_TO_MARGIN:
        return LED_SYS_STATE_INFLATING;
    case BP_STATE_DEFLATE_MEASURE:
        return LED_SYS_STATE_MEASURING;
    case BP_STATE_FAST_DEFLATE:
        return (s_fast_deflate_emergency != 0u) ? LED_SYS_STATE_EMERGENCY : LED_SYS_STATE_MEASURING;
    case BP_STATE_DONE:
        return LED_SYS_STATE_IDLE;
    case BP_STATE_ERROR:
        return LED_SYS_STATE_ERROR;
    default:
        return LED_SYS_STATE_IDLE;
    }
}
