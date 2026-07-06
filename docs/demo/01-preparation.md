# Phần 1: Chuẩn Bị Trước Demo

Tài liệu này hướng dẫn chuẩn bị môi trường cho demo PCAP offline, SQLite storage, và CLI hiện tại.

## 1. Yêu Cầu Môi Trường

| Công cụ | Yêu cầu tối thiểu | Vai trò |
| :--- | :--- | :--- |
| **CMake** | 3.16 trở lên | Cấu hình build C++17 |
| **C++ Compiler** | GCC 8+, Clang 7+, hoặc MSVC 2019+ | Biên dịch mã nguồn |
| **`pkg-config`** | Bắt buộc trên Linux | Hỗ trợ CMake tìm dependency |
| **`libpcap-dev`** | Bắt buộc | Đọc PCAP và biên dịch BPF filter |
| **SQLite3 dev package** | Bắt buộc | Build SQLite writer |
| **Qt/QML packages** | Bắt buộc cho GUI | Chạy desktop app |
| **Docker** | Tùy chọn | Chạy container demo |
| **Docker Compose** | Tùy chọn | Chạy `pcap-demo`/`assetd` |

Không cần PostgreSQL hoặc `psql`.

## 2. Kiểm Tra Nhanh Công Cụ

```bash
cmake --version
g++ --version          # hoặc clang++ --version
docker --version
docker compose version
pkg-config --version
```

## 3. Cấu Hình SQLite

Tạo `.env` nếu muốn đặt SQLite path mặc định:

```env
SQLITE_DATABASE_PATH=pnad.db
```

Hoặc truyền trực tiếp:

```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db
```

Database chỉ lưu asset inventory. Event asset mới chỉ in realtime ra terminal/giao diện.

## 4. Cấu Trúc CLI

CLI chính chỉ hỗ trợ đọc PCAP offline:

```text
asset-discovery --pcap <file> [--filter <bpf>] [--sqlite <file>] [--output table|json|csv]
```

| Tham số | Ý nghĩa | Mặc định |
| :--- | :--- | :--- |
| `--pcap <file>` | File PCAP/PCAPNG cần phân tích | Bắt buộc |
| `--filter <bpf>` | Bộ lọc BPF | `arp or udp port 67 or udp port 68` |
| `--sqlite <file>` | SQLite database path | `SQLITE_DATABASE_PATH` |
| `--output <format>` | `table`, `json`, hoặc `csv` | `json` |
| `--version` | Hiển thị version binary | |
| `-h`, `--help` | Hiển thị trợ giúp | |

Runtime config chỉ còn `configs/default.yaml` và được nạp tự động nếu tồn tại. YAML hiện chỉ hỗ trợ `output.format`.

## 5. Các Thành Phần Đã Bỏ

Không còn dùng:

- PostgreSQL, `DATABASE_URL`, `PG*`, `DB_*`, `db/schema.sql`
- `ASSET_DISCOVERY_EVENTS_JSON`, `events.json`, NDJSON event output, syslog event output
- `--event-rate-limit`, `--event-queue-capacity`, `--flip-flop-window`, `--reappearance-threshold`, `--local-net`, `--ignore-net`
- `--interface`, `--capture-backend`, `--config`, `--profile` trên CLI chính
