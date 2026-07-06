# Phần 13: Tham Số CLI Và Config Mặc Định

`asset-discovery` hiện chỉ chạy phân tích PCAP offline. Không còn live capture, profile config, PostgreSQL, event file output, hoặc chọn capture backend trên CLI.

## 1. Cú Pháp

```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db
./build/asset-discovery --pcap samples/multi-asset.pcap --sqlite pnad.db --output table
./build/asset-discovery --pcap samples/multi-asset.pcap --sqlite pnad.db --filter "arp"
```

## 2. Tham Số Còn Dùng

| Tham số | Ý nghĩa | Mặc định |
| :--- | :--- | :--- |
| `--pcap <file>` | File PCAP/PCAPNG cần phân tích | Bắt buộc |
| `--filter <bpf>` | Ghi đè nhanh BPF filter | `arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353` |
| `--broad-ipv4-enrichment` | Dùng filter rộng hơn để lấy TTL OS hint từ IPv4 traffic khi không truyền `--filter` | Tắt |
| `--sqlite <file>` | Đường dẫn SQLite local | `SQLITE_DATABASE_PATH` |
| `--output <format>` | `table`, `json`, hoặc `csv` | `json` |
| `--version` | In version binary | |
| `-h`, `--help` | In help text | |

Runtime cần SQLite path qua `--sqlite`, `SQLITE_DATABASE_PATH`, hoặc `.env`.

## 3. Config YAML

Chỉ còn file config mặc định:

```text
configs/default.yaml
```

File này được nạp tự động nếu tồn tại. YAML không khai báo capture source, capture backend, BPF filter, database URL, event policy, hoặc network policy.

Ví dụ:

```yaml
output:
  format: json
```

Chỉ còn hỗ trợ `output.format`.

## 4. Environment Còn Dùng

```env
SQLITE_DATABASE_PATH=pnad.db
```

Không còn dùng `DATABASE_URL`, `PG*`, `DB_*`, hoặc `ASSET_DISCOVERY_EVENTS_JSON`.

## 5. Cờ Đã Loại Bỏ

Các cờ sau bị reject với migration message rõ ràng:

```text
--config
--profile
--interface
--capture-backend
--live
--duration
--idle-timeout
--max-assets
--db-url
--events
--events-json
--syslog
--events-db
--event-rate-limit
--event-queue-capacity
--flip-flop-window
--reappearance-threshold
--local-net
--ignore-net
```

Lý do chính: database chỉ lưu asset inventory; event asset mới chỉ in realtime ra terminal/giao diện. Những lần asset cũ xuất hiện lại được phản ánh qua `last_seen` trong SQLite.
