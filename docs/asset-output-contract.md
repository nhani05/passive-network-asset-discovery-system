# Contract Output Asset

Tài liệu này định nghĩa các field asset dùng chung cho output table, JSON, CSV, và PostgreSQL.

## Field Asset

| Field | Ý nghĩa |
|---|---|
| `mac_address` | Địa chỉ MAC đã chuẩn hóa chữ thường, dùng làm khóa chính của asset. |
| `ip_addresses` | Tập địa chỉ IPv4 đã quan sát được cho asset, theo thứ tự ổn định. |
| `hostname` | Hostname tùy chọn, hiện được lấy từ DHCP option 12 khi có. |
| `first_seen` | Timestamp packet sớm nhất đã quan sát cho asset, định dạng `seconds.microseconds`. |
| `last_seen` | Timestamp packet mới nhất đã quan sát cho asset, định dạng `seconds.microseconds`. |
| `discovery_sources` | Tập source protocol ổn định, ví dụ `arp` và `dhcp`. |

## Quy Tắc Gộp

- Asset được định danh bằng địa chỉ MAC.
- Observation lặp lại cho cùng MAC cập nhật `last_seen`.
- Observation cũ hơn đến không theo thứ tự có thể cập nhật `first_seen`.
- Địa chỉ IP mới được thêm vào `ip_addresses`.
- Hostname DHCP không rỗng cập nhật `hostname`.
- Source protocol được tích lũy trong `discovery_sources`.

## JSON

Output JSON là một mảng object asset:

```json
[
  {
    "mac_address": "02:42:ac:11:00:03",
    "ip_addresses": ["192.168.1.20"],
    "hostname": "laptop-user",
    "first_seen": "1699606800.0",
    "last_seen": "1699606800.0",
    "discovery_sources": ["dhcp"]
  }
]
```

`hostname` được bỏ qua khi chưa biết hostname.

## CSV

Output CSV luôn có header sau:

```csv
mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources
```

Field có nhiều giá trị dùng `;` bên trong field CSV. Escape CSV dùng quy tắc quote chuẩn cho dấu phẩy, dấu quote, và ký tự xuống dòng.

## PostgreSQL

Schema chuẩn nằm trong `db/schema.sql`:

```sql
CREATE TABLE IF NOT EXISTS assets (
    mac_address TEXT PRIMARY KEY,
    ip_addresses TEXT[] NOT NULL DEFAULT '{}',
    hostname TEXT,
    first_seen TEXT NOT NULL,
    last_seen TEXT NOT NULL,
    discovery_sources TEXT[] NOT NULL DEFAULT '{}',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

Phần triển khai hiện tại ghi PostgreSQL qua `psql` bằng `--db-url <connection-string>`, `DATABASE_URL`, hoặc các biến môi trường chuẩn của `psql` (`PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER`, `PGPASSWORD`) được load từ file `.env` trong thư mục hiện tại.
