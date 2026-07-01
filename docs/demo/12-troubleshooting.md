# Phần 12: Hướng Dẫn Giải Quyết Sự Cố (Troubleshooting)

Tài liệu này tổng hợp các lỗi phổ biến thường gặp trong quá trình cài đặt, biên dịch hoặc chạy demo và cung cấp các bước xử lý khắc phục tương ứng.

---

## 1. Lỗi Biên Dịch (Compilation & Configuration)

### 1.1. Thiếu thư viện `libpcap` ở bước cấu hình CMake
* **Triệu chứng:** Khi chạy `cmake` với tùy chọn `DASSET_DISCOVERY_REQUIRE_PCAP=ON`, quá trình cấu hình báo lỗi dừng:
  ```text
  libpcap was not found. Install the libpcap development package or set ASSET_DISCOVERY_REQUIRE_PCAP=OFF.
  ```
* **Giải pháp khắc phục:**
  Cài đặt gói phát triển pcap trên máy Ubuntu/Debian:
  ```bash
  sudo apt-get update
  sudo apt-get install -y libpcap-dev

  # Chạy lại cấu hình CMake
  cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
  ```

### 1.2. Trình khách `psql` không có trong đường dẫn PATH
* **Triệu chứng:** Chương trình ghi PostgreSQL báo lỗi không gọi được lệnh `psql`.
* **Giải pháp khắc phục:** Cài đặt package `postgresql-client`:
  ```bash
  sudo apt-get install -y postgresql-client
  ```
  *(Lưu ý: Trong Docker image chính thức của ứng dụng, gói này đã được cài đặt sẵn).*

---

## 2. Lỗi Quyền Hạn Bắt Gói Tin (Live Capture Permissions)

### 2.1. Lỗi không đủ quyền mở socket
* **Triệu chứng:** Chạy capture live báo lỗi thiếu quyền raw socket hoặc không khởi động được backend capture.
* **Giải pháp khắc phục:**
  * **Native Linux:** Bắt buộc chạy lệnh với tiền tố `sudo`:
    ```bash
    sudo ./build/asset-discovery --interface eth0
    ```
    Hoặc cấp quyền cho file thực thi để chạy không cần `sudo`:
    ```bash
    sudo setcap cap_net_raw,cap_net_admin=eip ./build/asset-discovery
    ```
  * **Docker Container:** Đảm bảo đã truyền đủ các tham số đặc quyền:
    `--user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW`

---

## 3. Lỗi Cơ Sở Dữ Liệu (PostgreSQL Connectivity)

### 3.1. Lỗi không kết nối được database
* **Triệu chứng:** Chương trình báo lỗi kết nối hoặc treo ở bước kiểm tra database.
* **Giải pháp khắc phục:**
  * Đảm bảo container database đang ở trạng thái chạy khỏe mạnh: `docker compose ps` (Service `db` phải hiển thị `running (healthy)`).
  * Kiểm tra xem cổng kết nối PostgreSQL (mặc định `5432`) có bị ứng dụng khác chiếm dụng trên host hay không.
  * Kiểm tra các giá trị trong `.env` hoặc biến môi trường `DATABASE_URL`.

### 3.2. Cổng `5432` đã bị ứng dụng PostgreSQL khác của Host chiếm giữ
* **Giải pháp khắc phục:** Cấu hình Docker Compose export sang cổng khác của Host (ví dụ `5433`):
  ```bash
  # Khởi chạy db với cổng host tùy biến
  POSTGRES_HOST_PORT=5433 docker compose up -d db
  ```
  Sau đó cập nhật cấu hình file `.env` của bạn: `PGPORT=5433`.

### 3.3. Muốn tắt hoàn toàn ghi DB nhưng chương trình vẫn tự động ghi
* **Giải pháp khắc phục:** Chương trình tự ghi DB khi thấy biến môi trường PostgreSQL. Hãy xóa biến bằng cách:
  ```bash
  env -u PGHOST -u PGPORT -u PGDATABASE -u PGUSER -u PGPASSWORD ./build/asset-discovery ...
  ```

---

## 4. Các Vấn Đề Khi Chạy Thực Tế (Runtime & Traffic)

### 4.1. Không phát hiện được thiết bị nào khi chạy Live Capture
* **Triệu chứng:** Summary in ra màn hình thông báo: `No assets discovered`.
* **Giải pháp khắc phục:**
  * Kiểm tra xem card mạng khai báo có đúng không bằng cách chạy `ip -br link`.
  * Có thể do mạng hiện tại không có traffic ARP hay DHCP. Hãy sinh traffic thủ công bằng cách chạy ping gateway hoặc arping ở một terminal khác (xem chi tiết ở Phần 7).
  * Thử bỏ cờ lọc `--filter` tạm thời để kiểm tra card mạng có nhận gói tin IP thông thường hay không.

### 4.2. Gói tin bị drop nhiều khi chạy Live Capture hiệu năng cao
* **Triệu chứng:** Đầu ra báo metrics `packets_dropped_queue_full` lớn hơn 0.
* **Giải pháp khắc phục:**
  * Chạy binary được build ở chế độ tối ưu **Release** (xem Phần 2).
  * Tăng dung lượng hàng đợi đệm của ứng dụng bằng cờ `--event-queue-capacity 5000` hoặc tăng số parser workers trong benchmark.
