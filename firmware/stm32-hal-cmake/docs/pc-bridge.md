# PC / WebApp bridge — UART protocol

Firmware stream áp suất **100 Hz** trong mọi pha có đo (`INFLATE_SLOW_LISTEN`, `INFLATE_TO_MARGIN`, `DEFLATE_MEASURE`, …). WebApp (repo riêng) đọc cổng serial hoặc TCP bridge và xử lý tín hiệu.

## Thông số vật lý (đồng bộ plan)

- **Baud:** 115200, 8N1  
- **Tốc độ bơm chậm:** ~10 mmHg/s (`STATE_INFLATE_SLOW_LISTEN`) — servo PWM trên MCU  
- **SAF trần áp:** **280 mmHg** — MCU tắt bơm, mở van, LED khẩn cấp  
- **Lệnh target từ host:** clamp **≤ 280 mmHg** (cùng ngưỡng SAF)

## Khung TX (MCU → host)

### Stream mẫu (mỗi tick ~10 ms)

```
S,<seq>,<t_ms>,<p_mmHg>\r\n
```

- `seq`: số thứ tự mẫu tăng dần (uint32)  
- `t_ms`: `HAL_GetTick()`  
- `p_mmhg`: áp cuff sau chuyển đổi tối thiểu trên MCU (cần hiệu chỉnh offset/scale trong firmware)

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

Ví dụ WebApp tính `P_sys_est + 40` → gửi `T,165` (sẽ bị clamp nếu > 280).

### Hủy / dừng đo từ host

```
ABORT\r\n
```

hoặc rút gọn:

```
A\r\n
```

Tương đương yêu cầu xả nhanh an toàn (giống STOP phần cứng về hành vi xả).

## Luồng WebApp (đề xuất)

1. **Band-pass 0.5–5 Hz** trên chuỗi áp (hoặc high-pass quanh đường bao chậm).  
2. Trong pha bơm chậm, phát hiện **dao động đầu tiên** đủ biên độ → ghi **`P_sys_est`** (đồng bộ `t_ms` / `seq`).  
3. Gửi `T,<P_sys_est + margin>\r\n` (margin mặc định firmware/WebApp ~40 mmHg).  
4. Trong pha xả, chạy **MAA / envelope** để MAP, SBP/DBP (theo [`algorithm.md`](../algorithm.md)).

## LED (tóm tắt UX phần cứng)

| Trạng thái | Đèn |
|------------|-----|
| Idle / Done | Xanh sáng |
| Inflate (slow + margin) | Vàng nhấp nháy ~500 ms |
| Đo / xả bình thường | Vàng sáng |
| STOP / ≥280 / ABORT → fast deflate | Đỏ nhấp nháy ~150 ms |
| Error (leak / I2C) | Đỏ sáng |

Chi tiết: [`LED_Algorithm.md`](../LED_Algorithm.md).

## Fallback không có WebApp

Firmware sau timeout có thể chuyển sang ramp fallback (`INFLATE_FALLBACK_TARGET_MMHG` trong `board_config.h`). SAF **280 mmHg** và STOP **luôn** áp dụng.
