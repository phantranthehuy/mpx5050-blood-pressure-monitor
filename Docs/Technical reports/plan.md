# Dàn ý báo cáo — Máy đo huyết áp oscillometric (MPX5050 + STM32)

## Thông tin (điền vào `Project_report/main.tex`)

- **Môn / loại báo cáo:** Đồ án / báo cáo môn học EE3037 (điều chỉnh nếu khác).
- **Đề tài:** Thiết kế máy đo huyết áp điện tử phương pháp oscillometric dựa trên cảm biến áp MPX5050, ADC ADS1115 và vi điều khiển STM32F103; phần mềm host Web Serial để lọc tín hiệu và tính MAP/SBP/DBP (MAA).
- **GVHD:** *(điền tên)*  
- **SVTH / MSSV:** *(điền)*  

## Cấu trúc chương (ánh xạ file LaTeX)

1. **Chương 1 — Giới thiệu** (`chap1.tex`)  
   - Tổng quan bài toán đo huyết áp không xâm lấn.  
   - Phương pháp oscillometric, vai trò cuff–cảm biến–MCU–host.  
   - Nhiệm vụ đề tài (phần cứng, firmware, WebApp, an toàn SAF).  
   - Bố cục báo cáo.

2. **Chương 2 — Phần cứng** (`chap2.tex`)  
   - Sơ đồ khối: MPX5050 → điều kiện tín hiệu → ADS1115 (I2C) → STM32.  
   - Điều khiển bơm/van PWM; nút Start/Stop/High; LED trạng thái.  
   - Tham chiếu schematic/PCB (in từ KiCad — file ảnh/PDF trong `images/`).  
   - Giới hạn an toàn: trần áp 280 mmHg (SAF).

3. **Chương 3 — Firmware và thuật toán** (`chap3.tex`)  
   - Kiến trúc PlatformIO / STM32Cube; đọc áp; stream UART 100 Hz (`S,...`).  
   - FSM đo: bơm chậm, lệnh `T,...` từ host, xả chậm đo.  
   - Cơ sở MAA trên host: envelope, MAP, hệ số \(r_s\), \(r_d\) cho SBP/DBP; band-pass 0,5–5 Hz (theo tài liệu nội bộ).  
   - Hiệu chỉnh offset/scale ADC → mmHg.

4. **Chương 4 — Phần mềm host và kiểm thử** (`chap4.tex`)  
   - WebApp (Vite + TypeScript), Web Serial; giao thức `A/E/T/ABORT`.  
   - Luồng `P_\mathrm{sys\,est}` → `T,...`; clamp SAF.  
   - Checklist kiểm thử (UART, LED, SAF, leak timeout).

5. **Kết luận** (`ketluan.tex`)  
   - Kết quả đạt được; hạn chế (hiệu chỉnh, chỉ số MAA cố định); hướng phát triển.

6. **Tài liệu tham khảo** (`tlthamkhao.tex`)  
   - Datasheet NXP MPX5050, TI ADS111x, RM/STM32F103; tài liệu oscillometry / MAA nếu có.

## Hình ảnh đính kèm trong `Project_report/images/`

- `logo-bku.png` — logo trường (bìa).  
- `Print.pdf` — bản in schematic/board (nếu có).  
- `webapp-hero.png` — minh họa giao diện WebApp (tùy chọn).

## Biên dịch

Dùng **XeLaTeX** (class có `fontspec`). Ví dụ: `latexmk -xelatex main.tex` trong thư mục `Project_report`.
