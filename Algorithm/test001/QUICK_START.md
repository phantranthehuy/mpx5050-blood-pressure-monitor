/**
 ******************************************************************************
 * @file           : QUICK_START.md
 * @brief          : Hướng Dẫn Bắt Đầu Nhanh - Quick Start Guide
 * @author         : Blood Pressure Monitor Team
 * @date           : 2026-03-21
 ******************************************************************************
 */

# 🚀 Hướng Dẫn Bắt Đầu Nhanh (Quick Start Guide)
## Peak Detection Algorithm for Blood Pressure Measurement

Tài liệu này giúp bạn bắt đầu nhanh với thuật toán dò đỉnh để đo huyết áp trong **5 phút**.

---

## ✋ 5 Bước Cơ Bản

### Bước 1: Khởi Tạo Detector (Initialization)

```c
#include "peak_detection.h"

peak_detector_t detector;

// Khởi tạo: buffer 256 mẫu, tối đa 20 đỉnh
peak_detector_init(&detector, 256, 20);
```

### Bước 2: Đặt Lại Cho Lần Đo Mới (Reset)

```c
float initial_pressure = 80.0;  // mmHg (từ cảm biến)
peak_detector_reset(&detector, initial_pressure);
```

### Bước 3: Xử Lý Mẫu (Process Data)

```c
// Trong vòng lặp đo (20-30 giây)
for (int i = 0; i < NUM_SAMPLES; i++) {
    uint16_t adc_value = read_adc();                    // Đọc ADC
    float pressure = sensor_adc_to_pressure_mmhg(adc_value);  // Chuyển đổi
    
    peak_detector_process_sample(&detector, pressure, time_ms);
    
    time_ms += 10;  // Tăng thời gian 10ms
}
```

### Bước 4: Tính Toán Kết Quả (Calculate Results)

```c
bp_measurement_t result;

if (peak_detector_calculate_bp(&detector, &result)) {
    printf("Systolic:  %d mmHg\n", result.systolic);
    printf("Diastolic: %d mmHg\n", result.diastolic);
}
```

### Bước 5: Dọn Dẹp (Cleanup)

```c
peak_detector_deinit(&detector);
```

---

## 📋 Đầy Đủ Hơn (Complete Example)

```c
#include "peak_detection.h"
#include "sensor_interface.h"
#include <stdio.h>

int main(void) {
    peak_detector_t detector;
    bp_measurement_t result;
    uint32_t time_ms = 0;
    
    // Bước 1: Khởi tạo
    printf("Initializing detector...\n");
    if (!peak_detector_init(&detector, 256, 20)) {
        printf("ERROR: Init failed!\n");
        return -1;
    }
    
    // Bước 2: Đặt lại - đợi người dùng yên ổn
    printf("Place arm and wait...\n");
    uint16_t adc_init = read_adc();
    float pressure_init = sensor_adc_to_pressure_mmhg(adc_init);
    peak_detector_reset(&detector, pressure_init);
    
    // Bước 3: Đo trong 30 giây (3000 mẫu x 10ms)
    printf("Measuring...\n");
    for (int i = 0; i < 3000; i++) {
        uint16_t adc_raw = read_adc();
        float pressure = sensor_adc_to_pressure_mmhg(adc_raw);
        
        peak_detector_process_sample(&detector, pressure, time_ms);
        time_ms += 10;
        
        // Debug: in ra mỗi 1 giây
        if (i % 100 == 0) {
            printf("  Sample %d, Pressure: %.1f mmHg\n", i, pressure);
        }
    }
    
    printf("Calculating...\n");
    
    // Bước 4: Tính toán
    if (peak_detector_calculate_bp(&detector, &result)) {
        if (result.measurement_valid) {
            printf("\n====== RESULT ======\n");
            printf("Systolic:      %d mmHg\n", result.systolic);
            printf("Diastolic:     %d mmHg\n", result.diastolic);
            printf("MAP:           %d mmHg\n", result.mean_arterial);
            printf("Heart Rate:    %d bpm\n", result.pulse_rate);
            printf("Peaks Found:   %d\n", peak_detector_get_peaks_count(&detector));
            printf("====================\n");
        } else {
            printf("ERROR: Measurement invalid! Try again.\n");
        }
    }
    
    // Bước 5: Dọn dẹp
    peak_detector_deinit(&detector);
    return 0;
}
```

---

## 🔌 ADC Configuration (STM32F103)

Để sử dụng cảm biến MPX5050:

```c
// PA0 - ADC Channel 0 (MPX5050)
// Ref: 3.3V
// Resolution: 12-bit

uint16_t read_adc(void) {
    // TODO: Implement actual ADC reading
    // Return 0-4095 (12-bit value)
    return ADC1->DR;  // Ví dụ (your actual code)
}
```

### Chuyển Đổi ADC → Áp suất (mmHg)

```c
float sensor_adc_to_pressure_mmhg(uint16_t adc_value) {
    // 1. ADC → Điện áp (3.3V Vref, 12-bit)
    float voltage = (adc_value / 4095.0f) * 3.3f;
    
    // 2. Điện áp → Áp suất (kPa) [MPX5050]
    float pressure_kpa = (voltage - 0.2f) / (4.7f - 0.2f) * 350.0f;
    
    // 3. kPa → mmHg
    float pressure_mmhg = pressure_kpa * 7.50061f;
    
    return pressure_mmhg;
}
```

---

## ⚙️ Điều Chỉnh Thông Số

### Nếu Không Phát Hiện Được Đỉnh

```c
// Giảm hệ số bộ lọc (lọc ít hơn)
peak_detector_set_filter_coefficient(&detector, 0.3);
```

### Nếu Phát Hiện Nhiều Đỉnh Giả

```c
// Tăng hệ số bộ lọc (lọc nhiều hơn)
peak_detector_set_filter_coefficient(&detector, 0.1);
```

### Giải Pháp Chung Cho Lỗi

| Vấn Đề | Giải Pháp |
|--------|----------|
| Không tìm được đỉnh | Tăng thời gian đo, giảm α |
| Giá trị sai lệch | Kiểm tra ADC, tăng số mẫu |
| Quá nhiều điểm đỉnh | Tăng α (tăng lọc) |
| Quá ít điểm đỉnh | Giảm α (giảm lọc) |

---

## 📊 Thông Tin Output

### Blood Pressure Measurement Structure

```c
typedef struct {
    uint16_t systolic;         // Huyết áp tâm thu (mmHg)
    uint16_t diastolic;        // Huyết áp tâm trương (mmHg)
    uint16_t mean_arterial;    // Áp suất động mạch trung bình (mmHg)
    uint16_t pulse_rate;       // Nhịp tim (bpm)
    bool measurement_valid;    // Phép đo hợp lệ?
} bp_measurement_t;
```

### Ví Dụ Output

```
Systolic:      120 mmHg     (Huyết áp tâm thu - cao nhất)
Diastolic:      80 mmHg     (Huyết áp tâm trương - thấp nhất)
MAP:            93 mmHg     (Áp suất động mạch trung bình)
Heart Rate:     72 bpm      (Nhịp tim)
Peaks Found:     7           (Số đỉnh phát hiện)
```

---

## 📂 Các File Quan Trọng

| File | Mô Tả |
|------|-------|
| `peak_detection.h` | Khai báo hàm chính |
| `peak_detection.c` | Hiện thực thuật toán |
| `sensor_interface.h/c` | Chuyển đổi ADC → Áp suất |
| `main.c` | Ví dụ chính |
| `ALGORITHM_DOCUMENTATION.md` | Tài liệu chi tiết |

---

## 🧪 Kiểm Tra (Verify)

### 1. Xây Dựng (Build)

```bash
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### 2. Chạy Test

```c
// Thêm vào main.c
if (run_all_tests()) {
    printf("✓ All tests passed\n");
}
```

### 3. Xác Thực Kết Quả

Kiểm tra xem kết quả có hợp lý không:
- **SBP > DBP** ✓
- **60 ≤ SBP ≤ 250** ✓
- **30 ≤ DBP ≤ 200** ✓
- **Tối thiểu 3 đỉnh phát hiện** ✓

---

## 🎯 Tóm Tắt Hàm Chính

```c
// Khởi tạo/Dọn dẹp
peak_detector_init()      // Khởi tạo
peak_detector_deinit()    // Giải phóng
peak_detector_reset()     // Đặt lại

// Xử lý
peak_detector_process_sample()  // Xử lý một mẫu

// Kết quả
peak_detector_calculate_bp()         // Tính huyết áp
peak_detector_get_peaks_count()      // Lấy số đỉnh
peak_detector_get_statistics()       // Thống kê

// Cấu hình
peak_detector_set_filter_coefficient()  // Điều chỉnh bộ lọc
peak_detector_set_threshold()           // Điều chỉnh ngưỡng
```

---

## 💡 Mẹo Hữu Ích

1. **Tần suất lấy mẫu**: 100Hz (10ms) là lý tưởng
2. **Thời gian đo**: 20-30 giây cho kết quả tốt nhất
3. **Cảm biến**: Giữ cạnh tay ở mức trái tim
4. **Bộ lọc**: Bắt đầu với α=0.2, điều chỉnh nếu cần
5. **Debug**: Dùng `printf()` để theo dõi giá trị real-time

---

## ❓ FAQ

**Q: Phép đo luôn không hợp lệ?**  
A: Kiểm tra cảm biến, ADC, và đảm bảo có đủ đỉnh (≥3)

**Q: Giá trị không chính xác?**  
A: Kiểm tra hệ số chuyển đổi ADC, tăng thời gian đo

**Q: Làm thế nào để chạy test?**  
A: Gọi `run_all_tests()` từ `main()`

**Q: Cần điều chỉnh gì ngoài α?**  
A: Kiểm tra `MIN_PEAK_DISTANCE_MS`, thời gian đo

---

## 📞 Hỗ Trợ

- 📖 Xem `ALGORITHM_DOCUMENTATION.md` để hiểu sâu hơn
- 💻 Kiểm tra `main.c` để xem ví dụ
- 🧪 Chạy `test_peak_detection.c` để xác thực
- 📊 Xem README.md để biết thêm chi tiết

---

**Hãy bắt đầu ngay!** ✨

Tài liệu này cung cấp đủ thông tin để bạn bắt đầu trong 5 phút.
Đối với chi tiết kỹ thuật, xem `ALGORITHM_DOCUMENTATION.md`.

---

**Phiên bản**: 1.0  
**Cập nhật**: 2026-03-21
