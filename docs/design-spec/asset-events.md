# Asset Event Logging

Tài liệu này mô tả lớp event hiện tại của pipeline asset discovery.

## Flow

```text
ObservationBatch
  -> AssetMonitor
       -> AssetStore
       -> EventDispatcher
            -> stdout
       -> GUI asset/log callbacks
```

Event/log hiện chỉ là tín hiệu realtime cho asset mới. Database không ghi event history, không tạo bảng `asset_events`, và không xuất `events.json`/NDJSON/syslog.

## Event Types

| Type | Severity | Meaning |
| --- | --- | --- |
| `new_asset` | `info` | A MAC address is observed for the first time. |

Các event cũ như IP/MAC change, flip-flop, asset reappeared, hostname changed, non-local source IP đã bị loại bỏ. Khi asset đã tồn tại xuất hiện lại, hệ thống cập nhật `last_seen` trong bảng `assets`.

## Console Format

Mỗi event asset mới được in dạng human-readable ra stdout, ví dụ:

```text
[EVENT] ts=1.000000 severity=INFO type=new_asset ip=192.168.1.12 mac=aa:bb:cc:dd:ee:ff iface=eth0 protocol=arp message="New asset discovered"
```

## Database Persistence

SQLite là storage runtime hiện tại. Database chỉ lưu asset inventory:

- `mac_address`
- `ip_addresses`
- `hostname`
- `first_seen`
- `last_seen`
- `discovery_sources`
- metadata asset

Không có bảng event log. Migration SQLite chủ động drop bảng `asset_events` nếu gặp database cũ.

## CLI and Environment Configuration

Không còn cấu hình event output. Các biến/cờ sau đã bị loại bỏ:

- `ASSET_DISCOVERY_EVENTS_JSON`
- `--events`
- `--events-json`
- `--syslog`
- `--events-db`
- `--event-rate-limit`
- `--event-queue-capacity`
- `--flip-flop-window`
- `--reappearance-threshold`
- `--local-net`
- `--ignore-net`

SQLite path được cấu hình bằng `--sqlite <file>` hoặc `SQLITE_DATABASE_PATH`.
