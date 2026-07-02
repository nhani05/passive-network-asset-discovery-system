# Phần 13: Tham Số CLI, Config Và Giá Trị Mặc Định

Tài liệu này mô tả các tham số dòng lệnh hiện có của `asset-discovery`, các giá trị mặc định, cách dùng `--config`/`--profile`, và các cờ cũ đã bị loại bỏ.

---

## 1. Nguyên Tắc Thiết Kế CLI Hiện Tại

CLI chỉ nên dùng cho những thứ người vận hành cần thấy rõ ở mỗi lần chạy:

```text
--pcap hoặc --interface
--config hoặc --profile
--filter
--capture-backend
--output
--version
--help
```

Các policy ít thay đổi như event tuning, local network, ignore network nên đặt trong file YAML dưới `configs/`.

Database URL, password, và đường dẫn event NDJSON đi qua `.env` hoặc biến môi trường, không truyền qua CLI.

---

## 2. Cú Pháp Chạy Chính

### PCAP offline

```bash
./build/asset-discovery --pcap samples/arp.pcap
```

Với profile:

```bash
./build/asset-discovery --profile pcap --pcap samples/multi-asset.pcap
```

Override nhanh output:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output json
```

### Live capture

```bash
sudo ./build/asset-discovery --interface eth0
```

Với profile:

```bash
sudo ./build/asset-discovery --profile live --interface eth0
```

Override nhanh filter hoặc backend:

```bash
sudo ./build/asset-discovery --interface eth0 --filter "arp" --capture-backend af-packet
```

Live capture chạy tới khi nhận `SIGINT`/`SIGTERM` hoặc lỗi runtime nghiêm trọng. Không còn `--duration`, `--live`, `--idle-timeout`, hoặc `--max-assets`.

---

## 3. Thứ Tự Ưu Tiên Cấu Hình

Runtime config được merge theo thứ tự sau:

```text
Built-in defaults trong code
  -> configs/default.yaml nếu tồn tại trong working directory
  -> --config <file> hoặc --profile <name>
  -> .env/process environment
  -> CLI arguments
```

CLI có quyền override cao nhất. Ví dụ `configs/default.yaml` đặt:

```yaml
capture:
  filter: "arp or udp port 67 or udp port 68"
```

Nhưng lệnh này vẫn dùng filter `arp`:

```bash
./build/asset-discovery --pcap samples/arp.pcap --filter "arp"
```

---

## 4. Tham Số CLI Common

| Tham số | Giá trị | Ý nghĩa | Mặc định |
| --- | --- | --- | --- |
| `--pcap <file>` | Đường dẫn file PCAP | Chọn chế độ đọc PCAP offline | Không có, phải chọn đúng một nguồn input |
| `--interface <name>` | Tên interface, ví dụ `eth0` | Chọn chế độ live capture | Không có, phải chọn đúng một nguồn input |
| `--config <file>` | Đường dẫn YAML | Load policy/runtime defaults từ file cụ thể | Không có |
| `--profile <name>` | Tên profile | Load `configs/<name>.yaml` | Không có |
| `--filter <bpf>` | Biểu thức BPF | Lọc packet trước khi parse | Built-in: không lọc; `configs/default.yaml`: `arp or udp port 67 or udp port 68` |
| `--capture-backend <name>` | `auto`, `pcap`, `af-packet` | Chọn live backend | `auto` |
| `--output <format>` | `table`, `json`, `csv` | Format summary cuối | Built-in/default config: `json`; `configs/pcap.yaml`: `json` |
| `--version` | Không có value | In version và thoát | `asset-discovery 0.1.0` |
| `-h`, `--help` | Không có value | In help text và thoát | Không áp dụng |

`--config` và `--profile` không được dùng cùng nhau.

---

## 5. Advanced CLI Overrides

Các tham số này vẫn được hỗ trợ để test/debug nhanh, nhưng nên cấu hình trong YAML khi chạy demo hoặc production.

| Tham số | Giá trị | Ý nghĩa | Built-in default | `configs/default.yaml` |
| --- | --- | --- | --- | --- |
| `--event-rate-limit <sec>` | Số nguyên dương | Suppress duplicate low-value events trong cửa sổ thời gian này | `60` | `60` |
| `--event-queue-capacity <n>` | Số nguyên dương | Dung lượng event queue trong live pipeline | `1024` | `1024` |
| `--flip-flop-window <sec>` | Số nguyên dương | Cửa sổ phát hiện IP/MAC flip-flop | `300` | `300` |
| `--reappearance-threshold <sec>` | Số nguyên dương | Ngưỡng `asset_reappeared` | `15552000` | `15552000` |
| `--local-net <cidr>` | IPv4 CIDR, lặp lại được | Mạng local cho event `non_local_source_ip` | Trống | Trống |
| `--ignore-net <cidr>` | IPv4 CIDR, lặp lại được | Mạng bỏ qua khi xét non-local source | Trống | `127.0.0.0/8`, `169.254.0.0/16` |

Nếu truyền `--local-net` hoặc `--ignore-net` trên CLI, danh sách CLI sẽ override danh sách từ config file.

---

## 6. Schema YAML Được Hỗ Trợ

Ví dụ `configs/default.yaml`:

```yaml
capture:
  filter: "arp or udp port 67 or udp port 68"
  backend: auto

output:
  format: json

events:
  rate_limit_sec: 60
  queue_capacity: 1024
  flip_flop_window_sec: 300
  reappearance_threshold_sec: 15552000

network:
  local_nets: []
  ignore_nets:
    - "127.0.0.0/8"
    - "169.254.0.0/16"
```

YAML subset hiện hỗ trợ:

- top-level sections: `capture`, `output`, `events`, `network`
- scalar string/integer
- list string cho `network.local_nets` và `network.ignore_nets`
- comment bằng `#`
- quote đơn hoặc quote kép cho scalar/list value

Không hỗ trợ:

- tab indentation
- nested object ngoài schema trên
- unknown key
- YAML anchors/aliases
- inline map/list phức tạp

---

## 7. Những Giá Trị Không Được Đặt Trong YAML/CLI

Các giá trị nhạy cảm hoặc phụ thuộc môi trường phải đi qua `.env` hoặc process environment:

| Giá trị | Cách cấu hình |
| --- | --- |
| PostgreSQL connection URL | `DATABASE_URL` |
| PostgreSQL host/port/database/user/password | `PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER`, `PGPASSWORD`, `PGSERVICE` |
| PostgreSQL alias | `DB_HOST`, `DB_PORT`, `DB_NAME`, `DB_USER`, `DB_PASSWORD` |
| Event NDJSON path | `ASSET_DISCOVERY_EVENTS_JSON` |

Ví dụ `.env`:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
ASSET_DISCOVERY_EVENTS_JSON=logs/events.ndjson
```

Runtime hiện yêu cầu PostgreSQL configuration trước khi capture. Nếu thiếu `.env`/env DB, chương trình sẽ dừng trước khi đọc PCAP hoặc mở interface.

---

## 8. Các Cờ Đã Bị Loại Bỏ

Các cờ sau bị reject với migration message rõ ràng:

| Cờ cũ | Lý do |
| --- | --- |
| `--duration` | Live capture hiện chạy tới khi bị interrupt |
| `--live` | `--interface <name>` đã đủ để chọn live capture |
| `--idle-timeout` | Live capture không tự dừng do idle |
| `--max-assets` | Live capture không tự dừng theo số asset |
| `--db-url` | Secret phải đi qua `.env`/environment |
| `--events` | Stdout events bật mặc định |
| `--events-json` | NDJSON path dùng `ASSET_DISCOVERY_EVENTS_JSON` |
| `--syslog` | Syslog auto-enabled khi platform hỗ trợ |
| `--events-db` | DB event writes bật qua database environment |

---

## 9. Lệnh Kiểm Tra Nhanh

```bash
./build/asset-discovery --help
./build/asset-discovery --version
./build/asset-discovery --profile pcap --pcap samples/arp.pcap
sudo ./build/asset-discovery --profile live --interface eth0
```
