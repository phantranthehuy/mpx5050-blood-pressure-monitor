# Checklist kiểm thử firmware (bổ sung plan)

Ghi chú kết quả (✓ / ngày / ghi chú). Tham chiếu đầy đủ phần cứng: [`testcase.md`](testcase.md).

## An toàn & SAF

| ID | Mục | Kỳ vọng |
|----|-----|---------|
| SAF-T01 | **SAF-01** STOP đang bơm | Nhấn `SW_STOP`: bơm tắt ngay, van mở tối đa, đỏ nhấp nháy nhanh (~150 ms) |
| SAF-T02 | **SAF-02** trần **175 mmHg** (mặc định; có thể đổi `SAF`/`SAFH` UART) | Áp ≥ trần hiệu lực: bơm ngắt, van mở, LED khẩn cấp |
| SAF-T03 | Lệnh `T,300` | Host gửi target > SAF hiệu lực → firmware clamp đúng trần (không vượt SAF) |
| SAF-T04 | `ABORT` / `A` serial | Trong đo: xả nhanh + LED khẩn cấp như STOP |

## UART đóng vòng

| ID | Mục | Kỳ vọng |
|----|-----|---------|
| COM-T01 | Stream `S,...` | Trong inflate/deflate: ~100 dòng/giây; Arduino: `S` đầy đủ trường debug (xem `pc-bridge.md`) |
| COM-T02 | `T,<mmHg>` | Sau detect ảo: MCU chuyển `A,INFLATE_MARGIN`, ramp tới target (≤ trần SAF hiệu lực) |
| COM-T03 | Mất serial giữa chừng | Fallback inflate timeout; SAF/STOP vẫn hoạt động |
| COM-T04 | `SAF` / `SAFH` / `HIGH` | Gửi từ WebApp: MCU chấp nhận; quá áp / clamp `T` theo trần đã đặt |

## LED / HMI (LED_Algorithm)

| ID | Trạng thái | Kỳ vọng |
|----|------------|---------|
| LED-T01 | Idle | Xanh sáng (PB14), đỏ/vàng tắt |
| LED-T02 | Inflate slow/margin | Vàng nhấp nháy ~500 ms (PB15) |
| LED-T03 | Deflate measure | Vàng sáng giữ |
| LED-T04 | Fast deflate sau đo (P≤40) | Vàng sáng giữ (không đỏ nhấp nháy) |
| LED-T05 | STOP / SAF / ABORT fast | Đỏ nhấp nháy ~150 ms (PB13) |
| LED-T06 | Error (leak/I2C) | Đỏ sáng liên tục |

## Chức năng đo (tham testcase SYS)

| ID | Mục | Kỳ vọng |
|----|-----|---------|
| SYS-T01 | Start → bơm chậm | `A,INFLATE_SLOW`, áp tăng ~10 mmHg/s (điều chỉnh PWM) |
| SYS-T02 | WebApp gửi `T,...` | Chuyển margin rồi deflate khi đạt target |
| SYS-T03 | Deflate | `A,DEFLATE`, stream tiếp cho envelope WebApp |
| SYS-T04 | Kết thúc | `E,MEAS_END`, về idle xanh |
