# Sprint 3: Docker PCAP Và SQLite

Sprint 3 đưa luồng PCAP offline vào môi trường container có thể lặp lại. Trạng thái hiện tại đã thu gọn main CLI về phân tích PCAP, lưu SQLite, in event asset mới ra terminal, và không còn live capture trên `asset-discovery`.

## Mục Tiêu Sau Cleanup

- Dùng cùng pipeline parser/asset/output cho file PCAP và demo Docker.
- Giới hạn traffic đầu vào bằng biểu thức BPF trước khi parser xử lý packet.
- Đóng gói binary C++17 trong Docker image có libpcap, SQLite runtime, Qt runtime cho GUI, và backend service.
- Cung cấp Compose stack lặp lại được gồm `pcap-demo`, `assetd`, và SQLite volumes.
- Loại bỏ PostgreSQL, event file output, syslog event output, và các tham số event policy cũ.

## Phạm Vi

| Hạng mục | Trạng thái | Ghi chú |
| --- | --- | --- |
| PCAP offline | Hoàn tất | CLI chính hỗ trợ `--pcap <file>` và không còn nhận `--interface`/`--duration`. |
| BPF filter | Hoàn tất | CLI hỗ trợ `--filter <bpf>` cho PCAP. |
| SQLite | Hoàn tất | Asset inventory ghi qua `--sqlite <file>` hoặc `SQLITE_DATABASE_PATH`. |
| Dockerfile | Hoàn tất | Image multi-stage build binary C++17 với libpcap, SQLite, Qt runtime, `asset-discovery`, `asset-discovery-gui`, `assetd`, và `asset-capture`. |
| Docker Compose | Hoàn tất | `pcap-demo` chạy sample PCAP và ghi SQLite; `assetd` chạy ở profile `service`. |
| Event/log | Hoàn tất | Chỉ in `new_asset` realtime ra terminal/UI; không ghi event log ra DB/file/syslog. |

## CLI Hiện Tại

`asset-discovery` hiện chỉ nhận PCAP offline:

```text
asset-discovery --pcap <file> [--filter <bpf>] [--sqlite <file>] [--output table|json|csv]
asset-discovery --version
```

Quy tắc input:

- `--pcap <file>` là nguồn input bắt buộc.
- `--sqlite <file>` hoặc `SQLITE_DATABASE_PATH` là cấu hình SQLite bắt buộc khi chạy phân tích.
- `--filter` là tùy chọn; nếu truyền thì giá trị không được rỗng.
- `--output` nhận `table`, `json`, hoặc `csv`; mặc định là `json`.
- Các cờ cũ như `--interface`, `--duration`, `--capture-backend`, `--db-url`, `--events-json`, `--event-rate-limit`, `--local-net`, và `--ignore-net` bị từ chối với lỗi cấu hình rõ ràng.

## Capture Và Storage

- PCAP mode mở file bằng libpcap, kiểm tra datalink Ethernet, áp dụng BPF nếu có, rồi đọc toàn bộ packet.
- Packet không phải Ethernet hoặc không parse được vẫn bị bỏ qua an toàn ở pipeline parser hiện có.
- SQLite chỉ lưu asset inventory. Migration hiện tại drop bảng `asset_events` nếu database cũ còn tồn tại.
- Cột `last_seen` của asset được cập nhật khi asset đã biết xuất hiện lại, nên không cần event reappearance/flip-flop policy.

## Docker

`Dockerfile` dùng multi-stage build:

- Stage build: `ubuntu:22.04`, `build-essential`, `cmake`, `pkg-config`, `libpcap-dev`, `libsqlite3-dev`, và Qt dev packages.
- Stage runtime: `ubuntu:22.04`, `libpcap0.8`, `libsqlite3-0`, Qt runtime packages, user không đặc quyền `asset`, entrypoint `asset-discovery`.

`docker-compose.yml` cung cấp:

- `pcap-demo`: build/dùng image `passive-asset-discovery`, mount `./samples` read-only, chạy `/samples/multi-asset.pcap`, và ghi SQLite vào volume `pcap-demo-data`.
- `assetd`: profile `service`, dùng SQLite volume `assetd-data`, expose port `8080`, và chạy backend service với sample PCAP.
- Không còn service PostgreSQL, live-capture profile, `PG*`, `DB_*`, hoặc `DATABASE_URL`.

## Lệnh Kiểm Chứng Local

Build local:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Chạy PCAP có filter và SQLite:

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --sqlite pnad.db \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Khi chạy fixture ARP, output có event `new_asset` và bảng asset:

```text
1699606784.0 INFO new_asset ip=192.168.1.10 mac=02:42:ac:11:00:02 protocol=arp iface=pcap
MAC                IPs                     Hostname          First Seen        Last Seen         Sources
--------------------------------------------------------------------------------------------------------
02:42:ac:11:00:02  192.168.1.10                              1699606784.0      1699606784.0      arp
```

Kiểm tra lỗi BPF:

```sh
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --filter invalid --output table
```

Hình dạng lỗi mong đợi:

```text
error: invalid BPF filter for 'samples/arp.pcap': can't parse filter expression: syntax error
```

## Lệnh Docker

Build Docker image:

```sh
docker build -t passive-asset-discovery .
```

Chạy PCAP trong container:

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  -v "$PWD/.docker-data:/data" \
  -e SQLITE_DATABASE_PATH=/data/pnad.db \
  passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Chạy PCAP và ghi SQLite bằng Compose:

```sh
docker compose up --build pcap-demo
```

Chạy binary local để ghi SQLite:

```sh
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --output json
sqlite3 pnad.db \
  "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Với dữ liệu demo, có thể xóa volume SQLite bằng `docker compose down -v` rồi chạy lại `docker compose up --build pcap-demo`.

## Ghi Chú Vận Hành

- PCAP mode không cần quyền mạng đặc biệt; container mặc định chạy bằng user `asset`.
- Filter BPF được libpcap compile trước khi đọc packet; filter sai cú pháp trả lỗi khác 0.
- MAC randomization và MAC spoofing có thể làm một thiết bị xuất hiện như nhiều asset khác nhau.
- Một thiết bị có nhiều interface sẽ được ghi thành nhiều asset nếu các interface dùng MAC khác nhau.
- DHCP metadata phụ thuộc vào traffic quan sát được; thiếu DHCP hostname hoặc IP option thì asset vẫn được ghi nhưng ít metadata hơn.
- Event realtime chỉ dùng để quan sát asset mới trong terminal/UI; không phải dữ liệu lưu trữ.

## Kết Quả Mong Đợi

- Docker image build thành công.
- Container đọc được `/samples/arp.pcap` qua volume read-only.
- `pcap-demo` ghi asset vào bảng `assets` trong SQLite volume.
- Không có bảng `asset_events`, file events JSON/NDJSON, syslog event output, hoặc PostgreSQL service.
