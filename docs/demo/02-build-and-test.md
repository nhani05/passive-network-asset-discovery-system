# Phần 2: Build Native Và Chạy Test

Tài liệu này hướng dẫn cách biên dịch hệ thống trên máy host native với cấu hình PCAP bắt buộc và một thư mục build duy nhất, sau đó chạy bộ kiểm thử tự động (CTest).

---

## 1. Build Bắt Buộc libpcap

```bash
# Cấu hình dự án. CMake sẽ dừng nếu thiếu libpcap.
cmake -S . -B build

# Biên dịch mã nguồn
cmake --build build

# Chạy unit test để kiểm chứng
ctest --test-dir build --output-on-failure
```

* **Kỳ vọng:**
  * Cấu hình và biên dịch thành công mà không có cảnh báo hay lỗi liên quan đến thư viện pcap.
  * Lệnh `ctest` báo `100% tests passed`.
  * Binary `./build/asset-discovery` đọc được file PCAP thật.

---

## 2. Build GUI Và CLI Trong Cùng Thư Mục

```bash
cmake -S . -B build

cmake --build build
ctest --test-dir build --output-on-failure
```

* **Kỳ vọng:**
  * Vẫn chỉ dùng thư mục `build`.
  * Binary CLI `./build/asset-discovery` và GUI `./build/asset-discovery-gui` đều được tạo.

---

## 3. Kiểm Tra Số Lượng Test Cases

Bạn có thể kiểm tra nhanh tổng số lượng test và trạng thái chạy bằng lệnh sau:

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```

* **Kỳ vọng output:**
  ```text
  100% tests passed, 0 tests failed out of 31
  ```
  *(Số lượng test có thể thay đổi nhẹ tùy thuộc vào phiên bản codebase cụ thể).*

---

## 4. Chạy PCAP Với Build Hiện Tại

```bash
./build/asset-discovery --pcap path/to/user-traffic.pcap \
  --filter "arp or udp port 67 or udp port 68"
```
