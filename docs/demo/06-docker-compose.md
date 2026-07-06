# Phần 6: Demo Triển Khai Với Docker Compose

Tài liệu này hướng dẫn chạy Compose stack SQLite-only.

## 1. Kiểm Tra Cấu Hình

```bash
docker compose config
```

Kỳ vọng: Compose hợp lệ, gồm các service `pcap-demo` và `assetd`; không có service PostgreSQL.

## 2. Chạy PCAP Demo

```bash
docker compose up --build pcap-demo
```

Quy trình:

1. Docker Compose build image nếu cần.
2. `pcap-demo` mount `samples/` read-only.
3. `asset-discovery` phân tích `/samples/multi-asset.pcap`.
4. Event asset mới được in ra stdout.
5. Asset inventory được lưu vào SQLite tại `/work/data/pnad.db` trong volume `pcap-demo-data`.

## 3. Kiểm Tra Trạng Thái

```bash
docker compose ps
```

Kỳ vọng: `pcap-demo` đã chạy xong với exit code `0`.

## 4. Backend Service Với SQLite

```bash
docker compose --profile service up --build assetd
```

Service `assetd` dùng SQLite tại `/data/pnad.db` trong volume `assetd-data`.

## 5. Dừng Và Thu Dọn

```bash
docker compose down
```

Xóa luôn dữ liệu SQLite trong named volumes:

```bash
docker compose down -v
```
