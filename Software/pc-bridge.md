# PC / WebApp bridge — UART protocol

Firmware stream áp suất **100 Hz** trong mọi pha có đo (`INFLATE_SLOW_LISTEN`, `INFLATE_TO_MARGIN`, `DEFLATE_MEASURE`, …). WebApp (repo riêng) đọc cổng serial hoặc TCP bridge và xử lý tín hiệu.

## Thông số vật lý (đồng bộ plan)

- **Baud:** 115200, 8N1  
- **Tốc độ bơm chậm:** ~10 mmHg/s (`STATE_INFLATE_SLOW_LISTEN`) — servo PWM trên MCU  
- **SAF trần áp:** mặc định **175 mmHg** (`PRESSURE_SAFE_MAX_MMHG` trong `board_config.h`) — MCU tắt bơm, mở van, LED khẩn cấp khi áp ≥ trần hiệu lực.  
- **Lệnh target từ host:** clamp **≤** trần SAF hiệu lực (thường / HA — xem `SAF` / `SAFH` bên dưới).

### Đặt trần an toàn & chế độ HA (UART)

Giá trị clamp trong khoảng **120–300 mmHg** (giống firmware).

```
SAF,<mmHg>\r\n
SAFH,<mmHg>\r\n
HIGH,<0|1>\r\n
```

- **`SAF`:** trần thường (khi không ở chế độ HA).  
- **`SAFH`:** trần khi **HIGH** hiệu lực (nút phần cứng HIGH **hoặc** `HIGH,1` từ host).  
- **`HIGH,1` / `HIGH,0`:** bật/tắt chế độ huyết áp cao từ web (OR với nút HIGH trên board). Khi HA: fallback bơm +15 mmHg và dùng `SAFH` cho quá áp + clamp `T`.

Mặc định sau reset: `SAF` = `SAFH` = 175 mmHg (compile-time), có thể đổi lại bằng các lệnh trên.

## Khung TX (MCU → host)

### Stream mẫu (mỗi tick ~10 ms)

**Arduino / PlatformIO** (đầy đủ trường debug):

```
S,<seq>,<t_ms>,<p_mmHg>,<rc>,<counts>,<fsm>,<pump_pct>,<valve_pct>,<btn_s><btn_p><btn_h>\r\n
```

- `rc`: mã đọc ADS1115 (0 = OK).  
- `counts`: mẫu ADC (int16).  
- `fsm`: 0=IDLE 1=INFLATE_SLOW 2=INFLATE_MARGIN 3=DEFLATE 4=FAST_DEFLATE 5=DONE 6=ERROR.  
- `pump_pct` / `valve_pct`: PWM % (0–100).  
- `btn_s`, `btn_p`, `btn_h`: ba ký tự 0/1 — nút START / STOP / HIGH (đã đảo active-low).

**HAL CMake** (rút gọn): `S,<seq>,<t_ms>,<p_mmHg>\r\n` — WebApp chỉ cần 3 trường đầu sau `S,`.

- `seq`: số thứ tự mẫu tăng dần (uint32)  
- `t_ms`: thời gian ms trên MCU  
- `p_mmHg`: áp cuff (mmHg) sau chuyển đổi trên MCU

### Thông báo trạng thái / kết thúc

| Chuỗi | Ý nghĩa |
|-------|---------|
| `A,IDLE\r\n` | Sẵn sàng |
| `A,INFLATE_SLOW\r\n` | Đang bơm chậm + chờ WebApp “nghe” |
| `A,INFLATE_MARGIN\r\n` | Đang ramp tới target sau lệnh `T,...` |
| `A,DEFLATE\r\n` | Xả chậm đo oscillometric |
| `A,FAST_DEFLATE\r\n` | Xả nhanh (cuối chu kỳ hoặc an toàn) |
| `E,MEAS_END\r\n` | Kết thúc đo (áp đã xả thấp) |
| `E,SENSOR_OR_LEAK\r\n` | Lỗi cảm biến / hở khí (timeout tăng áp) |

## Khung RX (host → MCU)

Kết thúc dòng bằng `\n` (CR có thể bị bỏ qua).

### Đặt mục tiêu áp cuff (sau khi WebApp ước lượng SYS)

```
T,<target_mmHg>\r\n
```

Ví dụ WebApp tính `P_sys_est + 40` → gửi `T,165` (sẽ bị clamp nếu > 175).

### Hủy / dừng đo từ host

```
ABORT\r\n
```

hoặc rút gọn:

```
A\r\n
```

Tương đương yêu cầu xả nhanh an toàn (giống STOP phần cứng về hành vi xả).

### Tốc độ xả chậm (Arduino PIO / WebApp)

```
DR,<mmHg/s>\r\n
EARLYEND\r\n
```

- **`DR`:** setpoint tốc độ giảm áp trong pha xả đo (clamp firmware, thường 0.8–4 mmHg/s).  
- **`EARLYEND`:** yêu cầu kết thúc xả chậm sớm khi đang đo (theo điều kiện firmware).

### Bắt đầu đo từ WebApp (lệnh `START`)

Khi FSM đang **IDLE** (hoặc vừa về IDLE sau đo), MCU **chỉ** vào pha bơm chậm nếu có **nút START phần cứng** hoặc lệnh:

```
START\r\n
```

(hoa thường tùy ý). Hành vi tương đương một lần nhấn START trên board. Sau đó host gửi `T,...` trong pha `INFLATE_SLOW` như luồng oscillometric.

### Xác nhận đã parse lệnh (MCU → host)

Sau khi parse thành công một lệnh RX hợp lệ, firmware gửi một dòng ngắn (ASCII):

```
R,OK,<từ_khóa>\r\n
```

Ví dụ: `R,OK,START`, `R,OK,T`, `R,OK,SAF`, `R,OK,SAFH`, `R,OK,HIGH`, `R,OK,DR`, `R,OK,ABORT`. WebApp có thể hiển thị để biết MCU đã nhận dòng lệnh (khác với `A,...` là thông báo **trạng thái FSM**).

**Lưu ý HAL CMake:** xác nhận `R,...` được đưa vào hàng đợi trong ngắt RX và gửi từ vòng `main` (không gọi `HAL_UART_Transmit` trực tiếp trong ISR).

## Luồng WebApp (đề xuất)

1. **Band-pass 0.5–5 Hz** trên chuỗi áp (hoặc high-pass quanh đường bao chậm).  
2. Gửi `START` (hoặc bấm START trên board) để vào pha bơm chậm.  
3. Trong pha bơm chậm, phát hiện **dao động đầu tiên** đủ biên độ → ghi **`P_sys_est`** (đồng bộ `t_ms` / `seq`).  
4. Gửi `T,<P_sys_est + margin>\r\n` (margin mặc định firmware/WebApp ~40 mmHg), clamp theo SAF hiệu lực.  
5. **Tùy chọn — cắt sớm (host):** nếu áp cuff ≥ ngưỡng cấu hình và `|AC|` yên đủ lâu trong pha bơm, WebApp có thể gửi `T,...` sớm hơn mà không cần chạm trần SAF.  
6. Trong pha xả, chạy **MAA / envelope** để MAP, SBP/DBP (theo [`algorithm.md`](../algorithm.md)).

## LED (tóm tắt UX phần cứng)

| Trạng thái | Đèn |
|------------|-----|
| Idle / Done | Xanh sáng |
| Inflate (slow + margin) | Vàng nhấp nháy ~500 ms |
| Đo / xả bình thường | Vàng sáng |
| STOP / ≥175 / ABORT → fast deflate | Đỏ nhấp nháy ~150 ms |
| Error (leak / I2C) | Đỏ sáng |

Chi tiết: [`LED_Algorithm.md`](../LED_Algorithm.md).

## Fallback không có WebApp

Firmware sau timeout có thể chuyển sang ramp fallback (`INFLATE_FALLBACK_TARGET_MMHG` trong `board_config.h`). SAF **175 mmHg** và STOP **luôn** áp dụng.
