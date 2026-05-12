# Firmware Arduino — STM32F103 Blue Pill (PlatformIO)

Đây là **port song song** với bản CMake + HAL trong [`../stm32-hal-cmake/`](../stm32-hal-cmake/): cùng **STM32F103C8T6**, cùng map chân và **cùng giao thức UART** với WebApp/PC (xem [`../stm32-hal-cmake/docs/pc-bridge.md`](../stm32-hal-cmake/docs/pc-bridge.md)).

**Lưu ý:** Khi đổi thuật toán đo / FSM, nên coi bản **HAL CMake** là chuẩn tham chiếu rồi đồng bộ sang thư mục này (hoặc ngược lại — nhưng tránh chỉ sửa một bên).

## Công cụ

- [PlatformIO Core](https://platformio.org/install) (`python3 -m pip install --user platformio`)
- Nạp: **ST-Link** (mặc định `upload_protocol = stlink` trong [`platformio.ini`](platformio.ini)); có thể đổi sang `serial` nếu dùng bootloader UART.

## Build & nạp

```bash
cd firmware/stm32-arduino-pio
python3 -m platformio run
python3 -m platformio run -t upload
python3 -m platformio device monitor -b 115200
```

## Map chân (trùng HAL)

Định nghĩa trong [`src/board_pins.h`](src/board_pins.h) và hằng số áp suất trong [`src/board_config.h`](src/board_config.h):

| Chức năng | Chân |
|-----------|------|
| UART1 115200 | PA9 TX, PA10 RX (`Serial`) |
| I2C1 (ADS1115 `0x48`) | PB6 SCL, PB7 SDA (`Wire`) |
| PWM bơm / van | PA6, PA7 (`analogWrite`, 1 kHz) |
| Nút (active LOW, pull-up) | PA3 Start, PA4 Stop, PA5 High |
| LED | PB13 đỏ, PB14 xanh, PB15 vàng |

## Khác biệt so với bản HAL

- **ADS1115:** driver dùng `Wire` (địa chỉ 7-bit), không dùng `HAL_I2C_*`.
- **UART:** `Serial` + `uart_proto_poll_rx()` trong vòng lặp (polling), tương đương luồng lệnh `T,...` / `ABORT` như HAL.
- **100 Hz:** `HardwareTimer` trên **TIM2** (`setOverflow(100, HERTZ_FORMAT)`).
- **PWM:** `analogWriteFrequency(1000)` (toàn cục trên core STM32duino) + `analogWriteResolution(10)` + `analogWrite` — tương đương duty 0–100 % của HAL.

## Giao thức host

Giữ nguyên định dạng dòng `S,...`, `A,...`, `E,...` và lệnh `T,<mmHg>` như tài liệu `pc-bridge.md` ở bản HAL.
