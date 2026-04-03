/**
 ******************************************************************************
 * @file           : ALGORITHM_DOCUMENTATION.md
 * @brief          : Peak Detection Algorithm for Blood Pressure Measurement
 * @author         : Blood Pressure Monitor Team
 * @date           : 2026-03-21
 ******************************************************************************
 */

# Thuật Toán Dò Đỉnh Trong Đo Huyết Áp
## Peak Detection Algorithm for Blood Pressure Measurement

---

## 1. Giới Thiệu (Introduction)

Thuật toán dò đỉnh (Peak Detection Algorithm) là một phương pháp quan trọng trong việc đo huyết áp bằng
phương pháp dao động (Oscillometric Method). Thuật toán này phân tích tín hiệu áp suất để:

1. **Phát hiện các đỉnh** (peaks) trong tín hiệu áp suất
2. **Tính toán huyết áp tâm thu** (Systolic) - áp suất cực đại
3. **Tính toán huyết áp tâm trương** (Diastolic) - áp suất cực tiểu
4. **Ước tính nhịp tim** (Pulse Rate)

---

## 2. Nguyên Lý Hoạt Động (Operating Principle)

### 2.1 Phương Pháp Oscillometric
Khi bơm không khí vào bộ phận quấn của máy đo huyết áp, cánh tay trải qua ba giai đoạn:

- **Giai đoạn 1**: Áp suất > Huyết áp tâm thu (SBP) → Không có dao động
- **Giai đoạn 2**: Áp suất giữa Diastolic và Systolic → Dao động cực đại
- **Giai đoạn 3**: Áp suất < Huyết áp tâm trương (DBP) → Không có dao động

### 2.2 Quá Trình Xử Lý Tín Hiệu

```
Tín hiệu thô từ cảm biến
        ↓
    [Lọc Tín Hiệu] - Low-Pass Filter
        ↓
    [Tính Đạo Hàm] - Calculate Derivative
        ↓
    [Phát Hiện Đỉnh] - Peak Detection (Zero-Crossing)
        ↓
    [Phân Tích Đỉnh] - Peak Analysis
        ↓
    [Tính Toán BP] - Calculate Blood Pressure
```

---

## 3. Các Thành Phần Chính (Key Components)

### 3.1 Lọc Tín Hiệu (Low-Pass Filter)
```
formula: filtered[n] = α * raw[n] + (1-α) * filtered[n-1]
- α = 0.2 (filter coefficient)
- Loại bỏ nhiễu tần số cao
- Làm mịn tín hiệu áp suất
```

**Tác dụng:**
- Giảm nhiễu từ độ rung, cở động
- Làm mịn tín hiệu để phát hiện đỉnh chính xác

### 3.2 Phát Hiện Đỉnh (Peak Detection)
Sử dụng phương pháp **Zero-Crossing** của đạo hàm:

```
Peak Detection Logic:
if (derivative[n-1] > 0 AND derivative[n] < -threshold)
    → Peak detected at point n
```

**Quy trình:**
1. Tính đạo hàm (rate of change) của tín hiệu lọc
2. Phát hiện khi đạo hàm đổi từ dương thành âm (điểm cực đại)
3. Xác nhận đỉnh nếu cách đỉnh trước ít nhất 300ms

### 3.3 Tính Toán Huyết Áp (Blood Pressure Calculation)

```
Systolic (SBP)  = Baseline + MAX(peak magnitudes)
Diastolic (DBP) = Baseline + MIN(peak magnitudes)
MAP             = Baseline + MEAN(peak magnitudes)
Pulse Rate      = (Số đỉnh - 1) * 60 / Thời gian (bpm)
```

---

## 4. Cấu Trúc Dữ Liệu (Data Structures)

### 4.1 Peak Data Structure
```c
typedef struct {
    float pressure_value;      // Giá trị áp suất tại đỉnh
    float derivative;          // Đạo hàm (tốc độ thay đổi)
    uint32_t timestamp;        // Thời gian (ms)
    bool is_peak;              // Là đỉnh hay không
    float peak_magnitude;      // Độ lớn của đỉnh
} peak_data_t;
```

### 4.2 Blood Pressure Measurement Structure
```c
typedef struct {
    uint16_t systolic;         // Huyết áp tâm thu (mmHg)
    uint16_t diastolic;        // Huyết áp tâm trương (mmHg)
    uint16_t mean_arterial;    // Áp suất động mạch trung bình (mmHg)
    uint16_t pulse_rate;       // Nhịp tim (bpm)
    bool measurement_valid;    // Phép đo có hiệu lực
} bp_measurement_t;
```

### 4.3 Peak Detector State Machine
```c
typedef struct {
    float *pressure_buffer;    // Bộ nhớ đệm áp suất
    float *filtered_buffer;    // Tín hiệu lọc
    peak_data_t *peaks_buffer; // Bộ nhớ đệm đỉnh phát hiện
    uint16_t peaks_count;      // Số lượng đỉnh phát hiện
    float baseline_pressure;   // Áp suất cơ sở (mmHg)
    float alpha;               // Hệ số bộ lọc (0.0 - 1.0)
    ...
} peak_detector_t;
```

---

## 5. Các Hàm Chính (Main Functions)

### 5.1 Khởi Tạo
```c
bool peak_detector_init(peak_detector_t *detector, 
                       uint16_t buffer_size, 
                       uint16_t max_peaks);
```
- Cấp phát bộ nhớ cho các bộ đệm
- Khởi tạo các tham số
- Trả về true nếu thành công

### 5.2 Tái Đặt (Reset)
```c
void peak_detector_reset(peak_detector_t *detector, 
                        float baseline_pressure);
```
- Đặt lại trạng thái cho lần đo mới
- Thiết lập áp suất cơ sở

### 5.3 Xử Lý Mẫu (Process Sample)
```c
peak_data_t* peak_detector_process_sample(peak_detector_t *detector, 
                                         float pressure_value, 
                                         uint32_t timestamp);
```
- Nhận giá trị áp suất thô
- Lọc, tính toán, phát hiện đỉnh
- Trả về dữ liệu phân tích

### 5.4 Tính Toán Huyết Áp (Calculate BP)
```c
bool peak_detector_calculate_bp(peak_detector_t *detector, 
                               bp_measurement_t *measurement);
```
- Phân tích tất cả đỉnh phát hiện
- Tính toán SBP, DBP, MAP, nhịp tim
- Xác thực kết quả
- Trả về true nếu phép đo hợp lệ

---

## 6. Tham Số Cấu Hình (Configuration Parameters)

| Tham số | Giá trị | Ý nghĩa |
|--------|--------|--------|
| `PEAK_DETECTION_ALPHA` | 0.2 | Hệ số bộ lọc thông thấp |
| `MIN_PEAKS_FOR_VALID_BP` | 3 | Số đỉnh tối thiểu cho phép đo |
| `DERIVATIVE_ZERO_CROSSING_THRESH` | 0.01 | Ngưỡng phát hiện đỉnh |
| `MIN_PEAK_DISTANCE_MS` | 300 | Khoảng cách đỉnh tối thiểu (ms) |
| `MAX_MEASUREMENT_DURATION_MS` | 30000 | Thời lượng đo tối đa (30s) |

---

## 7. Ví Dụ Sử Dụng (Usage Example)

### 7.1 Khởi Tạo Hệ Thống
```c
#include "peak_detection.h"
#include "sensor_interface.h"

peak_detector_t detector;

// Khởi tạo
peak_detector_init(&detector, 256, 20);  // Buffer: 256 samples, Max: 20 peaks

// Đặt lại cho lần đo mới (áp suất ban đầu từ cảm biến)
peak_detector_reset(&detector, 40.0);  // mmHg
```

### 7.2 Xử Lý Dữ Liệu
```c
uint16_t adc_value = read_adc();  // Đọc từ ADC
float pressure = sensor_adc_to_pressure_mmhg(adc_value);

// Xử lý mẫu
peak_data_t *result = peak_detector_process_sample(&detector, pressure, time_ms);

if (result->is_peak) {
    // Phát hiện đỉnh - có thể ghi log, bật LED, etc.
    on_peak_detected(result->peak_magnitude);
}
```

### 7.3 Tính Toán Kết Quả
```c
bp_measurement_t bp;

if (peak_detector_get_peaks_count(&detector) >= 5) {
    if (peak_detector_calculate_bp(&detector, &bp)) {
        if (bp.measurement_valid) {
            printf("SBP: %d, DBP: %d, MAP: %d, HR: %d\n",
                   bp.systolic, bp.diastolic, bp.mean_arterial, bp.pulse_rate);
        }
    }
}
```

### 7.4 Dọn Dẹp
```c
peak_detector_deinit(&detector);  // Giải phóng bộ nhớ
```

---

## 8. Chuyển Đổi Cảm Biến (Sensor Conversion)

### 8.1 MPX5050 Sensor
- **Phạm vi áp suất**: 0 - 350 kPa
- **Đầu ra điện áp**: 0.2V - 4.7V
- **Vref STM32**: 3.3V
- **ADC**: 12-bit (0-4095)

### 8.2 Công Thức Chuyển Đổi
```
1. ADC → Điện áp:
   V = (ADC_value / 4095) * 3.3V

2. Điện áp → Áp suất (kPa):
   P = (V - 0.2) / (4.7 - 0.2) * (350 - 0)
   P = (V - 0.2) * 77.1930

3. kPa → mmHg:
   P_mmHg = P_kPa * 7.50061

4. Trực tiếp ADC → mmHg:
   P_mmHg = ((ADC_value / 4095) * 3.3 - 0.2) * 77.1930 * 7.50061
```

---

## 9. Kiểm Chứng Phép Đo (Measurement Validation)

Phép đo được coi là **hợp lệ** khi:

1. ✓ Có ít nhất **3 đỉnh** được phát hiện
2. ✓ **Systolic > Diastolic**
3. ✓ **60 mmHg ≤ Systolic ≤ 250 mmHg**
4. ✓ **30 mmHg ≤ Diastolic ≤ 200 mmHg**
5. ✓ **Thời lượng đo ≤ 30 giây**

---

## 10. Tối Ưu Hóa Hiệu Suất (Performance Optimization)

### 10.1 Điều Chỉnh Hệ Số Bộ Lọc
```c
// Nhiều nhiễu → α nhỏ hơn (ví dụ: 0.1)
peak_detector_set_filter_coefficient(&detector, 0.1);

// Ít nhiễu → α lớn hơn (ví dụ: 0.3)
peak_detector_set_filter_coefficient(&detector, 0.3);
```

### 10.2 Điều Chỉnh Ngưỡng Phát Hiện
```c
// Khó phát hiện đỉnh → Tăng ngưỡng
peak_detector_set_threshold(&detector, 0.7);

// Dễ phát hiện → Giảm ngưỡng
peak_detector_set_threshold(&detector, 0.3);
```

### 10.3 Lấy Thống Kê
```c
float min_p, max_p, mean_p;
peak_detector_get_statistics(&detector, &min_p, &max_p, &mean_p);
printf("Min: %.2f, Max: %.2f, Mean: %.2f\n", min_p, max_p, mean_p);
```

---

## 11. Xử Lý Lỗi (Error Handling)

### 11.1 Khởi Tạo Thất Bại
```c
if (!peak_detector_init(&detector, 256, 20)) {
    printf("Memory allocation failed\n");
    // Xử lý lỗi
}
```

### 11.2 Phép Đo Không Hợp Lệ
```c
if (!peak_detector_calculate_bp(&detector, &bp)) {
    printf("Measurement invalid\n");
    printf("Peaks detected: %d\n", peak_detector_get_peaks_count(&detector));
    // Yêu cầu đo lại
}
```

### 11.3 Phạm Vi Áp Suất
Nếu giá trị áp suất nằm ngoài phạm vi hợp lệ:
- **SBP < 60 mmHg** hoặc **SBP > 250 mmHg** → Lỗi
- **DBP < 30 mmHg** hoặc **DBP > 200 mmHg** → Lỗi

---

## 12. Ghi Chú Kỹ Thuật (Technical Notes)

1. **Tần Suất Lấy Mẫu**: Khuyến nghị 100Hz (10ms/mẫu)
2. **Độ Phân Giải ADC**: 12-bit tối thiểu
3. **Thời Gian Đo**: 20-30 giây cho kết quả chính xác
4. **Nhiệt Độ**: Chuẩn bị đo ở 20-25°C
5. **Vị Trí Cảm Biến**: Cánh tay ở mức tim

---

## 13. Tài Liệu Tham Khảo (References)

- MPX5050 Datasheet - NXP Semiconductors
- Oscillometric Blood Pressure Measurement - IEEE
- STM32F103 Reference Manual - STMicroelectronics
- Signal Processing for Blood Pressure Measurement

---

## 14. Tiếp Theo (Next Steps)

- [ ] Tích hợp với ADC thực tế
- [ ] Thêm UART để xuất dữ liệu
- [ ] Thêm LCD/OLED display
- [ ] Lưu dữ liệu vào flash
- [ ] Viết test case
- [ ] Xác thực với máy đo chính thức

