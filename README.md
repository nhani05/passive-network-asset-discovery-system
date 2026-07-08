# Passive Network Asset Discovery System

Passive Network Asset Discovery System (PNAD) là ứng dụng phát hiện tài sản mạng thụ động. Dự án đọc lưu lượng từ file PCAP/PCAPNG hoặc từ live capture trong GUI, phân tích các protocol discovery phổ biến và tạo inventory thiết bị trong mạng.

README này là tài liệu chính để build, chạy, test, demo Docker và vận hành bản hiện tại của dự án.

## Trạng Thái Hiện Tại

- CLI `asset-discovery` dùng cho phân tích PCAP/PCAPNG offline.
- GUI desktop `asset-discovery-gui` hỗ trợ cả PCAP/PCAPNG analysis và live capture.
- SQLite là storage bắt buộc cho CLI và là storage mặc định của GUI.
- Docker Compose có service GUI quyền thấp cho demo PCAP/SQLite/export và service live capture riêng cho traffic thật.
- Bộ test CTest hiện có 39 test cho CLI, parser, capture, core pipeline, storage, config, GUI model, GUI smoke và boundary check.

## Tính Năng

- Phân tích file `.pcap` và `.pcapng` có link type Ethernet.
- Live capture từ network interface trong GUI bằng libpcap.
- Parser cho ARP, DHCP, DNS, mDNS, LLMNR, SSDP, NetBIOS name service, TCP SYN-ACK và IPv4 endpoint enrichment.
- Bộ lọc capture mặc định:

```text
arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353
```

- Tùy chọn `--broad-ipv4-enrichment` dùng filter rộng hơn cho IPv4/TCP/TTL enrichment khi CLI không truyền `--filter`.
- Gom nhóm asset theo MAC address và bổ sung IP, hostname, display name, vendor, OS hint, device type, model hint, first seen, last seen và discovery sources.
- Xuất inventory dạng `table`, `json` hoặc `csv`.
- Ghi inventory, app settings và lịch sử phiên phân tích vào SQLite.
- Event `new_asset` được ghi ra stdout trong CLI và hiển thị trong Events của GUI.
- Email alert trong GUI khi phát hiện asset mới, cấu hình qua `.env` hoặc environment.
- Export JSON/CSV từ GUI.

## Thành Phần Chính

| Thành phần | Mục đích |
| --- | --- |
| `asset-discovery` | CLI phân tích PCAP/PCAPNG offline, ghi SQLite và in inventory. |
| `asset-discovery-gui` | Ứng dụng desktop Qt/QML cho dashboard, capture, assets, events và settings. |
| `asset-capture` | Helper diagnostic cho capture backend, raw socket permission và protocol self-test. |
| `asset-core` | Core library dùng chung cho GUI, CLI và test. |

## Cấu Trúc Dự Án

| Đường dẫn | Nội dung |
| --- | --- |
| `include/pnad/` | Header C++ theo module. |
| `src/app/` | Live capture pipeline và xử lý batch/concurrency. |
| `src/capture/` | PCAP/live capture, network interface, packet stream và capture child protocol. |
| `src/cli/` | Parse tham số CLI. |
| `src/config/` | Runtime config và loader cho `configs/default.yaml`. |
| `src/core/` | Core session orchestration cho PCAP/batch/stream. |
| `src/discovery/` | Asset domain, monitor và renderer. |
| `src/event/` | Asset event và event sink. |
| `src/gui/`, `qml/` | Desktop application Qt/QML. |
| `src/packet/` | Packet primitives, parser core, parser plugins và facade. |
| `src/storage/` | SQLite writer và schema migration. |
| `tests/` | Unit, integration và smoke tests theo module. |
| `samples/` | PCAP/PCAPNG fixture dùng cho demo và test. |
| `docs/design-spec/` | Sơ đồ và mô tả kiến trúc. |

## Yêu Cầu Hệ Thống

- CMake 3.16 trở lên.
- Trình biên dịch hỗ trợ C++17.
- SQLite3 development package.
- libpcap development package.
- Qt 5 hoặc Qt 6 với Core, Quick, Gui, Qml, Widgets và Network.
- `curl` nếu dùng email alert ngoài Docker.
- Docker và Docker Compose nếu chạy demo container.

Cài dependency trên Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  curl \
  pkg-config \
  libsqlite3-dev \
  libpcap-dev \
  qtbase5-dev \
  qtdeclarative5-dev \
  qtquickcontrols2-5-dev \
  qml-module-qtquick-controls2 \
  qml-module-qtquick-dialogs \
  qml-module-qtquick-layouts \
  qml-module-qtquick-window2
```

## Build

Build mặc định:

```sh
cmake -S . -B build
cmake --build build --parallel
```

Build release:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Nếu CMake báo thiếu libpcap, cài `libpcap-dev` rồi cấu hình lại build directory.

## Chạy GUI

```sh
./build/asset-discovery-gui
```

Luồng sử dụng chính trong GUI:

- Vào `Capture`, chọn `Live Capture` hoặc `PCAP/PCAPNG`.
- Với PCAP, chọn file `.pcap`/`.pcapng` rồi bấm `Analyze PCAP`.
- Với live capture, chọn network interface có trạng thái ready rồi bấm `Start Live`.
- Vào `Dashboard` hoặc `Assets` để xem inventory.
- Vào `Events` để xem event/log trong phiên chạy.
- Vào `Assets` để export JSON/CSV.
- Vào `Settings` để cấu hình SQLite path và email recipients.

GUI mặc định dùng `./data/pnad.db` và thư mục export `./out` nếu không có environment override. Nếu live capture trên Linux thiếu quyền, cấp capability cho binary:

```sh
sudo setcap cap_net_raw,cap_net_admin=eip build/asset-discovery-gui
```

## Chạy CLI

CLI hiện chỉ xử lý PCAP/PCAPNG offline. SQLite là bắt buộc: truyền `--sqlite <file>` hoặc đặt `SQLITE_DATABASE_PATH`.

Xem help:

```sh
./build/asset-discovery --help
```

Phân tích fixture mẫu và in bảng:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --sqlite /tmp/pnad-demo.db \
  --output table
```

Xuất JSON:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --sqlite /tmp/pnad-demo.db \
  --output json
```

Dùng SQLite path từ environment:

```sh
SQLITE_DATABASE_PATH=/tmp/pnad-demo.db \
  ./build/asset-discovery --pcap samples/multi-asset.pcap --output csv
```

Lưu ý: CLI không reset SQLite đích. Khi ghi vào database đã có dữ liệu, các asset trùng MAC được upsert/merge, còn asset khác, app settings và analysis sessions hiện có được giữ lại.

Tùy chọn CLI:

| Tùy chọn | Mô tả |
| --- | --- |
| `--pcap <file>` | Đọc packet từ file `.pcap` hoặc `.pcapng`. |
| `--filter <bpf>` | Lọc packet bằng BPF expression. Nếu truyền tùy chọn này, filter mặc định không còn được dùng. |
| `--broad-ipv4-enrichment` | Dùng filter `arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353 or ip` nếu không truyền `--filter`. Hữu ích cho TCP SYN-ACK và TTL OS hint. |
| `--sqlite <file>` | Lưu inventory vào SQLite. |
| `--output table\|json\|csv` | Định dạng output. Mặc định là `json`. |
| `--version` | In phiên bản CLI. |
| `-h`, `--help` | In help. |

NetBIOS, DNS port 53, LLMNR và TCP enrichment có parser nhưng không nằm trong filter mặc định. Dùng `--filter` hoặc `--broad-ipv4-enrichment` khi cần đọc các loại traffic đó từ fixture hoặc capture riêng.

## Capture Helper

`asset-capture` là helper diagnostic cho pipeline capture. Ở trạng thái hiện tại, binary này dùng để kiểm tra capture backend, raw socket permission và tự kiểm tra protocol message.

```sh
./build/asset-capture --backend-status
./build/asset-capture --self-test-protocol
```

`--backend-status` tách rõ `available=true/false` của libpcap backend và `raw_socket_permission=allowed|denied|unavailable`. Trường hợp `available=true` nhưng `raw_socket_permission=denied` nghĩa là app build đúng và libpcap có sẵn, nhưng live capture vẫn thiếu quyền raw socket.

Protocol mode đầy đủ chưa được bật; nếu gọi với tham số khác, binary sẽ báo chưa implement.

## Cấu Hình Và SQLite

Default runtime config nằm tại:

```text
configs/default.yaml
```

Config mặc định chỉ cấu hình output format:

```yaml
output:
  format: json
```

Các section cũ như `capture`, `database`, `events` và `network` không còn được loader chấp nhận. Capture source và SQLite path được cấu hình bằng CLI/environment hoặc GUI settings.

SQLite hiện có các bảng chính:

| Bảng | Mục đích |
| --- | --- |
| `assets` | Inventory theo MAC address. |
| `app_settings` | Settings được GUI lưu lại. |
| `analysis_sessions` | Lịch sử phiên PCAP/live capture trong GUI. |

CLI đọc `.env` cho `SQLITE_DATABASE_PATH`. GUI đọc `.env` cho SQLite, đường dẫn demo Docker và email settings. Tạo file `.env` từ template:

```sh
cp .env.example .env
```

Các biến chính:

| Biến | Mục đích |
| --- | --- |
| `SQLITE_DATABASE_PATH` | SQLite path cho CLI, hoặc fallback cho GUI nếu `PNAD_GUI_SQLITE_PATH` không đặt. |
| `PNAD_GUI_SQLITE_PATH` | SQLite path mặc định cho GUI. Trong Docker mặc định là `/data/pnad.db`. |
| `PNAD_GUI_PCAP_PATH` | File PCAP/PCAPNG mặc định cho GUI. |
| `PNAD_GUI_EXPORT_DIR` | Thư mục export mặc định cho GUI. |
| `PNAD_DOCKER_RUNTIME` | Bật nhánh diagnostic phù hợp khi chạy trong container. |
| `PNAD_EMAIL_ALERTS_ENABLED` | Bật/tắt email alert trong GUI. |
| `PNAD_EMAIL_SMTP_HOST` | SMTP host. |
| `PNAD_EMAIL_SMTP_PORT` | SMTP port. |
| `PNAD_EMAIL_TLS_MODE` | TLS mode, ví dụ `starttls`. |
| `PNAD_EMAIL_USERNAME` | SMTP username. |
| `PNAD_EMAIL_PASSWORD_ENV` | Tên biến môi trường chứa password thật. |
| `PNAD_EMAIL_PASSWORD` | Password SMTP trực tiếp. Chỉ nên dùng cho demo local. |
| `PNAD_SMTP_PASSWORD` | Password SMTP gián tiếp mặc định khi `PNAD_EMAIL_PASSWORD_ENV=PNAD_SMTP_PASSWORD`. |
| `PNAD_EMAIL_FROM` | Địa chỉ gửi. |
| `PNAD_EMAIL_RECIPIENTS` | Danh sách người nhận. |

GUI gửi email SMTP bằng `curl`. Email chỉ phát sinh khi pipeline tạo sự kiện `NewAsset`; nếu phân tích lại cùng SQLite đã có asset, hãy dùng database mới hoặc traffic tạo MAC mới để test email.

## Docker Native GUI Demo

Docker demo chính chạy trực tiếp ứng dụng desktop `asset-discovery-gui` qua display của host Linux. Demo không dùng browser, noVNC, Xvfb, Fluxbox, x11vnc hoặc websockify.

Một binary GUI được dùng cho hai service:

| Service | Mục đích | Quyền |
| --- | --- | --- |
| `pnad-gui` | Workflow mặc định: PCAP/PCAPNG, SQLite, export, settings, assets, events. | Không dùng host networking hoặc quyền packet capture. |
| `pnad-gui-live` | Workflow live capture từ interface thật của host. | Dùng `network_mode: host` và `NET_RAW`/`NET_ADMIN`. |

Chuẩn bị thư mục lưu dữ liệu và export:

```sh
mkdir -p data out
```

Nếu trước đó bạn đã chạy container cũ và thấy lỗi SQLite read-only, sửa owner của thư mục mount một lần:

```sh
sudo chown -R "$(id -u):$(id -g)" data out
```

Cho phép container kết nối X11. Cách đơn giản cho demo local:

```sh
xhost +local:docker
```

Chạy GUI mặc định:

```sh
docker compose up --build pnad-gui
```

Khi cửa sổ PNAD mở ra:

- PCAP/PCAPNG: vào `Capture`, chọn tab `PCAP/PCAPNG`, chọn file trong `/samples`, ví dụ `/samples/multi-asset.pcap`, rồi bấm `Analyze PCAP`.
- SQLite: vào `Settings`, dùng `/data/pnad.db` để dữ liệu tồn tại ngoài vòng đời container.
- Export: vào `Assets`, export JSON/CSV. File dialog mặc định mở tại `/out`; file sẽ xuất hiện trong thư mục `./out` trên host.

Sau demo, có thể thu hồi quyền X11:

```sh
xhost -local:docker
```

Nếu UID/GID trên máy bạn không phải `1000:1000`, đặt `PNAD_UID=$(id -u)` và `PNAD_GID=$(id -g)` trước khi build để file trong `./data` và `./out` thuộc đúng user host.

### Live Capture Trong GUI

Service `pnad-gui-live` chạy cùng ứng dụng Qt/QML như `pnad-gui`, nhưng được bật host networking và `NET_RAW`/`NET_ADMIN` để GUI có thể capture từ interface thật của host.

```sh
docker compose up --build pnad-gui-live
```

Trong GUI, vào `Capture`, chọn tab `Live Capture`, chọn interface thật của host có trạng thái ready, rồi bấm `Start Live`. Compose cấp `network_mode: host` và capabilities `NET_RAW`, `NET_ADMIN`; không bật `privileged` mặc định.

Entrypoint chuẩn bị mount `/data`, `/out`, `/work/logs` bằng root rồi chạy GUI bằng runtime user `asset`. Với `pnad-gui-live`, entrypoint giữ ambient `NET_RAW`/`NET_ADMIN` cho process GUI. Nếu host hoặc Docker runtime không cho capability-based capture hoạt động, fallback cuối cùng là chạy riêng live service bằng root trong container và phải ghi rõ điều đó khi demo.

### Debug Và Verification

Build image thủ công:

```sh
docker build -t passive-asset-discovery .
```

CLI chỉ dùng để debug trong container, không phải demo chính:

```sh
docker compose --profile debug run --rm debug-cli
docker compose --profile debug run --rm backend-status
docker compose --profile debug run --rm backend-status-live
```

`backend-status` chạy trong permission profile thấp hơn và có thể báo `raw_socket_permission=denied`; điều đó là expected cho workflow `pnad-gui`. `backend-status-live` chạy với cùng host networking và capabilities như `pnad-gui-live`; nếu lệnh này vẫn báo denied thì live capture chưa được Docker/host cấp quyền đúng.

Chạy test trong Docker:

```sh
docker compose --profile test build test
docker compose --profile test run --rm test
```

Runtime paths trong container:

| Path | Mục đích |
| --- | --- |
| `/samples` | PCAP/PCAPNG mount read-only từ `./samples`. |
| `/data` | SQLite database, mặc định `/data/pnad.db`. |
| `/out` | File export JSON/CSV từ GUI. |
| `/work/configs` | Config mặc định đóng gói trong image. |
| `/work/logs` | Log runtime nếu cần. |

## Test

Chạy toàn bộ test sau khi build:

```sh
ctest --test-dir build --output-on-failure
```

Liệt kê test:

```sh
ctest --test-dir build -N
```

Test hiện gồm parser primitives/plugins, parser facade/core, discovery, event sink/detector, SQLite writer, CLI arguments, config, capture backend/protocol/stream/supervisor, bounded queue, live pipeline, core session, PCAP fixture validation, kiểm tra CLI không xóa database hiện có, GUI model, GUI smoke và boundary check cho `asset-core`.

## Sample PCAP

| File | Mục đích |
| --- | --- |
| `samples/arp.pcap` | Fixture Ethernet tối thiểu gồm một ARP request. |
| `samples/multi-asset.pcap` | Fixture deterministic nhiều asset qua ARP và DHCP. |
| `samples/test.pcap` | PCAPNG demo tổng hợp có ARP, DHCP, mDNS và SSDP. |
| `samples/test2.pcap` | PCAP demo lớn hơn cho nhiều loại asset và metadata enrichment. |
| `samples/arp-test/arp-storm.pcap` | ARP traffic lặp để kiểm tra merge IP/last_seen. |
| `samples/dhcp-test/dhcp.pcap` | DHCP fixture. |
| `samples/dns-mdns-test/dns-mdns.pcap` | DNS/mDNS-oriented fixture. |
| `samples/ssdp-test/ssdp` | SSDP fixture không có extension; dùng tốt cho CLI, cần đổi tên `.pcap` nếu chọn bằng GUI. |
| `samples/nestbios-test/smb-legacy-implementation.pcapng` | Legacy NetBIOS/SMB reference fixture. |
| `samples/tcp-test/chargen-tcp.pcap` | TCP SYN-ACK/IPv4 enrichment fixture, nên chạy với `--broad-ipv4-enrichment`. |
| `samples/tcp-test/tfp_capture.pcapng` | Mixed-interface PCAPNG edge fixture; CLI hiện có thể từ chối nếu interface types khác nhau. |

Xem thêm trong `samples/README.md`.

## Troubleshooting

- `SQLite configuration is required`: truyền `--sqlite <file>` hoặc đặt `SQLITE_DATABASE_PATH`.
- `libpcap was not found`: cài `libpcap-dev`, xóa hoặc cấu hình lại `build/`, sau đó chạy lại CMake.
- Thiếu Qt module khi build: cài đầy đủ các gói Qt development ở phần yêu cầu hệ thống.
- GUI không mở do thiếu QML module: kiểm tra các gói `qml-module-qtquick-*`.
- Live capture trong Docker không có quyền: chạy `docker compose --profile debug run --rm backend-status-live`. Nếu `raw_socket_permission=denied`, kiểm tra lại `pnad-gui-live` có `network_mode: host` và `cap_add: NET_RAW, NET_ADMIN`.
- PCAP không phát hiện asset: kiểm tra link type Ethernet và filter không loại bỏ gói cần phân tích.
- TCP/IP enrichment không xuất hiện: dùng `--broad-ipv4-enrichment` hoặc filter chứa `ip`.
- SSDP sample không chọn được trong GUI: file `samples/ssdp-test/ssdp` không có extension; đổi tên/copy thành `.pcap` nếu cần chạy qua file picker.
- SQLite không được tạo: kiểm tra quyền ghi thư mục và đường dẫn `--sqlite`, `SQLITE_DATABASE_PATH` hoặc `PNAD_GUI_SQLITE_PATH`.
