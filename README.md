# Passive Network Asset Discovery System

PNAD là ứng dụng phát hiện tài sản mạng thụ động, tập trung vào luồng cốt lõi của đề tài: bắt và phân tích lưu lượng ARP/DHCP/SSDP/mDNS, phát hiện thiết bị trong mạng, hiển thị IP/MAC/hostname/vendor/OS/type/model/thời điểm xuất hiện/protocol, xuất dữ liệu CSV/JSON và chạy demo PCAP bằng Docker.

## Tính Năng Chính

- **PCAP offline**: chọn file `.pcap`/`.pcapng` và phân tích lại lưu lượng đã ghi mà không cần quyền live capture.
- **Phạm vi giao thức tập trung**: workflow chính sử dụng bộ lọc `arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353` để tập trung vào ARP, DHCP, SSDP và mDNS.
- **Phát hiện tài sản mạng**: nhận diện thiết bị theo MAC, IP, hostname, display name, vendor, OS hint, device type, model hint, thời điểm thấy lần đầu, thời điểm thấy lần cuối và protocol phát hiện.
- **Bảng tài sản và chi tiết tài sản**: hiển thị danh sách thiết bị đã phát hiện và thông tin chi tiết của thiết bị được chọn.
- **Log tài sản mới**: ghi nhận khi phát hiện một asset mới trong quá trình phân tích/capture.
- **Xuất dữ liệu**: xuất inventory hiện tại ra CSV hoặc JSON để phục vụ báo cáo và kiểm chứng.
- **Docker PCAP demo**: chạy kịch bản demo có tính lặp lại từ file PCAP mẫu mà không cần quyền live capture.
- **Kiểm thử tự động**: có test cho core engine, model/controller GUI, package inspection và QML smoke test cho màn hình discovery chính.

## Luồng Sử Dụng Chính

- **PCAP mode**: chọn file `.pcap`/`.pcapng` để phân tích offline.
- **Asset inventory**: xem IP, MAC, hostname, display name, vendor, OS hint, device type, model hint, first seen, last seen và protocol của tài sản đã phát hiện.
- **Export**: lưu kết quả hiện tại dưới dạng CSV hoặc JSON.
- **Demo Docker**: chạy demo PCAP mẫu để kiểm chứng nhanh luồng phát hiện tài sản.

## Build Requirements

Native desktop build:

- CMake 3.16 or newer.
- C++17 compiler.
- SQLite3 development package.
- Qt Quick/QML runtime and development packages.
- libpcap development package. PNAD is PCAP-based and the build fails when libpcap is unavailable.

Ubuntu/Debian desktop dependencies:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libsqlite3-dev libpcap-dev qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev qml-module-qtquick-controls2 qml-module-qtquick-dialogs qml-module-qtquick-layouts qml-module-qtquick-window2
```

## Build The Desktop App

```sh
cmake -S . -B build
cmake --build build --parallel
```

Run the desktop app:

```sh
./build/asset-discovery-gui
```

## Docker Demo

PCAP demo does not need live capture privileges:

```sh
docker compose up --build pcap-demo
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

The test set includes core engine tests, GUI model/controller tests, a package inspection test, and an offscreen QML smoke test that verifies the simplified core discovery view loads.

## Package

Build the GUI-only desktop package:

```sh
./scripts/package-release.sh
```
