# Hướng Dẫn Đóng Gói Hệ Thống Bằng Docker

Tài liệu này chỉ tập trung vào cách dùng Docker để đóng gói Passive Network Asset Discovery System thành container image có thể chạy trên môi trường khác.

## Yêu Cầu

- Docker Engine hoặc Docker Desktop.
- Repository đã được checkout đầy đủ.
- Chạy các lệnh từ thư mục gốc của repository.
- Runtime container chính dùng PCAP offline và SQLite.

## Build Image

Build image runtime bằng `Dockerfile` multi-stage:

```sh
docker build -t passive-asset-discovery:latest .
```

Dockerfile sẽ:

- Dùng stage build để cài compiler, CMake, `pkg-config`, và `libpcap-dev`.
- Build binary C++; CMake bắt buộc tìm thấy `libpcap`.
- Dùng stage runtime nhẹ hơn với `libpcap0.8`, `libsqlite3-0`, Qt runtime dependency, và các binary PNAD.
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
mkdir -p .docker-data
chmod 777 .docker-data
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  -v "$PWD/.docker-data:/data" \
  -e SQLITE_DATABASE_PATH=/data/pnad.db \
  passive-asset-discovery:latest \
  --pcap /samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

JSON output:

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  -v "$PWD/.docker-data:/data" \
  -e SQLITE_DATABASE_PATH=/data/pnad.db \
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

Chạy demo PCAP với SQLite volume:

```sh
docker compose up --build pcap-demo
```

Service `pcap-demo` sẽ:

- Build image từ `Dockerfile`.
- Mount `./samples` vào `/samples:ro`.
- Chạy `asset-discovery` với `samples/multi-asset.pcap`.
- Ghi kết quả vào SQLite ở `/work/data/pnad.db` trong volume `pcap-demo-data`.

## Live Capture Trong Container

Live capture không còn là workflow đóng gói của CLI chính; image chạy PCAP offline bằng `--pcap`.

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
