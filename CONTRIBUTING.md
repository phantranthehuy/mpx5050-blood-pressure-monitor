# 📖 Hướng Dẫn Sử Dụng GitHub Cho Team Huyết Áp

Chào anh em, để dự án không bị tình trạng *"Ủa file mạch tao vừa sửa đâu mất rồi?"* hay *"Đứa nào chép đè code của tao?"*, nhóm mình sẽ dùng GitHub để quản lý. 

Mọi người chịu khó đọc kỹ 5 phút để quá trình làm việc mượt mà nhé!

---

## 🛠 Bước 1: Chuẩn bị "đồ nghề" (Chỉ làm 1 lần)
1. **Tạo tài khoản:** Ai chưa có thì lên [GitHub.com](https://github.com/) tạo một tài khoản. Báo tên tài khoản cho **Huy** để Huy add vào dự án.
2. **Cài đặt Git:** Tải và cài đặt [Git tại đây](https://git-scm.com/downloads) (Cứ Next liên tục là được).
3. **Cài phần mềm quản lý (Khuyên dùng):**
   * Tải [GitHub Desktop](https://desktop.github.com/). Giao diện trực quan, bấm nút là xong, không cần gõ lệnh hệt như dùng Google Drive.
   * Riêng **Tùng** (viết code): Có thể dùng luôn công cụ Source Control có sẵn bên cột trái của VS Code cho tiện.

---

## 🌳 Bước 2: Quy tắc "Nhánh" (Branch) - BẮT BUỘC ĐỌC
Tưởng tượng dự án của mình là một cái cây:
* **Nhánh `main` (Thân cây):** Đây là nhánh THIÊNG LIÊNG. Chỉ chứa code đã chạy được, mạch đã vẽ xong. **Tuyệt đối không ai làm việc trực tiếp trên nhánh `main`**.
* **Nhánh cá nhân (Cành cây):** Mỗi người sẽ có một nhánh riêng để làm nháp. Sai thì xóa làm lại, không ảnh hưởng tới ai.

**Phân chia nhánh của nhóm mình:**
* `hw-huy`: Chỗ Huy vẽ mạch Altium/KiCad.
* `mech-ly`: Chỗ Ly up file 3D vỏ hộp và code giao diện.
* `algo-hoang`: Chỗ Hoàng up file Python/MATLAB, tài liệu thuật toán.
* `fw-tung`: Chỗ Tùng viết code C trên STM32.

---

## 🔄 Bước 3: Vòng lặp làm việc hàng ngày

Mỗi khi ngồi vào máy tính làm dự án, hãy nhẩm thần chú 4 bước này (dùng GitHub Desktop hoặc VS Code):

### 1️⃣ KÉO (PULL) - Lấy cái mới nhất về
* Trước khi bắt tay vào làm, luôn bấm nút **Fetch origin / Pull** để cập nhật xem đêm qua có ai
