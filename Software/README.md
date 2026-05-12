# Software — tài liệu host & WebApp

Thư mục này chứa tài liệu giao thức UART dùng chung với firmware và ứng dụng web đọc **USB TTL** (Web Serial).

## WebApp (Vite + TypeScript)

**Trình duyệt:** Chrome hoặc Edge (Web Serial). Phục vụ qua `http://localhost` hoặc HTTPS.

```bash
cd webapp
npm install
npm run dev
```

- Giao thức `S/A/E`, `T` và **MAA** trên host: [`pc-bridge.md`](pc-bridge.md), [`algorithm.md`](algorithm.md).
- Nút **Hủy** gửi `ABORT`; ô **SAF test** gửi `T,...` (clamp theo SAF đã cấu hình trên MCU / form, mặc định 175 mmHg). Sau kết nối, WebApp gửi `SAF` / `SAFH` / `HIGH` theo form **MCU — UART**.
- **Firefox/Safari** không có Web Serial: có thể dùng bridge TCP như gợi ý trong [`pc-bridge.md`](pc-bridge.md) (`socat`/nhỏ script đọc COM và WebSocket) rồi mở rộng webapp sau nếu cần.

## Firmware (tham chiếu)

Có hai biến thể cho **STM32F103C8 Blue Pill**, cùng chân và giao thức serial với WebApp:

| Thư mục | Công cụ | Ghi chú |
|--------|---------|--------|
| [`../firmware/stm32-hal-cmake/`](../firmware/stm32-hal-cmake/) | CMake + STM32 HAL | Bản gốc trong repo; build theo [`../firmware/stm32-hal-cmake/README.md`](../firmware/stm32-hal-cmake/README.md). |
| [`../firmware/stm32-arduino-pio/`](../firmware/stm32-arduino-pio/) | PlatformIO + **Arduino** framework | `Wire` / `Serial` / `HardwareTimer`; xem [`../firmware/stm32-arduino-pio/README.md`](../firmware/stm32-arduino-pio/README.md). |

### Build & nạp (HAL — CMake)

```bash
cd ../firmware/stm32-hal-cmake
cmake --preset Debug
cmake --build build/Debug
```

### Build & nạp (Arduino — PlatformIO)

```bash
cd ../firmware/stm32-arduino-pio
pio run
pio run -t upload
pio device monitor -b 115200
```

Cần [PlatformIO Core](https://platformio.org/install) và ST-Link (hoặc đổi `upload_protocol` trong [`../firmware/stm32-arduino-pio/platformio.ini`](../firmware/stm32-arduino-pio/platformio.ini)).

### Cấu hình phần cứng

Chân mặc định xem [`../firmware/stm32-hal-cmake/Core/Inc/board_config.h`](../firmware/stm32-hal-cmake/Core/Inc/board_config.h) (HAL) hoặc `board_config.h` trong project Arduino (cùng map chân):

- UART1 **115200**: PA9 TX, PA10 RX  
- I2C1: PB6 SCL, PB7 SDA (ADS1115 `0x48`)  
- PWM TIM3: PA6 bơm, PA7 van  
- Nút (active LOW): PA3 Start, PA4 Stop, PA5 High-mode  
- LED schematic D401/D402/D403: **PB13 đỏ**, **PB14 xanh**, **PB15 vàng**

Hiệu chỉnh `PRESSURE_ADC_OFFSET_COUNTS` và `PRESSURE_ADC_SCALE_MMHG_PER_COUNT` theo MPX5050 + đồng hồ áp.

## Giao tiếp WebApp / serial

Chi tiết khung tin, band-pass 0.5–5 Hz trên host, và luồng `P_sys_est` → lệnh `T,...`: [**pc-bridge.md**](pc-bridge.md).

## Kiểm thử

Checklist mở rộng: [**TESTING.md**](TESTING.md), tham chiếu [`testcase.md`](testcase.md).

## LED (HMI)

Logic không chặn theo [`LED_Algorithm.md`](LED_Algorithm.md); phần C trong firmware HAL: [`../firmware/stm32-hal-cmake/Core/Inc/led_hmi.h`](../firmware/stm32-hal-cmake/Core/Inc/led_hmi.h), [`../firmware/stm32-hal-cmake/Core/Src/led_hmi.c`](../firmware/stm32-hal-cmake/Core/Src/led_hmi.c), ánh xạ trạng thái trong [`../firmware/stm32-hal-cmake/Core/Src/bp_fsm.c`](../firmware/stm32-hal-cmake/Core/Src/bp_fsm.c) (`bp_fsm_led_hmi_state`). Bản Arduino dùng cùng thuật toán LED trong `src/led_hmi.cpp`.
