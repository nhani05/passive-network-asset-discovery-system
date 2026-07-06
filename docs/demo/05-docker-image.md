# Phần 5: Demo Đóng Gói Docker Image

Tài liệu này hướng dẫn build Docker image và chạy phân tích PCAP offline trong container.

## 1. Build Docker Image

```bash
docker build -t passive-asset-discovery .
```

Kỳ vọng:

- Image `passive-asset-discovery` được tạo thành công.
- Runtime image chỉ cần libpcap, SQLite runtime, Qt runtime cho GUI binary, và các binary PNAD.
- Không còn `postgresql-client` trong runtime image.

## 2. Chạy PCAP Offline Trong Container

Tạo thư mục lưu SQLite trên host:

```bash
mkdir -p .docker-data
chmod 777 .docker-data
```

### Đọc file PCAP ARP đơn giản

```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  -v "$PWD/.docker-data:/data" \
  -e SQLITE_DATABASE_PATH=/data/arp.db \
  passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --output table
```

### Đọc file PCAP đa thiết bị

```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  -v "$PWD/.docker-data:/data" \
  -e SQLITE_DATABASE_PATH=/data/multi.db \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --output json
```

### Áp dụng BPF filter

```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  -v "$PWD/.docker-data:/data" \
  -e SQLITE_DATABASE_PATH=/data/filter.db \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --filter "arp" \
  --output table
```

## 3. Docker Compose PCAP Demo

```bash
docker compose up --build pcap-demo
```

Kỳ vọng:

- Service `pcap-demo` chạy sample PCAP.
- Asset mới được in realtime ra stdout.
- SQLite database nằm trong volume `pcap-demo-data` tại `/work/data/pnad.db`.

## 4. Help Text Trong Docker

```bash
docker run --rm passive-asset-discovery --help
```
