

Dựa trên các yêu cầu về hiệu ứng hiển thị và thông số chân vi điều khiển (PB13, PB14, PB15) từ Schematic, tôi đã viết lại **toàn bộ giải thuật điều khiển LED hoàn chỉnh, chuẩn mực cho vi điều khiển nhúng**.

Giải thuật này được nâng cấp thêm một tính năng cực kỳ quan trọng: **Nhận diện thay đổi trạng thái (State-change detection)**. Nghĩa là ngay khi người dùng bấm nút hoặc máy chuyển sang bước đo tiếp theo, đèn sẽ thay đổi NGAY LẬP TỨC mà không bị trễ, đồng thời hoàn toàn không dùng hàm `HAL_Delay()` để tránh làm treo hệ thống đo huyết áp.

Bạn có thể tạo một file riêng (VD: `led_hmi.c` và `led_hmi.h`) hoặc copy trực tiếp đoạn code này vào khu vực *User Code* trong file `main.c`.

### ĐOẠN CODE C ĐẦY ĐỦ (Dành cho STM32 HAL Library)

```c

#include "main.h"

// ====================================================================

// 1. KHAI BÁO CHÂN PHẦN CỨNG (Dựa theo Schematic trang 1)

// ====================================================================

#define LED_RED_PORT      GPIOB

#define LED_RED_PIN       GPIO_PIN_13     // D401

#define LED_GREEN_PORT    GPIOB

#define LED_GREEN_PIN     GPIO_PIN_14     // D402

#define LED_YELLOW_PORT   GPIOB

#define LED_YELLOW_PIN    GPIO_PIN_15     // D403

// ====================================================================

// 2. KHAI BÁO CHU KỲ NHẤP NHÁY (Thời gian bằng mili-giây)

// ====================================================================

#define BLINK_NORMAL_MS   500  // Nháy chậm (Bơm căng) - 500ms sáng, 500ms tắt

#define BLINK_FAST_MS     150  // Nháy nhanh (Khẩn cấp) - 150ms sáng, 150ms tắt

// ====================================================================

// 3. ĐỊNH NGHĨA TRẠNG THÁI HỆ THỐNG (ENUM)

// ====================================================================

typedef enum {

    SYS_STATE_IDLE = 0,    // Chờ / Sẵn sàng đo

    SYS_STATE_INFLATING,   // Đang bơm căng vòng bít (Đo cao huyết áp)

    SYS_STATE_MEASURING,   // Đang xả khí để lấy mẫu tính huyết áp

    SYS_STATE_ERROR,       // Lỗi đo (hở khí, rung tay...)

    SYS_STATE_EMERGENCY    // Dừng khẩn cấp (Người dùng ấn nút STOP)

} SystemState_t;

// Biến toàn cục đại diện cho trạng thái hiện tại của máy (bạn sẽ đổi biến này ở hàm khác)

SystemState_t current_sys_state = SYS_STATE_IDLE; 

// ====================================================================

// 4. HÀM CẬP NHẬT TRẠNG THÁI LED (Đặt vào trong vòng lặp while(1))

// ====================================================================

void LED_Update_Task(void) 

{

    // Các biến static giúp lưu giữ giá trị qua mỗi vòng lặp

    static uint32_t previous_tick = 0;

    static uint8_t  blink_flag = 0;

    static SystemState_t last_sys_state = 255; // Giá trị ảo ban đầu để ép làm mới trạng thái

    

    uint32_t current_tick = HAL_GetTick(); // Đọc bộ đếm thời gian thực của STM32

    uint8_t state_changed = 0;             // Cờ báo hiệu có sự chuyển đổi trạng thái

    // Nếu trạng thái máy vừa bị thay đổi (Vd: Bấm nút START chuyển từ IDLE -> INFLATING)

    if (current_sys_state != last_sys_state) 

    {

        state_changed = 1;

        last_sys_state = current_sys_state;

        

        // Reset lại thời gian và cờ nhấp nháy để đèn sáng/tắt ngay lập tức không bị trễ

        previous_tick = current_tick;

        blink_flag = 1; 

    }

    // Xử lý logic LED tùy theo trạng thái

    switch (current_sys_state) 

    {

        case SYS_STATE_IDLE:

            // Yêu cầu: Đèn Xanh lá SÁNG, các đèn khác TẮT

            if (state_changed) {

                HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);

                HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);

                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);

            }

            break;

        case SYS_STATE_INFLATING:

            // Yêu cầu: Đèn Vàng NHẤP NHÁY chậm, các đèn khác TẮT

            if (state_changed) {

                HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);

                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);

            }

            // Logic nhấp nháy Non-blocking

            if (current_tick - previous_tick >= BLINK_NORMAL_MS) {

                previous_tick = current_tick;

                blink_flag = !blink_flag; // Đảo trạng thái (Sáng <-> Tắt)

                HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, blink_flag ? GPIO_PIN_SET : GPIO_PIN_RESET);

            }

            break;

        case SYS_STATE_MEASURING:

            // Yêu cầu: Đèn Vàng SÁNG giữ nguyên, các đèn khác TẮT

            if (state_changed) {

                HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);

                HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_SET);

                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);

            }

            break;

        case SYS_STATE_ERROR:

            // Yêu cầu: Đèn Đỏ SÁNG, báo lỗi (cần kiểm tra lại vòng bít/tay rung)

            if (state_changed) {

                HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);

                HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);

                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);

            }

            break;

        case SYS_STATE_EMERGENCY:

            // Yêu cầu: Đèn Đỏ NHẤP NHÁY NHANH, báo động dừng khẩn cấp

            if (state_changed) {

                HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);

                HAL_GPIO_WritePin(LED_YELLOW_PORT, LED_YELLOW_PIN, GPIO_PIN_RESET);

            }

            // Logic nhấp nháy Non-blocking (Tốc độ cao)

            if (current_tick - previous_tick >= BLINK_FAST_MS) {

                previous_tick = current_tick;

                blink_flag = !blink_flag; // Đảo trạng thái liên tục

                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, blink_flag ? GPIO_PIN_SET : GPIO_PIN_RESET);

            }

            break;

    }

}

```

### Hướng dẫn tích hợp vào chương trình chính

Trong hàm `main()` của bạn, cách thức hoạt động sẽ rất đơn giản và tách biệt. Khi một sự kiện xảy ra (như đọc nút nhấn), bạn chỉ việc thay đổi biến `current_sys_state`, hệ thống đèn sẽ tự động nhận biết và chạy đúng hiệu ứng!

```c

int main(void) {

    // 1. Gọi các hàm khởi tạo (HAL_Init, Cấu hình Clock, GPIO...)

    // Đảm bảo chân PB13, PB14, PB15 đã được set là GPIO Output trên CubeMX

    

    // Gán trạng thái khởi động mặc định

    current_sys_state = SYS_STATE_IDLE; 

    while (1) {

        

        // ------------- LUÔN GỌI HÀM LED NÀY ĐỂ ĐÈN CẬP NHẬT -------------

        LED_Update_Task(); 

        // ------------- VÍ DỤ VỀ CÁCH ĐỔI TRẠNG THÁI -------------

        // 1. Khi người dùng nhấn nút START (SW_START chân PB12)

        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET) {

             current_sys_state = SYS_STATE_INFLATING; // Bật bơm, đèn vàng tự động nháy

             // Code bật GPIO cho Mosfet Bơm (Q201) chạy ở đây...

        }

        

        // 2. Khi người dùng nhấn khẩn cấp STOP (SW_STOP chân PB11)

        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_RESET) {

             current_sys_state = SYS_STATE_EMERGENCY; // Đèn đỏ tự động nháy liên tục

             // Code tắt Bơm, mở 100% Van (Q202) ở đây...

        }

        

        // Code đọc ADC và truyền UART nằm ở đây...

    }

}

```

**Ưu điểm của giải thuật này:**

1. Code cực nhẹ, chạy siêu tốc độ, **tôn trọng tuyệt đối thời gian thực** của máy đo huyết áp.

2. Kiểm tra `state_changed` giúp MCU không phải gọi hàm `HAL_GPIO_WritePin()` liên tục hàng nghìn lần mỗi giây, giúp tiết kiệm điện năng cho mạch.

3. Rất dễ mở rộng: Nếu sau này bạn thêm màn hình LCD hay còi Buzzer, bạn chỉ việc nhét thêm lệnh vào bên trong khối `if (state_changed)` là xong.

---

### Ánh xạ sang firmware (plan đo WebApp + UART)

Trong repo firmware, không dùng biến toàn cục `current_sys_state` như ví dụ trên; thay vào đó:

| `LedHmiSystemState_t` (LED_Algorithm) | Trạng thái máy (plan / `BpState`) |
|--------------------------------------|-------------------------------------|
| `LED_SYS_STATE_IDLE` | `STATE_IDLE`, và `STATE_DONE` (chờ về idle xanh) |
| `LED_SYS_STATE_INFLATING` | `STATE_INFLATE_SLOW_LISTEN` + `STATE_INFLATE_TO_MARGIN` |
| `LED_SYS_STATE_MEASURING` | `STATE_DEFLATE_MEASURE`; và `STATE_FAST_DEFLATE` **bình thường** sau xả |
| `LED_SYS_STATE_ERROR` | `STATE_ERROR` (hở khí / I2C…) |
| `LED_SYS_STATE_EMERGENCY` | `STATE_FAST_DEFLATE` khi **STOP**, **quá áp (≥ trần SAF hiệu lực)**, hoặc **ABORT** UART |

**API:** `LedHmiSystemState_t bp_fsm_led_hmi_state(void)` trong [`bp_fsm.c`](src/bp_fsm.c); hiển thị không chặn: `led_hmi_task(bp_fsm_led_hmi_state())` trong [`main.c`](src/main.c). Logic nhấp nháy nằm trong [`led_hmi.c`](src/led_hmi.c), chân LED theo schematic **PB13/PB14/PB15** trong [`board_config.h`](include/board_config.h).