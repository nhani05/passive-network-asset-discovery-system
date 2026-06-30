# Hướng Dẫn Đóng Gói Hệ Thống Bằng Docker

Tài liệu này chỉ tập trung vào cách dùng Docker để đóng gói Passive Network Asset Discovery System thành container image có thể chạy trên môi trường khác.

## Yêu Cầu

- Docker Engine hoặc Docker Desktop.
- Repository đã được checkout đầy đủ.
- Chạy các lệnh từ thư mục gốc của repository.
- Nếu cần live capture trong container, nên dùng Linux host thật; Docker Desktop trên macOS/Windows có giới hạn khác với host networking Linux.

## Build Image

Build image runtime bằng `Dockerfile` multi-stage:

```sh
docker build -t passive-asset-discovery:latest .
```

Dockerfile sẽ:

- Dùng stage build để cài compiler, CMake, `pkg-config`, và `libpcap-dev`.
- Build binary C++ bằng `ASSET_DISCOVERY_REQUIRE_PCAP=ON`.
- Dùng stage runtime nhẹ hơn với `libpcap0.8`, `postgresql-client`, và binary `asset-discovery`.
- Chạy mặc định bằng user `asset` trong thư mục `/work`.

## Kiểm Tra Image

Xem image vừa build:

```sh
docker image ls passive-asset-discovery
```

Kiểm tra CLI help:

```sh
docker run --rm passive-asset-discovery:latest --help
```

## Chạy Thử Với PCAP

Mount thư mục `samples` vào container ở chế độ read-only:

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery:latest \
  --pcap /samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

JSON output:

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery:latest \
  --pcap /samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output json
```

## Đóng Gói Image Thành File Tar

Xuất image thành file để chuyển sang máy khác:

```sh
docker save passive-asset-discovery:latest -o passive-asset-discovery.tar
```

Kiểm tra file:

```sh
ls -lh passive-asset-discovery.tar
```

Không nên commit file `.tar` này vào repository vì đây là artifact build.

## Load Image Trên Máy Khác

Trên máy đích:

```sh
docker load -i passive-asset-discovery.tar
docker image ls passive-asset-discovery
```

Chạy kiểm tra:

```sh
docker run --rm passive-asset-discovery:latest --help
```

## Tag Image Theo Phiên Bản

Nên tag image theo phiên bản hoặc mốc demo:

```sh
docker tag passive-asset-discovery:latest passive-asset-discovery:v0.4.0-demo
```

Xuất image đã tag:

```sh
docker save passive-asset-discovery:v0.4.0-demo -o passive-asset-discovery-v0.4.0-demo.tar
```

## Build Bằng Docker Compose

Compose có thể build image cho các service demo:

```sh
docker compose build
```

Chạy demo PCAP cùng PostgreSQL:

```sh
docker compose up --build pcap-demo
```

Service `pcap-demo` sẽ:

- Build image từ `Dockerfile`.
- Mount `./samples` vào `/samples:ro`.
- Chạy `asset-discovery` với `samples/multi-asset.pcap`.
- Ghi kết quả vào PostgreSQL service nếu database sẵn sàng.

## Live Capture Trong Container

Live capture cần quyền mạng đặc biệt và Linux host:

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_DURATION=60 \
docker compose --profile live run --rm live-capture
```

Hoặc chạy trực tiếp:

```sh
docker run --rm --user 0:0 --net=host \
  --cap-add=NET_ADMIN \
  --cap-add=NET_RAW \
  passive-asset-discovery:latest \
  --interface eth0 \
  --duration 60 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Đổi `eth0` theo interface thật trên host.

## Dọn Dẹp

Xóa container dừng và image không dùng:

```sh
docker container prune
docker image prune
```

Xóa image cụ thể:

```sh
docker image rm passive-asset-discovery:latest
```
