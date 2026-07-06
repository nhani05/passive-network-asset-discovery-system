# Phần 4: Demo SQLite Local & Tích Hợp Hệ Thống

Runtime hiện dùng SQLite để lưu asset inventory; event/log không được ghi vào database.

## 1. Cấu Hình SQLite

Tạo `.env` ở thư mục gốc:

```env
SQLITE_DATABASE_PATH=pnad.db
```

Hoặc truyền trực tiếp trên CLI:

```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db
```

## 2. Chạy PCAP Và Ghi Asset

```bash
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --sqlite pnad.db \
  --filter "arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353" \
  --output table
```

Kỳ vọng:

- Terminal in event `new_asset` khi phát hiện MAC mới.
- Summary cuối hiển thị danh sách asset.
- File `pnad.db` được tạo.
- Database chỉ lưu asset inventory, không có event log.

## 3. Kiểm Chứng Không Còn Event Log

Nếu máy có `sqlite3` CLI:

```bash
sqlite3 pnad.db ".tables"
```

Kỳ vọng có các bảng như `assets`, `app_settings`, `analysis_sessions`; không có `asset_events`.

Kiểm tra asset:

```bash
sqlite3 pnad.db \
  "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

## 4. Kiểm Chứng Cờ Cũ Bị Từ Chối

```bash
./build/asset-discovery --pcap samples/arp.pcap --db-url old-db-url
```

Kỳ vọng:

```text
[CONFIG ERROR] --db-url has been removed; configure SQLite with --sqlite or SQLITE_DATABASE_PATH
```

```bash
./build/asset-discovery --pcap samples/arp.pcap --events-json old-event-path
```

Kỳ vọng:

```text
[CONFIG ERROR] --events-json has been removed; event file output is no longer supported
```

## 5. Kiểm Chứng Thiếu SQLite Configuration

```bash
env -u SQLITE_DATABASE_PATH ./build/asset-discovery --pcap samples/arp.pcap
```

Kỳ vọng:

```text
[CONFIG ERROR] SQLite configuration is required; set SQLITE_DATABASE_PATH or --sqlite
```
