# Đề Tài 1: Passive Network Asset Discovery System

## Mục Tiêu

PNAD là hệ thống giám sát mạng thụ động có khả năng đọc traffic từ file PCAP/PCAPNG hoặc network interface, phát hiện thiết bị trong mạng nội bộ và lưu inventory tài sản mạng.

Dự án đã hoàn thiện luồng chính:

- phát hiện asset mới từ ARP, DHCP, mDNS, SSDP và các nguồn enrichment khác,
- gom nhóm asset theo MAC address,
- lưu inventory vào SQLite,
- xem kết quả qua GUI desktop,
- chạy demo bằng Docker,
- kiểm thử tự động bằng CTest.

## Đối Chiếu Yêu Cầu

| Yêu cầu ban đầu | Trạng thái triển khai |
| --- | --- |
| Phát hiện asset mới từ traffic mạng | Đã có `AssetMonitor`, `AssetEventDetector` và event `new_asset` trong CLI/GUI. |
| Thu thập thông tin cơ bản của asset | Đã thu MAC, IP, hostname, display name, vendor, OS hint, device type, model hint, first seen, last seen và discovery sources. |
| Phân tích ARP/DHCP | Đã có parser ARP và DHCP, kèm metadata từ DHCP options. |
| Hoạt động với traffic thực tế hoặc file PCAP | CLI hỗ trợ PCAP/PCAPNG offline; GUI hỗ trợ PCAP/PCAPNG và live capture từ interface thật. |
| Triển khai bằng Docker | Đã có `Dockerfile` và `docker-compose.yml` với service `pnad-gui`, `pnad-gui-live`, debug và test. |
| Tài liệu thiết kế hệ thống | Xem `docs/design-spec/system-architecture.md`. |
| Hướng dẫn build/deploy/demo | Xem `README.md`. |

## Phạm Vi Tính Năng Hiện Có

- CLI `asset-discovery`:
  - đọc file `.pcap`/`.pcapng`,
  - nhận `--filter <bpf>`,
  - hỗ trợ `--broad-ipv4-enrichment`,
  - xuất `table`, `json`, `csv`,
  - ghi SQLite bắt buộc qua `--sqlite` hoặc `SQLITE_DATABASE_PATH`.

- GUI `asset-discovery-gui`:
  - Dashboard, Capture, Assets, Events và Settings,
  - PCAP/PCAPNG analysis,
  - live capture từ network interface,
  - lưu settings và analysis sessions,
  - export JSON/CSV,
  - email alert khi có asset mới.

- Parser/enrichment:
  - ARP,
  - DHCP,
  - DNS/mDNS/LLMNR,
  - SSDP,
  - NetBIOS name service,
  - TCP SYN-ACK,
  - IPv4 TTL-based OS hint,
  - curated OUI vendor hints.

- Storage:
  - bảng `assets`,
  - bảng `app_settings`,
  - bảng `analysis_sessions`,
  - migration qua SQLite `PRAGMA user_version`.

## Build Và Chạy Nhanh

Build:

```sh
cmake -S . -B build
cmake --build build --parallel
```

Chạy CLI với fixture mẫu:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --sqlite /tmp/pnad-demo.db \
  --output table
```

Chạy GUI native:

```sh
./build/asset-discovery-gui
```

Chạy Docker GUI demo:

```sh
mkdir -p data out
xhost +local:docker
docker compose up --build pnad-gui
```

Chạy Docker GUI live capture:

```sh
docker compose up --build pnad-gui-live
```

## Kiểm Thử

Chạy toàn bộ test:

```sh
ctest --test-dir build --output-on-failure
```

Bộ test hiện bao phủ:

- CLI argument/config validation,
- parser primitives và parser plugins,
- asset store/monitor/event detector,
- SQLite writer,
- capture backend/protocol/packet stream/supervisor,
- bounded queue và live pipeline,
- core session,
- PCAP fixture validation,
- GUI model và GUI smoke test,
- boundary check cho `asset-core`.

## Tài Liệu Liên Quan

| Tài liệu | Nội dung |
| --- | --- |
| `README.md` | Hướng dẫn build, chạy CLI/GUI, Docker, config, test và troubleshooting. |
| `docs/design-spec/system-architecture.md` | Sơ đồ kiến trúc, pipeline packet và runtime Docker. |
| `samples/README.md` | Danh sách fixture PCAP/PCAPNG và lệnh chạy mẫu. |
| `.env.example` | Template cấu hình SQLite, Docker GUI defaults và email alert. |

## Ghi Chú Vận Hành

- CLI không reset SQLite đích; asset trùng MAC được upsert/merge, dữ liệu khác trong database được giữ lại.
- GUI live capture cần quyền raw socket; dùng `setcap` khi chạy native hoặc service `pnad-gui-live` khi chạy Docker.
- Filter mặc định tập trung vào ARP, DHCP, SSDP và mDNS. Dùng `--filter` hoặc `--broad-ipv4-enrichment` cho DNS port 53, LLMNR, NetBIOS hoặc TCP/IPv4 enrichment.
