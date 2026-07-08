# Passive Network Asset Discovery System

Passive Network Asset Discovery System (PNAD) là ứng dụng phát hiện tài sản mạng thụ động. Dự án đọc lưu lượng từ file PCAP/PCAPNG hoặc phiên capture trong GUI, phân tích các gói tin quan trọng và tạo inventory thiết bị trong mạng.

PNAD tập trung vào luồng sử dụng chính: phát hiện thiết bị, xem thông tin asset, lưu dữ liệu cục bộ và xuất báo cáo. README này mô tả bản hiện tại của dự án và là tài liệu chính thức để build, chạy, test và đóng gói.

## Tính Năng

- Phân tích file `.pcap` và `.pcapng`.
- GUI desktop Qt/QML có Dashboard, Capture, Assets, Events và Settings.
- Chế độ GUI Live capture hoặc PCAP analysis.
- Bộ lọc capture mặc định:

```text
arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353
```

- Parser cho ARP, DHCP, DNS/mDNS, SSDP, NetBIOS, TCP và IPv4 enrichment.
- Gom nhóm asset theo MAC address và bổ sung IP, hostname, display name, vendor, OS hint, device type, model hint, first seen, last seen và discovery sources.
- Xuất inventory dạng `table`, `json` hoặc `csv`.
- Lưu inventory vào SQLite.
- Email alert trong GUI qua cấu hình môi trường `.env`.
- Docker GUI demo có service mặc định quyền thấp cho PCAP/SQLite/export và service live capture riêng cho traffic thật.
- Bộ test CTest cho core, parser, capture, discovery, storage, GUI model, GUI smoke và boundary check.

## Thành Phần Chính

| Thành phần | Mục đích |
| --- | --- |
| `asset-discovery` | CLI phân tích PCAP/PCAPNG và in inventory. |
| `asset-discovery-gui` | Ứng dụng desktop chính. |
| `asset-capture` | Capture helper/diagnostic; hiện có backend status, raw socket permission status và protocol self-test. |
| `asset-core` | Core library dùng chung cho CLI, GUI và test. |

## Cấu Trúc Dự Án

| Đường dẫn | Nội dung |
| --- | --- |
| `include/pnad/` | Header C++ theo module. |
| `src/app/` | Pipeline capture cấp ứng dụng. |
| `src/capture/` | PCAP/live capture, network interface và capture child process. |
| `src/cli/` | Parse tham số CLI. |
| `src/config/` | Runtime config và default config. |
| `src/core/` | Core session/library. |
| `src/discovery/` | Asset domain, monitor và renderer. |
| `src/event/` | Asset event và event sink. |
| `src/gui/`, `qml/` | Desktop application Qt/QML. |
| `src/packet/` | Packet primitives, parser core, parser plugins và facade. |
| `src/storage/` | SQLite writer. |
| `tests/` | Test được chia theo module tương ứng với source tree. |
| `samples/` | PCAP/PCAPNG fixture dùng cho demo và test. |

## Yêu Cầu Hệ Thống

- CMake 3.16 trở lên.
- Trình biên dịch hỗ trợ C++17.
- SQLite3 development package.
- libpcap development package.
- Qt 5 hoặc Qt 6 với các module desktop/QML cần cho GUI.
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

- Chọn Live hoặc PCAP mode ở trang Capture.
- Chọn network interface hoặc file `.pcap`/`.pcapng`.
- Start capture/analysis.
- Xem asset ở trang Dashboard hoặc Assets.
- Xem event/log trong phiên chạy.
- Export inventory sang CSV hoặc JSON.
- Cấu hình SQLite path và email recipients trong Settings.

Nếu live capture trên Linux thiếu quyền, cấp capability cho binary:

```sh
sudo setcap cap_net_raw,cap_net_admin=eip build/asset-discovery-gui
```

## Chạy CLI

CLI hiện dùng cho PCAP/PCAPNG offline analysis.

Xem help:

```sh
./build/asset-discovery --help
```

Phân tích fixture mẫu và in bảng:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --output table
```

Xuất JSON:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --output json
```

Ghi inventory vào SQLite:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --sqlite pnad.db \
  --output table
```

Tùy chọn CLI:

| Tùy chọn | Mô tả |
| --- | --- |
| `--pcap <file>` | Đọc packet từ file `.pcap` hoặc `.pcapng`. |
| `--filter <bpf>` | Lọc packet bằng BPF expression. |
| `--broad-ipv4-enrichment` | Dùng filter IPv4 rộng hơn để bổ sung TTL OS hint nếu không truyền `--filter`. |
| `--sqlite <file>` | Lưu inventory vào SQLite. |
| `--output table\|json\|csv` | Định dạng output. Mặc định là `json`. |
| `--version` | In phiên bản CLI. |
| `-h`, `--help` | In help. |

## Capture Helper

`asset-capture` là helper diagnostic cho pipeline capture. Ở trạng thái hiện tại, binary này dùng để kiểm tra capture backend, raw socket permission và tự kiểm tra protocol message.

```sh
./build/asset-capture --backend-status
./build/asset-capture --self-test-protocol
```

`--backend-status` tách rõ `available=true/false` của libpcap backend và `raw_socket_permission=allowed|denied|unavailable`. Trường hợp `available=true` nhưng `raw_socket_permission=denied` nghĩa là app build đúng và libpcap có sẵn, nhưng live capture vẫn thiếu quyền raw socket.

Protocol mode đầy đủ chưa được bật; nếu gọi với tham số khác, binary sẽ báo chưa implement.

## Cấu Hình

Default runtime config nằm tại:

```text
configs/default.yaml
```

Hiện config mặc định đặt output format là `json`. CLI vẫn cho phép override bằng `--output`.

SQLite có thể cấu hình bằng tham số CLI:

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap --sqlite pnad.db
```

Hoặc bằng biến môi trường:

```sh
SQLITE_DATABASE_PATH=pnad.db ./build/asset-discovery --pcap samples/multi-asset.pcap
```

Nếu có file `.env`, ứng dụng đọc `SQLITE_DATABASE_PATH` và cấu hình email alert từ file này. Tạo file `.env` từ template:

```sh
cp .env.example .env
```

Các biến email chính:

| Biến | Mục đích |
| --- | --- |
| `PNAD_EMAIL_ALERTS_ENABLED` | Bật/tắt email alert trong GUI. |
| `PNAD_EMAIL_SMTP_HOST` | SMTP host. |
| `PNAD_EMAIL_SMTP_PORT` | SMTP port. |
| `PNAD_EMAIL_TLS_MODE` | TLS mode, ví dụ `starttls`. |
| `PNAD_EMAIL_USERNAME` | SMTP username. |
| `PNAD_EMAIL_PASSWORD_ENV` | Tên biến môi trường chứa password thật. |
| `PNAD_EMAIL_PASSWORD` | Password SMTP trực tiếp. Dễ demo, nhưng không nên commit giá trị thật. |
| `PNAD_SMTP_PASSWORD` | Password SMTP gián tiếp mặc định khi `PNAD_EMAIL_PASSWORD_ENV=PNAD_SMTP_PASSWORD`. |
| `PNAD_EMAIL_FROM` | Địa chỉ gửi. |
| `PNAD_EMAIL_RECIPIENTS` | Danh sách người nhận. |
| `PNAD_GUI_SQLITE_PATH` | Đường dẫn SQLite mặc định cho GUI, hữu ích khi chạy Docker. Nếu không đặt env, GUI dùng `./data/pnad.db`. |
| `PNAD_GUI_PCAP_PATH` | File PCAP/PCAPNG mặc định cho GUI, hữu ích khi chạy Docker. |
| `PNAD_GUI_EXPORT_DIR` | Thư mục export mặc định cho GUI, hữu ích khi chạy Docker. Nếu không đặt env, GUI dùng `./out`. |

GUI gửi email SMTP bằng `curl`. Khi chạy Docker Compose, các biến `PNAD_EMAIL_*` và `PNAD_SMTP_PASSWORD` trong `.env` hoặc shell được truyền vào service GUI. Email chỉ phát sinh khi pipeline tạo sự kiện `NewAsset`; nếu phân tích lại cùng SQLite đã có asset, hãy dùng database mới hoặc PCAP/live traffic tạo asset mới để test.

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

Nếu muốn dùng Xauthority thay vì `xhost`, tạo file cookie cho Docker rồi chạy Compose với biến `XAUTHORITY` phù hợp theo cấu hình desktop của bạn. X11 vẫn là đường chạy mặc định được khuyến nghị vì ổn định hơn Wayland trong container.

Wayland có thể thử thủ công trên host hỗ trợ Qt Wayland bằng cách mount socket Wayland, truyền `WAYLAND_DISPLAY` và `XDG_RUNTIME_DIR`, rồi đặt `QT_QPA_PLATFORM=wayland`. Hướng này phụ thuộc compositor nên chưa là workflow mặc định.

### Live Capture Trong GUI

Service `pnad-gui-live` chạy cùng ứng dụng Qt/QML như `pnad-gui`, nhưng được bật host networking và `NET_RAW`/`NET_ADMIN` để GUI có thể capture từ interface thật của host.

```sh
docker compose up --build pnad-gui-live
```

Trong GUI, vào `Capture`, chọn tab `Live Capture`, chọn interface thật của host có trạng thái ready, rồi bấm `Start Live`. Compose cấp `network_mode: host` và capabilities `NET_RAW`, `NET_ADMIN`; không bật `privileged` mặc định.

Entrypoint chuẩn bị mount `/data`, `/out`, `/work/logs` bằng root rồi chạy GUI bằng runtime user `asset`. Với `pnad-gui-live`, entrypoint giữ ambient `NET_RAW`/`NET_ADMIN` cho process GUI. Nếu host hoặc Docker runtime không cho capability-based capture hoạt động, fallback cuối cùng là chạy riêng live service bằng root trong container và phải ghi rõ điều đó khi demo.

Nếu UID/GID trên máy bạn không phải `1000:1000`, đặt `PNAD_UID=$(id -u)` và `PNAD_GID=$(id -g)` trước khi build để file trong `./data` và `./out` thuộc đúng user host.

### Debug Và Verification

Build image thủ công:

```sh
docker build -t passive-asset-discovery .
```

CLI chỉ dùng để debug, không phải demo chính:

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

Trong `Settings`, trường `Recipients` luôn có thể chỉnh và lưu. Email chỉ được gửi khi `PNAD_EMAIL_ALERTS_ENABLED=true` cùng SMTP settings hợp lệ được cấu hình qua `.env` hoặc environment.

## Test

Chạy toàn bộ test sau khi build:

```sh
ctest --test-dir build --output-on-failure
```

Test hiện được chia theo module trong `tests/`, gồm parser, discovery, capture, storage, config, core, system utility, GUI model, GUI smoke, PCAP fixture và boundary check cho `asset-core`.

## Sample PCAP

| File | Mục đích |
| --- | --- |
| `samples/arp.pcap` | Fixture Ethernet tối thiểu gồm một ARP request. |
| `samples/multi-asset.pcap` | Fixture nhiều packet, phát hiện nhiều asset qua ARP và DHCP. |
| `samples/arp-test/arp-storm.pcap` | Fixture ARP traffic. |
| `samples/dhcp-test/dhcp.pcap` | Fixture DHCP. |
| `samples/dns-mdns-test/dns-mdns.pcap` | Fixture DNS/mDNS. |
| `samples/nestbios-test/smb-legacy-implementation.pcapng` | Fixture NetBIOS/SMB legacy. |
| `samples/tcp-test/chargen-tcp.pcap` | Fixture TCP. |
| `samples/tcp-test/tfp_capture.pcapng` | Fixture TCP capture dạng PCAPNG. |

Xem thêm trong `samples/README.md`.

## Troubleshooting

- `libpcap was not found`: cài `libpcap-dev`, xóa hoặc cấu hình lại `build/`, sau đó chạy lại CMake.
- Thiếu Qt module khi build: cài đầy đủ các gói Qt development ở phần yêu cầu hệ thống.
- GUI không mở do thiếu QML module: kiểm tra các gói `qml-module-qtquick-*`.
- Live capture trong Docker không có quyền: chạy `docker compose --profile debug run --rm backend-status-live`. Nếu `raw_socket_permission=denied`, kiểm tra lại `pnad-gui-live` có `network_mode: host` và `cap_add: NET_RAW, NET_ADMIN`; rootless Docker, Docker Desktop hoặc host bị hạn chế có thể không cấp được raw socket cho container.
- PCAP không phát hiện asset: kiểm tra file có link type Ethernet và filter không loại bỏ gói cần phân tích.
- SQLite không được tạo: kiểm tra quyền ghi thư mục và đường dẫn `--sqlite` hoặc `SQLITE_DATABASE_PATH`.
