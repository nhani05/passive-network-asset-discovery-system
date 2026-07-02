# Phần 2: Build Native Và Chạy Test

Tài liệu này hướng dẫn cách biên dịch (build) hệ thống trên máy host native dưới các cấu hình khác nhau, chạy bộ kiểm thử tự động (CTest) và đo hiệu năng xử lý (concurrency benchmark).

---

## 1. Build Không Bắt Buộc libpcap (Cho máy CI/Dev thiếu thư viện)

Cấu hình này hữu ích khi biên dịch trên các container CI hoặc môi trường dev tối giản không có sẵn `libpcap`. Ở chế độ này, bạn vẫn chạy được các test case logic nhưng không thực hiện được đọc file PCAP thật hay capture live.

```bash
# Cấu hình dự án với REQUIRE_PCAP=OFF
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF

# Biên dịch mã nguồn
cmake --build build

# Chạy unit test để kiểm chứng
ctest --test-dir build --output-on-failure
```

* **Kỳ vọng:**
  * Quá trình build hoàn tất. Có thể xuất hiện cảnh báo `libpcap was not found` trong log cấu hình nhưng không gây lỗi biên dịch.
  * Lệnh `ctest` báo `100% tests passed`.
  * Binary `./build/asset-discovery` được tạo ra nhưng khi chạy capture sẽ báo `backend is not available`.

---

## 2. Build Bắt Buộc libpcap (Cho máy Demo đầy đủ tính năng)

Đây là cấu hình chuẩn để chạy thử nghiệm toàn bộ hệ thống (đọc file PCAP, capture live trên interface).

```bash
# Cấu hình dự án yêu cầu bắt buộc libpcap
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON

# Biên dịch
cmake --build build

# Chạy unit test
ctest --test-dir build --output-on-failure
```

* **Kỳ vọng:**
  * Cấu hình và biên dịch thành công mà không có cảnh báo hay lỗi liên quan đến thư viện pcap.
  * `ctest` chạy thành công toàn bộ test cases.
  * *Lưu ý:* Nếu bị lỗi thiếu thư viện ở bước cấu hình, vui lòng cài đặt gói phát triển pcap trước (xem phần Troubleshooting).

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

## 4. Benchmark Concurrency & Backend Comparison

Để đo đạc thông lượng (throughput) tối đa mà pipeline xử lý đa luồng (concurrent pipeline) có thể đạt được, chúng ta nên biên dịch phiên bản tối ưu **Release**:

```bash
# Tạo thư mục build tối ưu Release
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DASSET_DISCOVERY_REQUIRE_PCAP=ON

# Biên dịch
cmake --build build-release
```

### 4.1. Benchmark Offline (Không cần quyền root)

Chạy ép hiệu năng (stress-test) offline bằng cách sử dụng luồng gói tin có sẵn từ một file PCAP của bạn:

```bash
# Lệnh chạy benchmark cơ bản
./build-release/asset-discovery-live-benchmark path/to/user-traffic.pcap

# Khai báo số luồng parser worker, kích thước batch và bộ lọc BPF
./build-release/asset-discovery-live-benchmark --pcap path/to/user-traffic.pcap \
  --workers 7 --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"
```

* Positional parameters tương thích ngược: `./build-release/asset-discovery-live-benchmark <pcap_path> [worker_count] [batch_size] [filter]`.

### 4.2. Benchmark Live Backend (Cần quyền root / CAP_NET_RAW trên Linux)

Chạy so sánh trực tiếp hiệu năng giữa hai backend capture `pcap` (libpcap truyền thống) và `af-packet` (Linux AF_PACKET/TPACKET_V3):

> [!NOTE]
> Các lệnh dưới đây dùng binary benchmark riêng `asset-discovery-live-benchmark`, không phải CLI chính `asset-discovery`. Vì vậy benchmark vẫn có `--duration` và `--backend` để giới hạn thời gian đo hiệu năng.

```bash
# Benchmark với backend libpcap
sudo ./build-release/asset-discovery-live-benchmark \
  --interface eth0 --duration 30 --backend pcap \
  --workers 7 --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"

# Benchmark với backend AF_PACKET
sudo ./build-release/asset-discovery-live-benchmark \
  --interface eth0 --duration 30 --backend af-packet \
  --workers 7 --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"
```

* **Kỳ vọng kết quả:**
  Đầu ra sẽ hiển thị các tham số môi trường cùng metrics chi tiết của phiên chạy:
  ```text
  benchmark=live-backend-comparison
  traffic_source=live-interface
  interface=eth0
  duration_seconds=30
  live_capture_metrics elapsed_seconds=30 packets_captured=... packets_enqueued=... packets_dropped_queue_full=0 packets_parsed=... packet_throughput_per_second=... backend_requested=af-packet backend_selected=af-packet backend_packets_copied=... backend_kernel_drops=0 packet_batches_dropped=0
  ```

> [!TIP]
> Mục tiêu thiết kế của pipeline là có khả năng xử lý tới **1,000,000 packets/giây** trong môi trường Release với cấu hình luồng phù hợp. Kết quả thực tế phụ thuộc nhiều vào cấu hình phần cứng CPU, card mạng (NIC), hệ điều hành và độ phức tạp của bộ lọc BPF.
