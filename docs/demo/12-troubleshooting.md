# Phần 12: Hướng Dẫn Giải Quyết Sự Cố (Troubleshooting)

Tài liệu này tổng hợp các lỗi phổ biến thường gặp trong quá trình cài đặt, biên dịch hoặc chạy demo và cung cấp các bước xử lý khắc phục tương ứng.

---

## 1. Lỗi Biên Dịch (Compilation & Configuration)

### 1.1. Thiếu thư viện `libpcap` ở bước cấu hình CMake
* **Triệu chứng:** Khi chạy `cmake`, quá trình cấu hình báo lỗi dừng:
  ```text
  libpcap was not found. Install the libpcap development package before building PNAD.
  ```
* **Giải pháp khắc phục:**
  Cài đặt gói phát triển pcap trên máy Ubuntu/Debian:
  ```bash
  sudo apt-get update
  sudo apt-get install -y libpcap-dev

  # Chạy lại cấu hình CMake
  cmake -S . -B build
  ```

### 1.2. Thiếu thư viện SQLite development khi build
* **Triệu chứng:** CMake hoặc linker báo thiếu SQLite.
* **Giải pháp khắc phục:** Cài đặt package SQLite development:
  ```bash
  sudo apt-get install -y libsqlite3-dev
  ```

---

## 2. Lỗi Đọc File PCAP

### 2.1. File PCAP không tồn tại hoặc không đọc được
* **Triệu chứng:** Chương trình báo không mở được file PCAP.
* **Giải pháp khắc phục:**
  * Kiểm tra lại đường dẫn truyền qua `--pcap`.
  * Đảm bảo file có đuôi `.pcap` hoặc `.pcapng` và user hiện tại có quyền đọc file.
  * **Docker Container:** Đảm bảo mount đúng thư mục chứa PCAP, ví dụ `-v "$PWD/samples:/samples:ro"`.

---

## 3. Lỗi Cơ Sở Dữ Liệu SQLite

### 3.1. Thiếu SQLite path
* **Triệu chứng:** Chương trình dừng trước khi đọc PCAP:
  ```text
  [CONFIG ERROR] SQLite configuration is required; set SQLITE_DATABASE_PATH or --sqlite
  ```
* **Giải pháp khắc phục:**
  ```bash
  ./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db
  ```
  Hoặc tạo `.env`:
  ```env
  SQLITE_DATABASE_PATH=pnad.db
  ```

### 3.2. SQLite path không ghi được
* **Triệu chứng:** Chương trình báo lỗi mở database hoặc set WAL mode.
  ```text
  [DATABASE ERROR] Failed to initialize SQLite database: ...
  ```
* **Giải pháp khắc phục:**
  * Chọn thư mục user hiện tại có quyền ghi.
  * Trong Docker, mount thư mục writable:
  ```bash
  mkdir -p .docker-data
  chmod 777 .docker-data
  docker run --rm \
    -v "$PWD/samples:/samples:ro" \
    -v "$PWD/.docker-data:/data" \
    -e SQLITE_DATABASE_PATH=/data/pnad.db \
    passive-asset-discovery \
    --pcap /samples/arp.pcap
  ```

---

## 4. Các Vấn Đề Khi Chạy Thực Tế (Runtime & Traffic)

### 4.1. Không phát hiện được thiết bị nào khi đọc PCAP
* **Triệu chứng:** Summary in ra màn hình thông báo: `No assets discovered`.
* **Giải pháp khắc phục:**
  * Kiểm tra file PCAP có chứa ARP, DHCP, SSDP hoặc mDNS không.
  * Thử bỏ cờ lọc `--filter` tạm thời để kiểm tra dữ liệu trong file.

### 4.2. PCAP lớn làm xử lý chậm
* **Triệu chứng:** Ứng dụng xử lý PCAP lớn chậm hoặc GUI phản hồi chậm.
* **Giải pháp khắc phục:**
  * Chạy binary được build ở chế độ tối ưu **Release** (xem Phần 2).
  * Giảm độ rộng filter hoặc chia nhỏ PCAP để khoanh vùng bottleneck.
  * Chỉ bật `--broad-ipv4-enrichment` khi cần TTL OS hint; mode này bắt nhiều IPv4 traffic hơn filter mặc định.
