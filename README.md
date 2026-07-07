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
- Docker PCAP demo chạy bằng fixture có sẵn, không cần quyền live capture.
- Bộ test CTest cho core, parser, capture, discovery, storage, GUI model và smoke test.

## Thành Phần Chính

| Thành phần | Mục đích |
| --- | --- |
| `asset-discovery` | CLI phân tích PCAP/PCAPNG và in inventory. |
| `asset-discovery-gui` | Ứng dụng desktop chính. |
| `asset-capture` | Capture helper dùng bởi pipeline capture. |
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
| `scripts/` | Script build smoke, Docker verification và đóng gói release. |

## Yêu Cầu Hệ Thống

- CMake 3.16 trở lên.
- Trình biên dịch hỗ trợ C++17.
- SQLite3 development package.
- libpcap development package.
- Qt 5 hoặc Qt 6 với các module desktop/QML cần cho GUI.
- Docker và Docker Compose nếu chạy demo container.

Cài dependency trên Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  libsqlite3-dev \
  libpcap-dev \
  qtbase5-dev \
  qtdeclarative5-dev \
  libqt5websockets5-dev \
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
| `PNAD_EMAIL_FROM` | Địa chỉ gửi. |
| `PNAD_EMAIL_RECIPIENTS` | Danh sách người nhận. |

## Docker Demo

Chạy PCAP demo bằng Docker Compose:

```sh
docker compose up --build pcap-demo
```

Demo build image, mount `samples/` read-only, phân tích `samples/multi-asset.pcap`, ghi SQLite vào Docker volume và in inventory dạng bảng.

Build image thủ công:

```sh
docker build -t passive-asset-discovery .
```

Chạy CLI trong container:

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --output json
```

Kiểm tra runtime Docker đầy đủ:

```sh
./scripts/verify-docker-runtime.sh
```

## Test

Chạy toàn bộ test sau khi build:

```sh
ctest --test-dir build --output-on-failure
```

Chạy smoke build dành cho CI/dev:

```sh
./scripts/ci-smoke-build.sh
```

Test hiện được chia theo module trong `tests/`, gồm parser, discovery, capture, storage, config, core, GUI model, GUI smoke và các kiểm tra PCAP fixture.

## Đóng Gói Desktop

Tạo gói desktop Linux:

```sh
./scripts/package-release.sh
```

Kết quả:

```text
release/pnad-desktop-linux.tar.gz
```

Gói desktop chỉ ship GUI, launcher, icon và file `.env` mặc định. CLI không nằm trong desktop release package.

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
- Live capture không có quyền: chạy với quyền phù hợp hoặc cấp capability `cap_net_raw,cap_net_admin`.
- PCAP không phát hiện asset: kiểm tra file có link type Ethernet và filter không loại bỏ gói cần phân tích.
- SQLite không được tạo: kiểm tra quyền ghi thư mục và đường dẫn `--sqlite` hoặc `SQLITE_DATABASE_PATH`.
