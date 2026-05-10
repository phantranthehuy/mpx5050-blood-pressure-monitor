# Firmware — máy đo huyết áp oscillometric (STM32F103 Blue Pill)

Project **PlatformIO** (`ststm32`, board `bluepill_f103c8`, framework `stm32cube`): đọc áp **ADS1115** (I2C), điều khiển **bơm/van PWM**, stream **100 Hz** qua UART cho WebApp/PC, SAF **280 mmHg** và **Emergency Stop** cục bộ.

## Build & nạp

```bash
cd Firmware
pio run -t upload
pio device monitor -b 115200
```

Cần [PlatformIO Core](https://platformio.org/install) và ST-Link (hoặc đổi `upload_protocol` trong [`platformio.ini`](platformio.ini)).

## Cấu hình phần cứng

Chân mặc định xem [`include/board_config.h`](include/board_config.h):

- UART1 **115200**: PA9 TX, PA10 RX  
- I2C1: PB6 SCL, PB7 SDA (ADS1115 `0x48`)  
- PWM TIM3: PA6 bơm, PA7 van  
- Nút (active LOW): PA3 Start, PA4 Stop, PA5 High-mode  
- LED schematic D401/D402/D403: **PB13 đỏ**, **PB14 xanh**, **PB15 vàng**

Hiệu chỉnh `PRESSURE_ADC_OFFSET_COUNTS` và `PRESSURE_ADC_SCALE_MMHG_PER_COUNT` theo MPX5050 + đồng hồ áp.

## Giao tiếp WebApp / serial

Chi tiết khung tin, band-pass 0.5–5 Hz trên host, và luồng `P_sys_est` → lệnh `T,...`: [**docs/pc-bridge.md**](docs/pc-bridge.md).

## Kiểm thử

Checklist mở rộng: [**TESTING.md**](TESTING.md), tham chiếu [`testcase.md`](testcase.md).

## LED (HMI)

Logic không chặn theo [`LED_Algorithm.md`](LED_Algorithm.md); code: [`include/led_hmi.h`](include/led_hmi.h), [`src/led_hmi.c`](src/led_hmi.c), ánh xạ trạng thái trong [`src/bp_fsm.c`](src/bp_fsm.c) (`bp_fsm_led_hmi_state`).
