# Firmware — máy đo huyết áp oscillometric (STM32F103 Blue Pill)

Project **STM32Cube / CMake** (HAL): đọc áp **ADS1115** (I2C), điều khiển **bơm/van PWM**, stream **100 Hz** qua UART cho WebApp/PC, SAF **280 mmHg** và **Emergency Stop** cục bộ.

## Build & nạp

Cần **CMake ≥ 3.22**, **GNU Arm Embedded Toolchain** (`arm-none-eabi-gcc`), và **Git** (lần cấu hình đầu CMake sẽ clone HAL/CMSIS vào `third_party/`).

```bash
cd Firmware
cmake --preset Debug
cmake --build build/Debug
```

File nhị phân: `build/Debug/bp_monitor.bin`, ELF `build/Debug/bp_monitor`.

**STM32CubeIDE for VS Code:** mở folder `Firmware`, chọn preset **Debug** hoặc **Release**, chạy CMake configure/build trong extension theo [First project creation](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/1.0.1/en/docs/markup/getting_started/first_project_creation.html).

**Nạp chip:** dùng `stm32flash`, OpenOCD, hoặc STM32CubeProgrammer tới địa chỉ flash `0x08000000`, ví dụ:

```bash
stm32flash -w build/Debug/bp_monitor.bin -v -g 0x0 /dev/ttyUSB0   # tuỳ adapter UART bootloader
```

**UART serial:** `115200 8N1` trên USART1 (PA9/PA10).

### STM32F103C8T6 (Blue Pill)

Không cần đổi macro biên dịch: ST dùng **`STM32F103xB`** trong CMSIS cho cả **C8** và **CB** (file chip là `stm32f103xb.h`). Điều quan trọng là **kích thước Flash trong linker**:

| MCU | Flash | Việc cần làm |
|-----|-------|----------------|
| **STM32F103C8T6** | 64 KiB | Giữ nguyên `cmake/STM32F103C8Tx_FLASH.ld` (đã cấu hình sẵn). |
| **STM32F103CB…** | 128 KiB | Đổi `LENGTH` Flash trong linker script thành `128K` (hoặc dùng script ST cho RB/CB 128K). |

## Cấu hình phần cứng

Chân mặc định xem [`Core/Inc/board_config.h`](Core/Inc/board_config.h):

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

Logic không chặn theo [`LED_Algorithm.md`](LED_Algorithm.md); code: [`Core/Inc/led_hmi.h`](Core/Inc/led_hmi.h), [`Core/Src/led_hmi.c`](Core/Src/led_hmi.c), ánh xạ trạng thái trong [`Core/Src/bp_fsm.c`](Core/Src/bp_fsm.c) (`bp_fsm_led_hmi_state`).
