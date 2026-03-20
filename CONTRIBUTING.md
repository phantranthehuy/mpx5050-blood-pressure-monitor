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
* Trước khi bắt tay vào làm, luôn bấm nút **Fetch origin / Pull** để cập nhật xem đêm qua có ai đẩy cái gì mới lên không.

### 2️⃣ LÀM VIỆC (WORK)
* Nhớ chuyển sang **nhánh của mình** (ví dụ Tùng thì chọn nhánh `fw-tung`).
* Mở file lên sửa code, vẽ mạch, lưu file (`Ctrl + S`) bình thường trên máy tính.

### 3️⃣ LƯU NHÁP (COMMIT) - Quan trọng!
* Khi làm xong một cụm việc (VD: *Tùng vừa viết xong hàm xả van*, *Huy vừa đi dây xong phần nguồn*), hãy mở GitHub Desktop lên. Nó sẽ hiện ra các file bạn vừa sửa.
* Ở góc trái bên dưới, điền 1 dòng tin nhắn ngắn gọn vào ô **Summary** (VD: *"Hoàn thành hàm điều khiển van PWM"*).
* Bấm nút **Commit to [tên nhánh của bạn]**. (Hành động này giống như bạn Save As một phiên bản vĩnh viễn trên máy của bạn).

### 4️⃣ ĐẨY LÊN (PUSH) - Báo cáo với anh em
* Bấm nút **Push origin** trên cùng. Thao tác này sẽ đẩy cái bản nháp bạn vừa Commit lên mạng để cả nhóm cùng thấy.

---

## 🤝 Bước 4: Cách gộp bài (Pull Request)
Khi bạn thấy tính năng mình làm trên nhánh cá nhân đã hoàn hảo và muốn đưa nó vào bản chính thức (nhánh `main`), bạn làm như sau:

1. Lên trang web GitHub của dự án.
2. Bấm vào nút màu xanh **Compare & pull request**.
3. Viết vài dòng mô tả bạn vừa làm gì.
4. Bấm **Create pull request**.
5. **Huy (Quản lý)** sẽ vào kiểm tra. Nếu ok, Huy sẽ bấm **Merge** để gộp nhánh của bạn vào `main`.

---

## ⚠️ 3 Lời thề "Xương Máu" của nhóm
1. **Không động vào đồ của nhau:** Tùng chỉ sửa file trong thư mục `/Firmware`. Huy chỉ sửa file trong `/Hardware`. Như vậy KHÔNG BAO GIỜ bị xung đột (Conflict).
2. **Commit thường xuyên:** Đừng để 1 tuần mới Commit 1 lần. Cứ làm xong 1 tính năng nhỏ, chạy thử thấy OK là Commit ngay.
3. **Mạch phần cứng (Huy & Ly chú ý):** Git không thể trộn (merge) file bản vẽ mạch `.PcbDoc` hay file cơ khí 3D. Nên mỗi khi vẽ xong, ngoài file gốc, hãy xuất thêm 1 file **PDF (cho mạch)** hoặc **ảnh chụp màn hình** đẩy lên cùng để Hoàng và Tùng có thể xem chân cắm (Pinout) mà không cần cài Altium.

Chúc anh em code không bug, mạch không cháy nổ! 🚀
