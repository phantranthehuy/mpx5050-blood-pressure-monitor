/**
 * Hằng số ứng dụng (áp suất, timeout, PWM %) — dùng chung bp_fsm / pressure.
 * Map chân nằm trong board_pins.h (Arduino).
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

#define UART_BAUD 115200u

/* ADS1115 ADDR→GND = 0x48 (Wire dùng địa chỉ 7-bit) */
#define ADS1115_I2C_ADDR_7BIT ((uint8_t)0x48u)

#define PRESSURE_SAFE_MAX_MMHG           185.0f
/** Giới clamp cho lệnh UART `SAF` / `SAFH` (mmHg). */
#define PRESSURE_SAFE_UART_MIN_MMHG      120.0f
#define PRESSURE_SAFE_UART_MAX_MMHG      300.0f
#define LEAK_TIMEOUT_MS                  10000u
#define LEAK_MIN_RISE_MMHG               10.0f
#define LEAK_CONFIRM_MS                  1500u
#define BP_I2C_FAIL_THRESH               50u
#define SAF_OVERPRESSURE_DEBOUNCE_MS     300u
#define SAF_OVERPRESSURE_IMMEDIATE_MARGIN_MMHG 8.0f
#define INFLATE_FALLBACK_TARGET_MMHG     180.0f
#define INFLATE_FALLBACK_AFTER_MS        120000u
#define TARGET_MARGIN_DEFAULT_MMHG       40.0f

#define TARGET_PRESSURE_RATE_MMHG_S      10.0f
/** Tốc độ giảm áp mặc định trong DEFLATE_MEASURE (mmHg/s). Nhanh hơn Omron điển hình (~2.5–4), vẫn trong vùng đo dao động ổn định. WebApp: `DR,<rate>`. */
#define DEFLATE_SLOW_RATE_MMHG_S         8.0f
/** Clamp cho lệnh UART `DR,...` (mmHg/s). */
#define DEFLATE_SLOW_RATE_UART_MIN_MMHG_S  2.5f
#define DEFLATE_SLOW_RATE_UART_MAX_MMHG_S  10.0f
/** Hệ số P servo van (trên dp_dt đã lọc EMA). */
#define DEFLATE_SLOW_VALVE_ERR_GAIN      1.68f
/** EMA trên dp_dt trong DEFLATE_MEASURE (0–1). Thấp hơn → lọc nhiễu, van ít giật vùng ~150–170. */
#define DEFLATE_SLOW_DP_EMA_ALPHA        0.34f
/** Tích phân sai số tốc độ (err = dp_ema + rate, mmHg/s): bù khi lâu không đạt rate. */
#define DEFLATE_SLOW_INTEGRAL_KI         0.70f
/** Trần tích phân ∫(err·dt) với err (mmHg/s); thặng dư tích lũy khi xả chậm hơn setpoint. */
#define DEFLATE_SLOW_INTEGRAL_CAP_MMHG   32.f
/** Khi err âm (xả quá nhanh): co tích phân để tránh wind-up. */
#define DEFLATE_SLOW_INTEGRAL_DECAY      0.88f
/** Chỉ tích phân khi err > ngưỡng (lọc nhiễu gần setpoint). */
#define DEFLATE_SLOW_INTEGRAL_ERR_ON_MMHGS   0.12f
/** err âm hơn ngưỡng này → co tích phân (rộng hơn → ít co vì nhiễu dp). */
#define DEFLATE_SLOW_INTEGRAL_ERR_OFF_MMHGS  0.24f
/** Tích phân ≥ ngưỡng → cho phép bước van lớn hơn (đuổi kịp setpoint). */
#define DEFLATE_SLOW_INTEGRAL_BOOST_THRESH_MMHG  2.35f
#define DEFLATE_SLOW_VALVE_MAX_STEP_BOOST      8u
/** Ngưỡng áp cuff (mmHg) coi là “cực cao”: chỉ vùng này dùng CAP_HIGH phẳng (ΔP lớn → dễ xả vèo).
 *  Trước đây =135 khiến mọi áp ≥135 (gồm ~150) cùng một trần van quá thấp → ~0.5 mmHg/s. */
#define DEFLATE_SLOW_CAP_PRESSURE_MMHG_HIGH  172.0f
#define DEFLATE_SLOW_CAP_PRESSURE_MMHG_LOW    48.0f
/** Áp gối (mmHg): trên break nội suy từ cap_hi; dưới break nội suy nhanh tới cap_lo (bám mmHg/s khi cuff ~<160). */
#define DEFLATE_SLOW_CAP_CURVE_BREAK_MMHG    162.0f
/** Trần % logic van (nội suy theo áp cuff): cuff cao → cap_hi (ít mở); cuff giảm dần → nội suy tới cap_lo (mở nhiều hơn). */
#define DEFLATE_SLOW_VALVE_PCT_CAP_HIGH       47u
#define DEFLATE_SLOW_VALVE_PCT_CAP_LOW        80u
/** Trần van tại áp = DEFLATE_SLOW_CAP_CURVE_BREAK_MMHG (nối hai đoạn cong). */
#define DEFLATE_SLOW_VALVE_PCT_AT_BREAK       58u
/** Nhân thêm lên err_gain khi cuff thấp (chỉ áp dụng dưới DEFLATE_SLOW_ERR_GAIN_LOWPRESS_BELOW_MMHG). */
#define DEFLATE_SLOW_VALVE_ERR_GAIN_LOWPRESS_MULT  1.08f
/** Chỉ tăng gain vùng áp rất thấp; tránh hunt khi vừa qua BREAK (~160). */
#define DEFLATE_SLOW_ERR_GAIN_LOWPRESS_BELOW_MMHG  92.0f
/** Giảm nhẹ nhánh PI khi đã có feedforward (tránh overshoot / dao động). */
#define DEFLATE_SLOW_PI_ON_FF_SCALE           0.82f
/** Feedforward: mẫu số áp thấp (mmHg) — nhỏ hơn p_lo → pos tăng nhanh hơn khi cuff giảm. */
#define DEFLATE_SLOW_FF_PRESSURE_REF_LO       52.0f
/** Feedforward: phần khoảng [min,vmax] mở “mặc định” (0–1). */
#define DEFLATE_SLOW_FF_FRAC_OF_SPAN          0.985f
/** Sàn pos tối đa (sau ramp) khi cuff thấp — ramp DEFLATE_SLOW_FF_FLOOR_RAMP_MM tránh nhảy tại BREAK. */
#define DEFLATE_SLOW_FF_POS_FLOOR_BELOW_BREAK 0.78f
/** Khoảng mmHg dưới BREAK để sàn FF tăng dần 0→FLOOR (smoothstep); hết 0 tại BREAK, đủ ở ~110. */
#define DEFLATE_SLOW_FF_FLOOR_RAMP_MM         48.0f
/** Smoothstep cuff: dải hẹp quanh ~170 mmHg để pos_sfloor tăng nhanh (trước đây 176→150 quá dài → dưới 170 xả chậm dần). */
#define DEFLATE_SLOW_FF_SMOOTH_FLOOR_CUFF_HI_MMHG    174.0f
#define DEFLATE_SLOW_FF_SMOOTH_FLOOR_CUFF_LO_MMHG    168.0f
#define DEFLATE_SLOW_FF_SMOOTH_FLOOR_AT_HI           0.11f
#define DEFLATE_SLOW_FF_SMOOTH_FLOOR_AT_LO           0.42f
/** Feedforward: cuff ≥ p_hi: pos tối thiểu (vẫn thận trọng ở ΔP lớn). */
#define DEFLATE_SLOW_FF_POS_MIN_ABOVE_P_HI    0.11f
/** Bước slew van tối đa/tick khi cuff < DEFLATE_SLOW_ERR_GAIN_LOWPRESS_BELOW_MMHG (100 Hz). */
#define DEFLATE_SLOW_VALVE_MAX_STEP_LOWPRESS  7u
/** DEFLATE_MEASURE: áp ≤ ngưỡng này (liên tục debounce) → DONE (không dùng FAST_DEFLATE). */
#define MEASURE_END_PRESSURE_MMHG        15.0f
/** Chỉ FAST_DEFLATE (STOP / EARLYEND / …) và xả van max khi ERROR: kết thúc xả nhanh khi áp ≤ gấp đôi ngưỡng kết thúc pha đo chậm (van đã max, rút ngắn đuôi xả). */
#define FAST_DEFLATE_END_PRESSURE_MMHG   (MEASURE_END_PRESSURE_MMHG * 2.0f)
/** Thời gian tối thiểu (ms) giữ FAST_DEFLATE trước khi cho phép chuyển DONE theo áp (kéo dài pha xả nhanh). */
#define FAST_DEFLATE_MIN_DURATION_MS     10000u
/** Thời gian tối thiểu (ms) xả khẩn cấp ở ERROR (van max) trước khi tự về IDLE theo áp. START/UART vẫn thoát ngay. */
#define BP_ERROR_VENT_MIN_DURATION_MS    10000u
/** Số tick @100 Hz liên tiếp áp ≤ MEASURE_END_PRESSURE_MMHGGLOBAL GUARDS (moi tick, truoc switch(state))
---------------------------------------------------------------
- I2C fail streak (khong o IDLE/IDLE_VENT/DONE/ERROR) -> ERROR
- STOP edge: IDLE <-> IDLE_VENT (toggle)
- STOP pressed (active states) -> FAST_DEFLATE (emergency)
- EARLYEND (host) + state=DEFLATE_MEASURE -> FAST_DEFLATE (non-emergency)
- Overpressure (vuot tran an toan) -> ERROR
- ABORT: IDLE->IDLE_VENT, IDLE_VENT->IDLE, else -> FAST_DEFLATE
---------------------------------------------------------------


MAIN FSM
========

                 STOP edge / ABORT
        +-----------------------------------+
        |                                   v
+-------------------+                 +-------------------+
|       IDLE        |<--------------->|     IDLE_VENT     |
| pump=0, valve=cl  |                 | pump=0, valve=open|
+---------+---------+                 +-------------------+
          |
          | START or UART START
          v
+---------------------------+
| INFLATE_SLOW_LISTEN       |
| pump ramp, valve closed   |
+-------------+-------------+
  | host target pending  -> INFLATE_TO_MARGIN
  | pressure >= target-2 -> DEFLATE_MEASURE
  | timeout -> INFLATE_TO_MARGIN
  | leak detected -> ERROR
  v
+---------------------------+
| INFLATE_TO_MARGIN         |
| pump faster, valve closed |
+-------------+-------------+
  | pressure >= target-2
  v
+---------------------------+
| DEFLATE_MEASURE            |
| pump=0, valve servo slow   |
+-------------+-------------+
  | pressure <= MEASURE_END for N ticks
  v
+---------------------------+
|           DONE            |
| pump=0, valve closed      |
+-------------+-------------+
  | after ~800 ms
  v
+---------------------------+
|           IDLE            |
+---------------------------+


FAST_DEFLATE (entered by STOP/ABORT or EARLYEND)
+---------------------------+
| FAST_DEFLATE              |
| pump=0, valve fully open  |
+-------------+-------------+
  | pressure <= FAST_END and min duration
  v
+---------------------------+
|           DONE            |
+---------------------------+


ERROR (entered by I2C fail / overpressure / leak)
+---------------------------+
|           ERROR           |
| pump=0, valve fully open  |
+-------------+-------------+
  | START or UART START -> bp_fsm_init() -> IDLE
  | pressure <= FAST_END and min duration -> IDLE mới chuyển DONE (tránh nhiễu). */
#define DEFLATE_MEASURE_DONE_DEBOUNCE_TICKS  6u

#define PUMP_PWM_MIN                     5u
/** Trần PWM bơm (0–100). Giảm nhẹ nếu nguồn/USB sụt khi motor chạy; tăng lại khi đã gia cố tụ/dây. */
#define PUMP_PWM_MAX                     26u

/* --- Soft-start chống brownout do dòng inrush motor bơm ---
 * Trong PUMP_SOFTSTART_MS đầu tiên sau khi rời IDLE, kẹp PWM ≤
 * PUMP_SOFTSTART_CAP_PCT để dòng đỉnh không kéo sụt rail VCC.
 * (CAP phải < PUMP_PWM_MAX thì khoảng đầu mới thật sự nhẹ hơn trần toàn cục.)
 * Đồng thời mọi thay đổi pump_pwm trong các state INFLATE bị cap
 * theo PUMP_SLOPE_PER_TICK_MAX (đơn vị %/tick @100Hz). */
#define PUMP_SOFTSTART_MS                900u
#define PUMP_SOFTSTART_CAP_PCT           20u
#define PUMP_SLOPE_PER_TICK_MAX          1

#define VALVE_CLOSED_DUTY                0u
#define VALVE_FULL_OPEN_DUTY             100u

/** % logic FSM (0 = giữ áp, 100 = xả tối đa) → % PWM thật trên chân van (PA1).
 *  pin = SEAL + (VENT - SEAL) * logic / 100.
 *  Board hiện tại: SEAL=0, VENT=100 (PWM thấp = đóng xả, cao = mở xả).
 *  Nếu trước đây phải dùng VALVE_NORMALLY_OPEN=1: đặt SEAL=100, VENT=0. */
#define VALVE_PIN_AT_LOGIC_SEAL          0u
#define VALVE_PIN_AT_LOGIC_VENT          100u

/** Lúc mới vào DEFLATE_MEASURE: mở van thấp rồi servo kéo (tránh nhảy mạnh). */
#define VALVE_DEFLATE_INITIAL_LOGIC_PCT  14u
/** Giới hạn bước chỉnh van mỗi tick @100 Hz trong DEFLATE_MEASURE (logic %). */
#define VALVE_DEFLATE_MAX_STEP_PER_TICK    6u
/** Biên logic % van trong DEFLATE_MEASURE (phải chứa INITIAL). */
#define VALVE_DEFLATE_LOGIC_MIN_PCT        2u
#define VALVE_DEFLATE_LOGIC_MAX_PCT        85u

/* --- MPX5050 + ADS1115 (đọc counts → mmHg trong pressure.c) ---
 * MPX5050 (datasheet Rev 11): Vout = Vs * (0.018 * P_kPa + 0.04).
 * ADS1115 build_cfg(): PGA ±4.096 V → LSB = 4.096/32768 V/count. */
#define MPX5050_VS_VOLTS        5.0f
#define ADS1115_PGA_FSR_VOLTS   4.096f

#endif
