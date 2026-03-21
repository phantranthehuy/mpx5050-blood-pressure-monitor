# Hệ Thống Đo Huyết Áp - Peak Detection Algorithm
## Blood Pressure Measurement System - Peak Detection Algorithm

**Dự án**: MPX5050 Blood Pressure Monitor  
**Phiên bản**: 1.0  
**Ngày tạo**: 2026-03-21  
**Tác giả**: Blood Pressure Monitor Team

---

## 📋 Mục Lục (Table of Contents)

1. [Giới thiệu](#giới-thiệu)
2. [Cấu trúc Dự Án](#cấu-trúc-dự-án)
3. [Yêu Cầu Hệ Thống](#yêu-cầu-hệ-thống)
4. [Cài Đặt & Xây Dựng](#cài-đặt--xây-dựng)
5. [Hướng Dẫn Sử Dụng](#hướng-dẫn-sử-dụng)
6. [Cấu Trúc Thuật Toán](#cấu-trúc-thuật-toán)
7. [API Reference](#api-reference)
8. [Ví Dụ](#ví-dụ)
9. [Testing](#testing)
10. [Tối Ưu Hóa](#tối-ưu-hóa)
11. [Xử Lý Sự Cố](#xử-lý-sự-cố)

---

## Giới Thiệu

Dự án này cung cấp một **thuật toán dò đỉnh (Peak Detection Algorithm)** chuyên nghiệp cho hệ thống đo huyết áp
sử dụng cảm biến **MPX5050** (áp suất tuyệt đối) trên microcontroller **STM32F103**.

### Các Tính Năng Chính:
- ✅ **Lọc tín hiệu**: Loại bỏ nhiễu bằng bộ lọc thông thấp
- ✅ **Phát hiện đỉnh**: Sử dụng phương pháp zero-crossing của đạo hàm
- ✅ **Tính toán BP**: Xác định huyết áp tâm thu, tâm trương và áp suất động mạch trung bình
- ✅ **Đo nhịp tim**: Tính toán nhịp tim từ khoảng cách đỉnh
- ✅ **Xác thực dữ liệu**: Kiểm tra giá trị nằm trong phạm vi hợp lệ
- ✅ **Cấu hình linh hoạt**: Điều chỉnh hệ số bộ lọc, ngưỡng phát hiện

---

## 📁 Cấu Trúc Dự Án

```
test001/
├── CMakeLists.txt                 # CMake configuration
├── CMakePresets.json              # CMake presets
├── stm32f103x8_flash.ld           # Linker script
├── ALGORITHM_DOCUMENTATION.md     # Tài liệu thuật toán (tiếng Việt + tiếng Anh)
├── README.md                      # File này
│
├── Inc/                           # Header files
│   ├── peak_detection.h          # Peak detection algorithm headers
│   ├── sensor_interface.h        # Sensor interface and conversion
│   └── test_peak_detection.h     # Test function headers
│
├── Src/                           # Source files
│   ├── main.c                    # Main program with example usage
│   ├── peak_detection.c          # Peak detection implementation
│   ├── sensor_interface.c        # Sensor interface implementation
│   ├── test_peak_detection.c     # Test implementation
│   ├── startup_stm32f103xx.S     # Startup code
│   ├── syscall.c                 # System calls
│   └── sysmem.c                  # Memory management
│
├── cmake/                         # CMake support files
│   ├── gnu-tools-for-stm32.cmake
│   └── vscode_generated.cmake

└── build/                         # Build output (generated)
```

---

## ⚙️ Yêu Cầu Hệ Thống

### Hardware
- **MCU**: STM32F103 (hoặc tương thích)
- **Cảm Biến**: MPX5050 (hoặc cảm biến áp suất tương tự)
- **ADC**: 12-bit, tối thiểu 100Hz sampling rate

### Software
- **IDE**: Visual Studio Code + STM32 extension hoặc STM32CubeIDE
- **Compiler**: ARM GCC (arm-none-eabi-gcc)
- **CMake**: Ver. 3.20+
- **Thư viện**: Không cần thư viện bên ngoài (self-contained)

### Công Cụ Phát Triển
```bash
# Windows
- CMake 3.22+
- ARM GCC Toolchain
- STM32CubeMX (tùy chọn)

# Hoặc dùng VS Code Extensions:
- Cortex-Debug
- CMake Tools
- STM32 extension
```

---

## 🔧 Cài Đặt & Xây Dựng

### 1. Clone hoặc Copy Dự Án

```bash
cd c:\Users\Huy Hoang\mpx5050-blood-pressure-monitor\Algorithm\test001
```

### 2. Tạo Build Directory

```bash
mkdir build
cd build
```

### 3. Cấu Hình CMake

```bash
# Debug configuration
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..

# Hoặc Release
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
```

### 4. Xây Dựng Dự Án

```bash
cmake --build . --config Debug
# hoặc
make
```

### 5. Flashing sang MCU

```bash
# Dùng STM32 ST-Link
st-flash write test001.bin 0x08000000
```

---

## 📖 Hướng Dẫn Sử Dụng

### Bước 1: Khởi Tạo Hệ Thống

```c
#include "peak_detection.h"
#include "sensor_interface.h"

peak_detector_t detector;

// Khởi tạo với buffer 256 samples, tối đa 20 peaks
if (!peak_detector_init(&detector, 256, 20)) {
    printf("Initialization failed\n");
    return -1;
}
```

### Bước 2: Đặt Lại cho Lần Đo Mới

```c
// Đọc giá trị áp suất ban đầu từ cảm biến
uint16_t adc_value = read_adc();
float initial_pressure = sensor_adc_to_pressure_mmhg(adc_value);

// Đặt lại detector
peak_detector_reset(&detector, initial_pressure);
```

### Bước 3: Xử Lý Mẫu

```c
uint32_t time_ms = 0;
const uint16_t SAMPLING_PERIOD_MS = 10;  // 100Hz

while (measuring) {
    // Đọc từ ADC
    uint16_t adc_value = read_adc();
    float pressure = sensor_adc_to_pressure_mmhg(adc_value);
    
    // Xử lý mẫu
    peak_data_t *data = peak_detector_process_sample(
        &detector,
        pressure,
        time_ms
    );
    
    if (data->is_peak) {
        printf("Peak detected: %.2f mmHg\n", data->peak_magnitude);
    }
    
    time_ms += SAMPLING_PERIOD_MS;
}
```

### Bước 4: Tính Toán Kết Quả

```c
bp_measurement_t bp_result;

if (peak_detector_calculate_bp(&detector, &bp_result)) {
    if (bp_result.measurement_valid) {
        printf("Systolic:  %d mmHg\n", bp_result.systolic);
        printf("Diastolic: %d mmHg\n", bp_result.diastolic);
        printf("MAP:       %d mmHg\n", bp_result.mean_arterial);
        printf("Pulse:     %d bpm\n", bp_result.pulse_rate);
    }
}
```

### Bước 5: Dọn Dẹp

```c
peak_detector_deinit(&detector);
```

---

## 🏗️ Cấu Trúc Thuật Toán

### Quy Trình Xử Lý

```
┌─────────────────────────────────────────┐
│  Sensor (MPX5050)                       │
│  Raw Pressure Value                     │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  ADC Conversion (12-bit)                │
│  ADC Value → Pressure (mmHg)            │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  Low-Pass Filter (α=0.2)                │
│  Eliminate high-frequency noise         │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  Derivative Calculation                 │
│  Rate of change analysis                │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  Peak Detection (Zero-Crossing)         │
│  Find local maxima                      │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  Peak Storage & Analysis                │
│  Store peaks with timings               │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  Blood Pressure Calculation             │
│  SBP, DBP, MAP, Pulse Rate              │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  Validation                             │
│  Check value ranges                     │
└────────────────┬────────────────────────┘
                 ▼
┌─────────────────────────────────────────┐
│  Result Output                          │
│  Display/Store measurements             │
└─────────────────────────────────────────┘
```

---

## 📚 API Reference

### Khởi Tạo & Quản Lý

| Hàm | Mô Tả |
|-----|-------|
| `peak_detector_init()` | Khởi tạo detector, cấp phát bộ nhớ |
| `peak_detector_deinit()` | Giải phóng bộ nhớ |
| `peak_detector_reset()` | Đặt lại cho lần đo mới |

### Xử Lý Dữ Liệu

| Hàm | Mô Tả |
|-----|-------|
| `peak_detector_process_sample()` | Xử lý một mẫu áp suất |
| `peak_detector_low_pass_filter()` | Lọc tín hiệu |
| `peak_detector_calculate_derivative()` | Tính đạo hàm |
| `peak_detector_detect_peak()` | Phát hiện đỉnh |

### Lấy Dữ Liệu

| Hàm | Mô Tả |
|-----|-------|
| `peak_detector_get_peaks_count()` | Lấy số đỉnh phát hiện |
| `peak_detector_get_peak()` | Lấy dữ liệu đỉnh cụ thể |
| `peak_detector_calculate_bp()` | Tính toán huyết áp |
| `peak_detector_get_statistics()` | Lấy thống kê tín hiệu |

### Cấu Hình

| Hàm | Mô Tả |
|-----|-------|
| `peak_detector_set_threshold()` | Đặt ngưỡng phát hiện |
| `peak_detector_set_filter_coefficient()` | Đặt hệ số bộ lọc |

---

## 📝 Ví Dụ

### Ví Dụ 1: Đo Huyết Áp Đơn Giản

```c
#include "peak_detection.h"

int main(void) {
    peak_detector_t detector;
    bp_measurement_t result;
    
    // Khởi tạo
    peak_detector_init(&detector, 256, 20);
    peak_detector_reset(&detector, 80.0f);
    
    // Giả sử có dữ liệu từ cảm biến
    float pressure_samples[] = {80, 85, 90, 95, 100, 95, 90, 85, 80};
    
    for (int i = 0; i < 9; i++) {
        peak_detector_process_sample(&detector, pressure_samples[i], i*10);
    }
    
    // Tính toán
    if (peak_detector_calculate_bp(&detector, &result)) {
        if (result.measurement_valid) {
            printf("SBP: %d, DBP: %d\n", result.systolic, result.diastolic);
        }
    }
    
    peak_detector_deinit(&detector);
    return 0;
}
```

### Ví Dụ 2: Điều Chỉnh Thông Số Bộ Lọc

```c
// Nếu tín hiệu có nhiều nhiễu
peak_detector_set_filter_coefficient(&detector, 0.1);  // α nhỏ = lọc nhiều hơn

// Nếu cần phản ứng nhanh hơn
peak_detector_set_filter_coefficient(&detector, 0.4);  // α lớn = phản ứng nhanh
```

### Ví Dụ 3: Lấy Thống Kê

```c
float min_p, max_p, mean_p;
peak_detector_get_statistics(&detector, &min_p, &max_p, &mean_p);
printf("Min: %.2f, Max: %.2f, Mean: %.2f\n", min_p, max_p, mean_p);
```

---

## 🧪 Testing

### Chạy Test Suite

```bash
# Compile với test functions
# (test_peak_detection.c được compile)

# Gọi từ main hoặc debug
if (run_all_tests()) {
    printf("✓ All tests passed\n");
} else {
    printf("✗ Some tests failed\n");
}
```

### Các Test Hiện Có

1. **Test Realistic Measurement**: Dữ liệu huyết áp thực tế bình thường
2. **Test Noisy Data**: Dữ liệu có nhiễu nhân tạo
3. **Test Edge Cases**: Các trường hợp biên
4. **Test Filter Response**: Kiểm tra ngoại lệ bộ lọc

---

## ⚡ Tối Ưu Hóa

### 1. Hiệu Suất Bộ Nhớ

```c
// Giảm kích thước buffer nếu cần tiết kiệm RAM
peak_detector_init(&detector, 128, 15);  // Nhỏ hơn

// Hoặc tăng nếu cần lưu trữ dài hạn
peak_detector_init(&detector, 512, 50);  // Lớn hơn
```

### 2. Độ Chính Xác

```c
// Tần suất lấy mẫu cao hơn = độ chính xác tốt hơn
// Khuyến nghị: 100Hz (10ms) trở lên

// Điều chỉnh hệ số bộ lọc
// - Ít nhiễu: α = 0.3 - 0.4
// - Nhiễu: α = 0.1 - 0.2 (lọc nhiều)
```

### 3. Tốc Độ Tính Toán

```c
// Sử dụng fixed-point nếu cần (thay vì floating-point)
// Hạn chế số phép tính logarit/trig
// Pre-calculate hằng số
```

---

## 🐛 Xử Lý Sự Cố

### Vấn Đề: Phép Đo Không Hợp Lệ

**Nguyên nhân**: Không đủ đỉnh được phát hiện

```c
// Kiểm tra số đỉnh
printf("Peaks: %d\n", peak_detector_get_peaks_count(&detector));

// Giải pháp:
// 1. Tăng thời gian đo
// 2. Giảm hệ số bộ lọc α
// 3. Kiểm tra cảm biến
```

### Vấn Đề: Giá Trị Sai Lệch

**Nguyên nhân**: Nhiễu trong tín hiệu

```c
// Giảm hệ số bộ lọc
peak_detector_set_filter_coefficient(&detector, 0.1);

// Hoặc tăng ngưỡng phát hiện
peak_detector_set_threshold(&detector, 0.7);
```

### Vấn Đề: Không Phát Hiện Đỉnh

**Nguyên nhân**: Tín hiệu quá mịn hoặc cảm biến không hoạt động

```c
// 1. Kiểm tra cảm biếu
// 2. Kiểm tra ADC
// 3. Tăng hệ số lọc
peak_detector_set_filter_coefficient(&detector, 0.4);  // Nhạy cảm hơn
```

---

## 📊 Phạm Vi Giá Trị

| Thông Số | Tối Thiểu | Bình Thường | Tối Đa |
|----------|-----------|------------|--------|
| Systolic (SBP) | 60 mmHg | 120 mmHg | 250 mmHg |
| Diastolic (DBP) | 30 mmHg | 80 mmHg | 200 mmHg |
| Pulse Rate | 30 bpm | 70 bpm | 180 bpm |
| Đỉnh tối thiểu | - | 3 | - |
| Thời gian đo | 5s | 20s | 30s |

---

## 📞 Liên Hệ & Hỗ Trợ

Nếu có câu hỏi hoặc vấn đề:

1. Kiểm tra **ALGORITHM_DOCUMENTATION.md** để hiểu rõ thuật toán
2. Xem **ví dụ** trong **main.c**
3. Chạy **test suite** để xác thực
4. Kiểm tra lại **cấu hình cảm biến**

---

## 📄 Giấy Phép

Copyright (c) 2025 - All Rights Reserved

---

## ✅ Checklist Triển Khai

- [ ] Cấu hình ADC trên STM32F103
- [ ] Kết nối MPX5050 sensor
- [ ] Xây dựng dự án
- [ ] Chạy test suite
- [ ] Flash firmware lên MCU
- [ ] Kiểm tra đầu ra trên console
- [ ] Xác thực với máy đo chính thức
- [ ] Điều chỉnh thông số nếu cần
- [ ] Lưu kết quả đo
- [ ] Triển khai trên sản phẩm

---

**Cập nhật lần cuối**: 2026-03-21  
**Phiên bản**: 1.0
