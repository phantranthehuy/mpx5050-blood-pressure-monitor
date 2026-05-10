### 1. KIẾN TRÚC FÍRMWARE (Firmware Architecture)

Do vi điều khiển phải làm nhiều việc cùng lúc (đọc I2C, lọc nhiễu, tính toán, gửi UART, điều khiển bơm/van), bạn bắt buộc phải thiết kế code theo dạng **Máy trạng thái (State Machine)** kết hợp với **Ngắt Timer (Timer Interrupt)**.

**Các trạng thái (States) chính của hệ thống:**

1. `STATE_IDLE`: Chờ lệnh bắt đầu từ nút nhấn hoặc từ WebApp.

2. `STATE_INFLATE`: Bật bơm (PWM = 100%), đóng van. Bơm vòng bít lên ngưỡng ~160 - 180 mmHg.

3. `STATE_DEFLATE_MEASURE`: Tắt bơm, mở van từ từ (PWM ~30-40%) để xả khí chậm (tốc độ lý tưởng là giảm 3-5 mmHg/giây). Bắt đầu thu thập và xử lý tín hiệu AC/DC tại đây.

4. `STATE_CALCULATE`: Xả nhanh toàn bộ khí (Mở van 100%). Chạy thuật toán tìm MAP, SYS (Tâm thu), DIA (Tâm trương).

5. `STATE_SEND_RESULT`: Gửi kết quả cuối cùng qua UART và quay về `IDLE`.

---

### 2. THUẬT TOÁN TÁCH TÍN HIỆU (AC/DC Separation) TRÊN STM32

STM32F103 (Core M3) **không có bộ xử lý số thực phần cứng (No Hardware FPU)**, nên các phép toán `float` sẽ chạy bằng phần mềm hơi chậm một chút. Tuy nhiên, với tần số lấy mẫu chậm (100Hz = 10ms/lần), STM32F103 hoàn toàn dư sức gánh vác.

Để tiết kiệm tài nguyên CPU, ta không dùng các bộ lọc bậc cao phức tạp, mà dùng **Bộ lọc trung bình trượt theo hàm mũ (Exponential Moving Average - EMA Filter)** hoặc IIR bậc 1.

**Công thức C cốt lõi đặt trong ngắt Timer 10ms (100Hz):**

```c

// Các biến toàn cục

float raw_pressure = 0.0;

float dc_pressure = 0.0;   // Áp suất vòng bít (DC)

float ac_pulse = 0.0;      // Dao động nhịp tim (AC)

// Trọng số cho bộ lọc thông thấp (Low-pass filter) - Điều chỉnh từ 0.01 đến 0.1

#define ALPHA_DC 0.05 

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {

    if (htim->Instance == TIM2) { // Ngắt chu kỳ 10ms

        if(current_state == STATE_DEFLATE_MEASURE) {

            

            // 1. Đọc giá trị thô từ ADS1115 (qua I2C)

            int16_t adc_raw = ADS1115_Read();

            

            // Chuyển đổi ADC sang mmHg (Cần hệ số calib thực tế của MPX5050GP)

            raw_pressure = (adc_raw - OFFSET) * SCALE_FACTOR; 

            

            // 2. Tách tín hiệu DC (Áp suất tĩnh của vòng bít) bằng Low-Pass Filter

            dc_pressure = (ALPHA_DC  *raw_pressure) + ((1.0 - ALPHA_DC)*  dc_pressure);

            

            // 3. Tách tín hiệu AC (Dao động mạch máu)

            // Tín hiệu AC chính là Tín hiệu thô trừ đi tín hiệu gốc DC

            ac_pulse = raw_pressure - dc_pressure;

            

            // (Tùy chọn) Đưa ac_pulse qua một bộ lọc Bandpass/Moving Average nhẹ nữa để cho mượt đồ thị

            

            // 4. Bắn dữ liệu THỜI GIAN THỰC qua UART lên WebApp để vẽ đồ thị

            Send_Realtime_UART(dc_pressure, ac_pulse);

            

            // 5. Lưu vào mảng để thuật toán tính Huyết áp xử lý

            Process_Oscillometric_Data(dc_pressure, ac_pulse);

        }

    }

}

```

---

### 3. THUẬT TOÁN TÍNH HUYẾT ÁP (The Oscillometric Algorithm)

Lưu ý quan trọng: RAM của STM32F103C8T6 chỉ có **20KB**. Nếu quá trình xả khí mất 30 giây ở 100Hz, bạn sẽ có 3000 mẫu dữ liệu. Không thể lưu tất cả vào RAM. **Bạn chỉ được phép lưu các "ĐỈNH" (Peaks) của tín hiệu AC.**

**Cách làm (Bên trong hàm `Process_Oscillometric_Data`):**

1. **Tìm đỉnh (Peak Detection):** Liên tục theo dõi biến `ac_pulse`. Nếu thấy tín hiệu đi lên rồi đi xuống, đó là 1 nhịp tim (1 đỉnh).

2. **Lưu đỉnh:** Ghi lại **Biên độ của đỉnh AC đó (Amplitude)** và **Giá trị DC tương ứng** tại thời điểm đó vào 2 mảng nhỏ (khoảng 50-80 phần tử là đủ cho 1 lần đo).

3. **Tính toán (Khi chuyển sang `STATE_CALCULATE`):**

    *Tìm đỉnh AC có biên độ lớn nhất trong mảng $\rightarrow$ Đó chính là* *MAP (Mean Arterial Pressure - Huyết áp trung bình)**.

   * **Huyết áp tâm thu (SYS):** Tìm ngược từ vị trí MAP về phía đầu mảng, tìm điểm có biên độ AC xấp xỉ bằng `0.55 * Max_AC`. Lấy áp suất DC tại điểm đó làm Tâm Thu.

   * **Huyết áp tâm trương (DIA):** Tìm xuôi từ vị trí MAP về phía cuối mảng, tìm điểm có biên độ AC xấp xỉ bằng `0.85 * Max_AC`. Lấy áp suất DC tại điểm đó làm Tâm Trương.

   * *(Lưu ý: Hệ số 0.55 và 0.85 là hệ số thực nghiệm tiêu chuẩn, bạn có thể phải tinh chỉnh lại lúc test thực tế).*

---

### 4. GIAO THỨC TRUYỀN DỮ LIỆU QUA UART (UART Protocol Formulation)

Để WebApp có thể phân biệt được đâu là dữ liệu đang vẽ đồ thị, đâu là kết quả cuối cùng, bạn cần quy định một Format truyền chuỗi (String Protocol) đơn giản.

*Khuyến nghị set Baudrate UART: 115200 bps.*

**A. Khung truyền Real-time (Gửi mỗi 10ms để vẽ đồ thị):**

Sử dụng format chữ cái đầu làm cờ báo.

*Format:* `D,<DC_Value>,<AC_Value>\n`

*Ví dụ STM32 gửi:*

```text

D,140.5,0.12

D,140.2,0.45

D,139.8,1.20   <- Đang có mạch đập

```

**B. Khung truyền Kết quả (Chỉ gửi 1 lần khi đo xong):**

*Format:* `R,<SYS>,<DIA>,<HR>\n` (HR là Heart Rate - Nhịp tim)

*Ví dụ STM32 gửi:*

```text

R,120,80,75

```

**Code C mô phỏng khối gửi UART:**

```c

char uart_buf[50];

// Hàm gửi lúc đang đo

void Send_Realtime_UART(float dc, float ac) {

    // Dùng sprintf để format chuỗi

    sprintf(uart_buf, "D,%.1f,%.2f\n", dc, ac);

    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 10);

}

// Hàm gửi lúc đo xong

void Send_Result_UART(int sys, int dia, int hr) {

    sprintf(uart_buf, "R,%d,%d,%d\n", sys, dia, hr);

    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

}

```

---

### TÓM LẠI: Ưu/Nhược điểm của phương pháp này

**Ưu điểm:**

*   **Bảo mật & Đóng gói:** Thuật toán (chất xám của bạn) nằm toàn bộ trên MCU, không bị lộ code trên trình duyệt.

*   **Đúng chuẩn Y tế:** Nếu WebApp bị treo, trình duyệt bị crash, dây cáp lỏng... Máy đo vẫn tự động hoạt động, tự động xả van an toàn vì não bộ điều khiển (STM32) làm chủ hoàn toàn quá trình cơ học.

*   **WebApp nhẹ nhàng:** WebApp lúc này chỉ cần đọc chuỗi Text từ Serial, tách dấu phẩy `,`) và push vào Chart. Cực kỳ nhẹ, mượt và dễ lập trình.

**Nhược điểm (Thách thức cho người code):**

*   Bạn phải có kỹ năng code C/C++ khá tốt, đặc biệt là kiểm soát bộ nhớ RAM khi thao tác mảng.

*   Phải tự viết thuật toán tìm đỉnh (Peak Detection) trên C.

*   Khó Debug thuật toán hơn so với Web: Khi thuật toán tính sai, bạn phải in các mảng dữ liệu ra console để xem tại sao nó bắt nhầm đỉnh, thay vì dùng các công cụ debug trực quan như trên web.

*Gợi ý:* Trong giai đoạn đầu, bạn cứ in TOÀN BỘ dữ liệu thô `[Thời gian, Giá trị ADC]` ra Serial Monitor, copy vào Excel để vẽ biểu đồ và tìm hệ số lọc (Alpha) cũng như hệ số biên độ (0.55/0.85) chuẩn xác nhất cho cảm biến của mình, sau đó mới viết thuật toán nhúng hẳn vào STM32.