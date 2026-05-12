

Dựa trên bản vẽ Schematic bạn đã cung cấp, để đảm bảo hệ thống phần cứng và phần mềm hoạt động ổn định trước khi tích hợp lên WebApp, bạn cần thực hiện quy trình kiểm thử (Testing) một cách bài bản. 

Dưới đây là bộ **Testcase toàn diện (Hardware Bring-up & Functional Test)** được chia thành từng phân hệ (Subsystem) bám sát trực tiếp vào các linh kiện trên sơ đồ của bạn.

---

### PHẦN 1: KIỂM THỬ NGUỒN (POWER SUPPLY TESTS)

*Mục đích: Đảm bảo không có chạm chập và các mức điện áp cấp cho IC đều chính xác trước khi nạp code.*

| Test ID | Tính năng kiểm tra | Các bước thực hiện (Test Steps) | Kết quả mong đợi (Expected Results) |

| :--- | :--- | :--- | :--- |

| **PWR-01** | Test chạm chập (Short-circuit) | Dùng VOM đo thông mạch giữa các net: `+3.7V - DGND`, `+5V - DGND`, `+3.3V - DGND`. | VOM **KHÔNG** kêu bíp (điện trở > 1kOhm). Không có chạm chập. |

| **PWR-02** | Test Nguồn Pin (BT101) & Sạc (U101) | Cắm pin 3.7V vào BT101. Cắm cáp nguồn vào chân IN của TP4056 (U101). | Đo chân `+3.7V` (OUT+) đạt khoảng 3.7V - 4.2V. Đèn báo sạc trên board sáng. |

| **PWR-03** | Test Mạch Boost 5V (U102) | Cấp nguồn pin. Đo điện áp tại ngõ ra của cuộn cảm L101 / Diode D101 / Tụ C101. | Điện áp đo được ở net `+5V` phải đạt chính xác **5.0V** (điều chỉnh biến trở RV101 nếu cần). |

| **PWR-04** | Test Nguồn Logic (MCU/Cảm biến) | Đo điện áp tại chân cung cấp cho MCU (net `+3.3V`) và chân VDD của cảm biến. | Net `+3.3V` đạt ~3.3V. Net `+5V` tại chân 2 (VS) của MPX5050GP đạt 5V. |

---

### PHẦN 2: KIỂM THỬ KHỐI VI ĐIỀU KHIỂN & NGOẠI VI (MCU & PERIPHERALS)

*Mục đích: Đảm bảo MCU sống, nạp được code và giao tiếp cơ bản với các ngoại vi.*

| Test ID | Tính năng kiểm tra | Các bước thực hiện (Test Steps) | Kết quả mong đợi (Expected Results) |

| :--- | :--- | :--- | :--- |

| **MCU-01** | Nạp Firmware | Cắm mạch nạp ST-Link vào `SW_START`, `SW_STOP` (hoặc SWCLK, SWDIO). Nạp code chớp tắt LED. | MCU nạp code thành công. LED hoạt động. |

| **IO-01** | Đọc Nút nhấn (BUTTON) | Nhấn lần lượt `SW_START`, `SW_STOP`, `SW_HIGH`. Theo dõi trạng thái GPIO trên chế độ Debug. | Trạng thái GPIO thay đổi từ HIGH xuống LOW (vì có kéo trở nội/ngoại). |

| **IO-02** | Điều khiển LED | Viết code xuất mức logic HIGH/LOW ra các chân của `D401` (Đỏ), `D402` (Xanh lá), `D403` (Vàng). | Các LED sáng/tắt tương ứng theo lệnh. Độ sáng tốt (nhờ trở 220R). |

| **COM-01** | Giao tiếp UART | Viết code gửi chuỗi "Hello WebApp" qua `UART_TX` (J101). Kết nối USB-TTL xem trên máy tính. | Terminal PC hiển thị đúng chuỗi với Baudrate 115200. Không bị lỗi font. |

---

### PHẦN 3: KIỂM THỬ CƠ CẤU CHẤP HÀNH (PWM & DRIVER)

*Mục đích: Kiểm tra khả năng đóng ngắt dòng điện lớn cho Bơm và Van khí.*

| Test ID | Tính năng kiểm tra | Các bước thực hiện (Test Steps) | Kết quả mong đợi (Expected Results) |

| :--- | :--- | :--- | :--- |

| **DRV-01** | MOSFET Bơm (Q201 - AO3400A) | Xuất PWM 50% và 100% ra chân `PWM_Motor`. Dùng VOM hoặc Osilloscope đo tại J201. | Động cơ bơm quay. Ở PWM 50% bơm yếu hơn 100%. MOSFET Q201 không bị nóng ran. |

| **DRV-02** | MOSFET Van (Q202 - AO3400A) | Xuất PWM 0%, 30% và 100% ra chân `PWM_Valve`. | Van xả đóng chặt ở 0% (hoặc 100% tùy loại van thường đóng/mở). Van xả từ từ ở 30%. |

| **DRV-03** | Test Diode Xả (D201, D202) | Cho Bơm/Van hoạt động và tắt đột ngột. Đo tín hiệu tại chân Drain của Mosfet. | Diode 1N5819 dập tắt xung áp ngược (Flyback), không có xung điện áp âm làm hỏng MCU. |

---

### PHẦN 4: KIỂM THỬ KHỐI CẢM BIẾN & XỬ LÝ TÍN HIỆU TƯƠNG TỰ (SENSOR & ADC_BLOCK)

*Mục đích: Đảm bảo tín hiệu áp suất được số hoá với độ nhiễu thấp nhất.*

| Test ID | Tính năng kiểm tra | Các bước thực hiện (Test Steps) | Kết quả mong đợi (Expected Results) |

| :--- | :--- | :--- | :--- |

| **SEN-01** | Giao tiếp I2C Level Shifter | Scan I2C bus trên STM32 (qua mạch dịch mức Q301, Q302). | Tìm thấy địa chỉ I2C của ADS1115 là `0x48` (theo Note trên schematic: Default I2C Address = 1001000). |

| **SEN-02** | Đọc ADC tĩnh (0 mmHg) | Để hở vòng bít (không bơm). Đọc giá trị ADC từ U501 (kênh AIN0). | ADC trả về một giá trị cố định (Offset của cảm biến). Dao động nhiễu (noise) phải rất nhỏ (vài LSB). |

| **SEN-03** | Tuyến tính cảm biến | Bơm một lượng khí nhất định, kẹp chặt ống. Đọc lại giá trị ADC. | Giá trị ADC tăng lên đáng kể và giữ ổn định, không bị trôi (Drift) quá nhiều. |

---

### PHẦN 5: KIỂM THỬ CHỨC NĂNG HỆ THỐNG (SYSTEM INTEGRATION & STATE MACHINE)

*Mục đích: Giả lập quy trình đo huyết áp thực tế phối hợp giữa Hardware và thuật toán.*

| Test ID | Tính năng kiểm tra | Các bước thực hiện (Test Steps) | Kết quả mong đợi (Expected Results) |

| :--- | :--- | :--- | :--- |

| **SYS-01** | Trạng thái INFLATE (Bơm) | Quấn vòng bít vào tay. Nhấn `SW_START`. | Bơm chạy 100%. Van đóng chặt. Áp suất tăng dần trên đồ thị. Đạt ~160mmHg thì bơm tự động dừng. |

| **SYS-02** | Trạng thái DEFLATE (Xả từ từ) | Ngay sau khi bơm dừng. | Van mở một góc nhỏ (PWM ~30-40%). Áp suất giảm đều đặn từ 2-3 mmHg/giây. |

| **SYS-03** | Trích xuất tín hiệu (WebApp/UART) | Theo dõi chuỗi UART đẩy lên máy tính liên tục trong lúc xả (Ví dụ: `D,120.5,1.2`). | Trên WebApp vẽ được 2 đồ thị: Đường DC (giảm dần mượt mà) và Đường AC (có các đỉnh dao động nhịp tim). |

| **SYS-04** | Trạng thái CALCULATE & END | Sau khi áp suất DC giảm xuống dưới 40mmHg. | Van mở 100% để xả nhanh khí còn dư. Thuật toán tính toán và trả về kết quả SYS/DIA/HR hợp lý (VD: 120/80). |

---

### PHẦN 6: KIỂM THỬ AN TOÀN VÀ XỬ LÝ LỖI (SAFETY & EDGE CASES)

*Mục đích: Đây là thiết bị y tế, tiêu chuẩn an toàn là tối thượng. Thiết bị KHÔNG ĐƯỢC làm tổn thương tay người dùng.*

| Test ID | Tính năng kiểm tra | Các bước thực hiện (Test Steps) | Kết quả mong đợi (Expected Results) |

| :--- | :--- | :--- | :--- |

| **SAF-01** | Bấm dừng khẩn cấp | Đang bơm nửa chừng (hoặc đang xả), nhấn nút `SW_STOP`. | Bơm TẮT NGAY LẬP TỨC. Van MỞ 100% ngay lập tức. Đèn Red chớp báo hiệu. |

| **SAF-02** | Cắt quá áp bằng Firmware | Ghi đè code bơm liên tục. Chặn không cho van xả. | Khi áp suất ADC đọc được vượt quá **280 mmHg**, Firmware phải tự động ngắt Bơm và mở Van 100% (Safety Limit). |

| **SAF-03** | Lỗi hở vòng bít (Timeout) | Không cắm vòng bít vào máy. Nhấn Start. | Bơm chạy nhưng áp suất ADC không tăng. Sau 10 giây (Timeout), hệ thống báo lỗi hở khí, tự tắt bơm. |

| **SAF-04** | Lỗi đứt dây cảm biến | Rút chân SDA/SCL hoặc ngắt nguồn cảm biến MPX5050. | Hàm đọc I2C trả về lỗi. Hệ thống vào trạng thái Error, không cho phép bật Bơm. |

---

**Lời khuyên cho kỹ sư phát triển:** 

Bạn nên in bảng này ra thành Check-list. Cứ làm xong hàm C nào hoặc hàn xong cụm linh kiện nào, bạn test và tích (✓) vào đó. Qua được 100% các Test case này, hệ thống của bạn hoàn toàn đủ tiêu chuẩn để demo và bảo vệ dự án một cách cực kỳ tự tin!