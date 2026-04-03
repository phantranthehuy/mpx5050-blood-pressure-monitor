# 📖 Hướng Dẫn Sử Dụng GitHub Cho Team Huyết Áp

Chào anh em, để dự án không bị tình trạng *"Ủa file mạch tao vừa sửa đâu mất rồi?"* hay *"Đứa nào chép đè code của tao?"*, nhóm mình sẽ dùng GitHub để quản lý. 

Mọi người chịu khó đọc kỹ 5 phút để quá trình làm việc mượt mà nhé!

---

## ⚠️ 3 Lời Thề "Xương Máu" Của Nhóm (BẮT BUỘC ĐỌC)
1. **Không động vào đồ của nhau:** Tùng chỉ sửa file trong thư mục `/Firmware`. Huy chỉ sửa file trong `/Hardware`. Như vậy KHÔNG BAO GIỜ bị xung đột (Conflict).
2. **Commit thường xuyên:** Đừng để 1 tuần mới Commit 1 lần. Cứ làm xong 1 tính năng nhỏ, chạy thử thấy OK là Commit ngay.
3. **Lưu ý với mạch phần cứng (Huy & Ly):** Git không thể trộn (merge) file bản vẽ `.PcbDoc` hay file cơ khí 3D. Nên mỗi khi vẽ xong, ngoài file gốc, hãy xuất thêm 1 file **PDF (cho mạch)** hoặc **Ảnh chụp màn hình** đẩy lên cùng để Hoàng và Tùng có thể xem chân cắm (Pinout) mà không cần cài Altium/Solidworks.

---

## 🌳 Quy Tắc "Nhánh" (Branch)
Tưởng tượng dự án của mình là một cái cây:
* **Nhánh `main` (Thân cây):** Đây là nhánh THIÊNG LIÊNG. Chỉ chứa code đã chạy được, mạch đã vẽ xong. **Tuyệt đối không ai làm việc trực tiếp trên nhánh `main`**.
* **Nhánh cá nhân (Cành cây):** Mỗi người sẽ có một nhánh riêng để làm nháp. Sai thì xóa làm lại, không ảnh hưởng tới ai.

**Phân chia nhiệm vụ và nhánh:**
* ⚡ `hw-huy`: Chỗ Huy vẽ mạch Altium/KiCad.
* ⚙️ `mech-ly`: Chỗ Ly up file 3D vỏ hộp và code giao diện.
* 🧠 `algo-hoang`: Chỗ Hoàng up file Python/MATLAB, tài liệu thuật toán.
* 💻 `fw-tung`: Chỗ Tùng viết code C trên STM32.

---

## 🛠 Bước 1: Chuẩn bị "đồ nghề" (Chỉ làm 1 lần)
1. **Tạo tài khoản:** Ai chưa có thì lên [GitHub.com](https://github.com/) tạo tài khoản. Báo tên user cho **Huy** để Huy add vào dự án.
2. **Cài đặt Git:** Tải và cài đặt [Git tại đây](https://git-scm.com/downloads) (Cứ Next liên tục là được).
3. **Cài phần mềm quản lý:**
   * **Khuyên dùng:** Tải [GitHub Desktop](https://desktop.github.com/). Giao diện trực quan, bấm nút là xong, không cần gõ lệnh.
   * **Riêng Tùng (Coder):** Có thể dùng luôn công cụ Source Control có sẵn bên cột trái của VS Code cho tiện.

---

## 🔄 Bước 2: Vòng lặp làm việc (Chọn 1 trong 3 cách dưới đây)

Mỗi khi ngồi vào máy tính làm dự án, hãy nhẩm thần chú 4 bước: **KÉO (Pull) ➡️ LÀM VIỆC (Work) ➡️ LƯU NHÁP (Commit) ➡️ ĐẨY LÊN (Push)**. 

Dưới đây là hướng dẫn chi tiết theo công cụ bạn chọn:

### 🟢 Cách 1: Dành cho người mới - Dùng GitHub Desktop
* **(Lần đầu tiên) Tải dự án:** Chọn `File` ➡️ `Clone repository...` ➡️ Chọn tên dự án ➡️ `Clone`.
* **(Lần đầu tiên) Chọn nhánh:** Nhìn lên chữ `Current Branch` ở giữa phía trên ➡️ Chọn nhánh của bạn (VD: `hw-huy`).
1. **Kéo (Pull):** Bấm nút `Fetch origin` ở góc trên bên phải. Nếu có code mới từ người khác, nó sẽ hiện nút `Pull origin`. Bấm vào đó.
2. **Làm việc:** Mở thư mục dự án, sửa file, vẽ mạch, lưu file (`Ctrl + S`) bình thường.
3. **Lưu nháp (Commit):** Mở lại GitHub Desktop. Điền tóm tắt công việc vào ô **Summary** ở góc trái phía dưới (VD: *Đi dây xong phần nguồn*) ➡️ Bấm `Commit to [tên nhánh]`.
4. **Đẩy lên (Push):** Bấm nút `Push origin` ở thanh trên cùng để báo cáo với anh em.

### 🔵 Cách 2: Dành cho Coder - Dùng trực tiếp VS Code (Tùng chú ý)
* **(Lần đầu tiên) Tải dự án:** Mở VS Code ➡️ Bấm `Ctrl + Shift + P` ➡️ Gõ `Git: Clone` ➡️ Dán link dự án vào ➡️ Chọn thư mục lưu.
* **(Lần đầu tiên) Chọn nhánh:** Nhìn xuống **góc dưới cùng bên trái** (thanh màu xanh), bấm vào tên nhánh hiện tại và chọn `fw-tung`.
1. **Kéo (Pull):** Mở tab `Source Control` (Ctrl+Shift+G). Bấm vào dấu `...` ở góc trên ➡️ Chọn `Pull`.
2. **Làm việc:** Viết code, lưu file. Các file bị sửa sẽ hiện chữ `M` (Modified).
3. **Lưu nháp (Commit):** Rê chuột vào chữ `Changes`, bấm dấu cộng `+` (Stage). Gõ nội dung công việc vào ô **Message** ➡️ Bấm nút **Commit**.
4. **Đẩy lên (Push):** Bấm vào nút **Sync Changes** màu xanh to đùng.

### 🔴 Cách 3: Dành cho hệ "Pro" - Dùng Dòng lệnh (Git CLI)
Mở Terminal trên máy hoặc ngay trong VS Code (`Ctrl + ~`).

**(Lần đầu tiên) Tải dự án & Chọn nhánh:**
```bash
git clone https://github.com/phantranthehuy/mpx5050-blood-pressure-monitor.git
cd mpx5050-blood-pressure-monitor
git checkout fw-tung  # Thay bằng tên nhánh của bạn
```

**Vòng lặp làm việc hàng ngày:**
```bash
# 1. Kéo code mới về
git pull origin fw-tung  

# 2. Làm việc xong, lưu file lại. Để kiểm tra những file đã sửa gõ: git status

# 3. Gom file và Ghi nhận (Commit)
git add .
git commit -m "Thêm hàm đọc ADC từ cảm biến MPX5050"

# 4. Đẩy lên mạng
git push origin fw-tung
```

---

## 🤝 Bước 3: Gộp bài vào nhánh chính (Pull Request)
Chỉ thực hiện khi bạn đã làm xong một chức năng lớn/hoàn thiện và muốn đưa vào bản chính thức (`main`):

1. Lên trang web GitHub của dự án.
2. Bạn sẽ thấy một thanh thông báo màu xanh báo hiệu bạn vừa Push, bấm nút **Compare & pull request**.
3. Viết vài dòng mô tả chi tiết bạn vừa làm gì.
4. Bấm **Create pull request**.
5. Nhắn tin cho **Huy (Quản lý)** vào duyệt. Nếu mọi thứ OK, Huy sẽ bấm **Merge** để gộp nhánh của bạn vào `main`.

---

> **💡 Lưu ý nhỏ (Đặc biệt với Cách 3):** Lệnh `git add .` rất tiện nhưng sẽ gom tất cả rác vào dự án nếu file `.gitignore` thiết lập chưa chuẩn. Hãy đảm bảo file `.gitignore` đã được đẩy lên nhánh `main` trước khi mọi người tải dự án về nhé!

**🚀 Chúc anh em code không bug, mạch không cháy nổ!**
