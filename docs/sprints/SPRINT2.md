# Sprint 2: Metadata Asset, Export, Và SQLite

Sprint 2 mở rộng MVP đọc PCAP offline với metadata asset có cấu trúc, output máy đọc được, bổ sung thông tin từ DHCP, export CSV, và lưu SQLite.

## Phạm Vi

| Issue | Trạng thái | Kết quả |
|---|---|---|
| #21 | Hoàn tất | CLI contract hiện còn PCAP offline, `--filter`, `--sqlite`, và output `table/json/csv`; các cờ live capture và `--db-url` cũ đã bị loại bỏ. |
| #11 | Hoàn tất | `--output json` in JSON ổn định. |
| #13 | Hoàn tất | Decode IPv4 và UDP hỗ trợ nhận diện DHCP. |
| #12 | Hoàn tất | Metadata DHCP tạo observation cho asset gồm client MAC, IP được yêu cầu, hostname, và source. |
| #22 | Hoàn tất | Fixture DHCP PCAP ổn định được sinh trong test và kiểm chứng qua output JSON của CLI. |
| #23 | Hoàn tất | Packet bị cắt ngắn hoặc chưa hỗ trợ được parser bỏ qua an toàn. |
| #38 | Hoàn tất | Asset inventory được ghi vào SQLite qua `--sqlite` hoặc `SQLITE_DATABASE_PATH`. |
| #36 | Hoàn tất | `--output csv` in CSV ổn định. |
| #14 | Hoàn tất | Contract asset/output được ghi tại `docs/asset-output-contract.md`. |

## Lệnh Đã Kiểm Chứng

Từ thư mục gốc repository:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Kết quả đã kiểm chứng:

```text
100% tests passed, 0 tests failed out of 17
```

## Ví Dụ Output

Fixture ARP ở dạng CSV:

```sh
./build/asset-discovery --pcap samples/arp.pcap --output csv
```

Hình dạng mong đợi:

```csv
mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources
02:42:ac:11:00:02,192.168.1.10,,1699606784.0,1699606784.0,arp
```

Fixture DHCP được sinh ở dạng JSON:

```sh
./build/asset-discovery --pcap build/tests/dhcp.pcap --output json
```

Hình dạng mong đợi:

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

## SQLite

Chạy discovery và lưu asset:

```sh
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --output json
```

`.env` có thể khai báo SQLite path:

```env
SQLITE_DATABASE_PATH=pnad.db
```

Truy vấn asset đã lưu:

```sh
sqlite3 pnad.db \
  "SELECT mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources FROM assets;"
```

## Ghi Chú

- DHCP test PCAP được sinh bởi `tests/GenerateDhcpPcap.py`; fixture này ổn định và đủ nhỏ cho CI.
- Parser bỏ qua packet chưa hỗ trợ hoặc sai định dạng thay vì crash.
- Binary load `.env` từ thư mục hiện tại trước khi resolve cấu hình database.
