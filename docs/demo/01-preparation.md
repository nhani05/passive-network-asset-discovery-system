# Phần 1: Chuẩn Bị Trước Demo

Tài liệu này hướng dẫn chuẩn bị môi trường, kiểm tra công cụ, cấu hình PostgreSQL và cấu trúc giao diện dòng lệnh (CLI) của hệ thống.

---

## 1. Yêu Cầu Môi Trường

Để thực hiện toàn bộ các kịch bản demo, môi trường của bạn cần hỗ trợ các công cụ sau:

| Công cụ | Yêu cầu tối thiểu | Vai trò trong Demo |
| :--- | :--- | :--- |
| **CMake** | 3.16 trở lên | Cấu hình và sinh build files cho dự án C++17 |
| **C++ Compiler** | Hỗ trợ C++17 (GCC 8+, Clang 7+, MSVC 2019+) | Biên dịch mã nguồn |
| **`libpcap-dev`** | Tùy chọn (Cần cho Live capture/PCAP) | Thư viện bắt gói tin mạng |
| **Docker** | Tùy chọn (Cần cho container demo) | Đóng gói và chạy ứng dụng trong môi trường cô lập |
| **Docker Compose** | Tùy chọn (Cần cho Compose stack) | Khởi chạy cụm dịch vụ App + PostgreSQL |
| **`psql`** | Tùy chọn | Client truy vấn kiểm tra dữ liệu trong PostgreSQL |
| **`pkg-config`** | Tùy chọn | Hỗ trợ CMake tìm thư viện `libpcap` |

---

## 2. Kiểm Tra Nhanh Công Cụ

Hãy chạy các lệnh sau ở terminal để đảm bảo các công cụ đã được cài đặt và sẵn sàng:

```bash
cmake --version
g++ --version          # hoặc clang++ --version
docker --version
docker compose version
psql --version
pkg-config --version
```

---

## 3. Kiểm Tra Interface Mạng (Cho Live Capture)

Để thực hiện live capture, bạn cần biết tên interface mạng đang hoạt động (ví dụ: `eth0`, `enp4s0`, `wlan0`):

```bash
# Liệt kê trạng thái rút gọn các interface mạng
ip -br link

# Kiểm tra bảng định tuyến để tìm interface kết nối Internet mặc định
ip route
```

> [!NOTE]
> Trong các tài liệu hướng dẫn tiếp theo, interface mặc định được giả định là `eth0`. Hãy thay thế `eth0` bằng tên interface thực tế trên máy của bạn khi chạy demo.

---

## 4. Lưu Ý Quan Trọng Về PostgreSQL

* **Tự động tải cấu hình:** Binary khi khởi động sẽ tự động quét và tải file cấu hình `.env` nằm trong thư mục hiện tại.
* **Tự động kích hoạt ghi DB:** Nếu phát hiện các cấu hình hợp lệ (qua `.env`, biến môi trường `DATABASE_URL` hoặc các biến `PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER`, `PGPASSWORD`), hệ thống sẽ **tự động** ghi nhận tài sản (assets) và lịch sử sự kiện (events) vào PostgreSQL.
* **Tự động tạo Schema:** Bảng `assets` và `asset_events` sẽ tự động được khởi tạo (qua `CREATE TABLE IF NOT EXISTS`), do đó bạn không cần chạy file SQL schema thủ công trước khi demo.
* **PostgreSQL configuration là bắt buộc:** Runtime hiện tại fail sớm nếu không có `.env`, `DATABASE_URL`, `PG*`, hoặc `DB_*`. Điều này đảm bảo demo luôn lưu được asset snapshot và event history.

---

## 5. Cấu Trúc CLI (Command Line Interface)

Hệ thống hỗ trợ hai chế độ bắt gói tin (chỉ được chọn duy nhất một nguồn):

```text
# Chế độ đọc từ file PCAP offline
asset-discovery --pcap <đường_dẫn_file_pcap> [--config <file>|--profile <name>] [--filter <biểu_thức_bpf>] [--output table|json|csv]

# Chế độ Live Capture trực tiếp từ Interface
asset-discovery --interface <tên_interface> [--config <file>|--profile <name>] [--filter <biểu_thức_bpf>] [--capture-backend auto|pcap|af-packet]
```

### Các tham số chi tiết:

| Tham số | Ý nghĩa | Mặc định |
| :--- | :--- | :--- |
| `--pcap <file>` | Đường dẫn file PCAP offline cần đọc | (Bắt buộc nếu chạy offline) |
| `--interface <name>` | Tên interface mạng để capture live | (Bắt buộc nếu chạy live) |
| `--config <file>` | Load policy/runtime defaults từ file YAML | Không bắt buộc |
| `--profile <name>` | Load `configs/<name>.yaml` | Không bắt buộc |
| `--filter <bpf>` | Bộ lọc gói tin BPF (Berkeley Packet Filter) | Không lọc |
| `--capture-backend <backend>` | Chọn backend live capture: `auto`, `pcap` hoặc `af-packet` | `auto` |
| `--output <format>` | Định dạng summary cuối khi kết thúc: `table`, `json`, `csv` | `json` |
| `--event-rate-limit <sec>` | Advanced override cho cửa sổ suppress event trùng/lặp | `60` |
| `--event-queue-capacity <n>` | Advanced override cho dung lượng hàng đợi event | `1024` |
| `--flip-flop-window <sec>` | Advanced override cho cửa sổ phát hiện IP/MAC flip-flop | `300` |
| `--reappearance-threshold <sec>`| Advanced override cho ngưỡng asset xuất hiện trở lại | `15552000` |
| `--local-net <cidr>` | CIDR khai báo local network cho event `non_local_source_ip` (có thể khai báo nhiều lần) | (Trống) |
| `--ignore-net <cidr>` | CIDR mạng cần bỏ qua khỏi check `non_local_source_ip` (có thể khai báo nhiều lần) | (Trống) |
| `--version` | Hiển thị version binary | |
| `-h`, `--help` | Hiển thị thông tin trợ giúp | |

Các tham số event/network tuning nên đặt trong `configs/default.yaml`, `configs/live.yaml`, hoặc file truyền qua `--config`. File YAML chỉ hỗ trợ schema policy/runtime, không hỗ trợ `capture.interface`, `capture.pcap`, `database.url`, hoặc lựa chọn event sink.
