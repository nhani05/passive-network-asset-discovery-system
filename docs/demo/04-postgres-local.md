# Phần 4: Demo PostgreSQL Local & Tích Hợp Hệ Thống

Tài liệu này hướng dẫn cách khởi chạy cơ sở dữ liệu PostgreSQL local qua Docker Compose, cấu hình biến môi trường kết nối cho ứng dụng native và kiểm thử ghi nhận dữ liệu (tài sản và sự kiện) tự động.

---

## 1. Khởi Chạy Cơ Sở Dữ Liệu PostgreSQL

Chúng ta sử dụng dịch vụ database định nghĩa sẵn trong file `docker-compose.yml`:

```bash
# Khởi chạy dịch vụ db ở chế độ background
docker compose up -d db

# Kiểm tra trạng thái hoạt động của DB
docker compose ps db
```

* **Kỳ vọng:** Dịch vụ `db` có trạng thái `running (healthy)`.

---

## 2. Cấu Hình Biến Môi Trường Kết Nối

Ứng dụng của chúng ta tự động nạp cấu hình cơ sở dữ liệu từ file `.env` ở thư mục chạy. Hãy copy file mẫu hoặc tự tạo file `.env` với nội dung sau:

```bash
# Copy file môi trường ví dụ
cp .env.example .env
```

Nội dung cơ bản của file `.env` bao gồm:
```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
```

---

## 3. Khởi Chạy Và Ghi Nhận Tự Động (Tích hợp DB)

Khi file `.env` đã được cấu hình chính xác, bất kỳ phiên chạy phân tích pcap hay capture live nào cũng sẽ tự động đồng bộ tài sản và lịch sử sự kiện vào PostgreSQL.

```bash
# Khởi chạy phân tích PCAP
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

* **Kỳ vọng:** Chương trình chạy thành công, in bảng kết quả tổng hợp ra màn hình và đồng thời tự động ghi dữ liệu vào DB trong quá trình xử lý.

---

## 4. Truy Vấn Kiểm Chứng Dữ Liệu Bằng `psql`

Sử dụng `psql` kết nối vào database trong container để xem dữ liệu đã được lưu:

### 4.1. Kiểm tra danh sách tài sản (bảng `assets`):
```bash
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

* **Kỳ vọng:** Thấy đầy đủ thông tin của 4 thiết bị đã được phát hiện ở Phần 3.

### 4.2. Kiểm tra lịch sử sự kiện phát hiện (bảng `asset_events`):
```bash
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select event_time, event_type, severity, ip_address, mac_address from asset_events order by id;"
```

---

## 5. Kiểm Chứng Các Quy Tắc Đặc Biệt

### 5.1. Demo Cơ Chế Tránh Trùng Lặp (Upsert)

Bảng `assets` sử dụng địa chỉ MAC làm khóa chính độc nhất (`PRIMARY KEY`). Khi chạy lại phân tích, dữ liệu sẽ được cập nhật (upsert) chứ không sinh dòng mới bị trùng:

```bash
# Chạy lại chương trình lần 2
./build/asset-discovery --pcap samples/multi-asset.pcap --output table

# Đếm số dòng trong DB
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select count(*) from assets;"
```
* **Kỳ vọng:** Số bản ghi trong bảng `assets` vẫn duy trì là `4` dòng.

### 5.2. Demo Schema Tự Khởi Tạo (Auto Schema)

Hệ thống tự tạo bảng nếu chúng chưa tồn tại. Chúng ta sẽ giả lập tình huống database sạch:

```bash
# Xóa bảng cũ
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "drop table if exists asset_events; drop table if exists assets;"

# Chạy ứng dụng
./build/asset-discovery --pcap samples/arp.pcap --output table

# Kiểm tra lại cấu trúc bảng trong DB
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "\dt"
```
* **Kỳ vọng:** Bảng `assets` và `asset_events` đã tự động được dựng lại mà không gặp lỗi.

### 5.3. Từ Chối Cờ Cũ `--db-url`

Cờ cấu hình qua tham số dòng lệnh `--db-url` đã bị loại bỏ hoàn toàn để tránh rò rỉ thông tin đăng nhập trong log lệnh:

```bash
./build/asset-discovery --pcap samples/arp.pcap --db-url "postgresql://postgres:123456@localhost:5432/asset_discovery"
```
* **Kỳ vọng:** Chương trình lập tức báo lỗi và dừng:
  ```text
  [CONFIG ERROR] --db-url has been removed; configure PostgreSQL with .env, DATABASE_URL, PG*, or DB_* environment variables
  ```

---

## 6. Kiểm Chứng Lỗi Khi Thiếu PostgreSQL Configuration

Runtime hiện tại yêu cầu PostgreSQL configuration trước khi đọc PCAP hoặc mở live interface. Nếu không có `.env`, `DATABASE_URL`, `PG*`, hoặc `DB_*`, chương trình phải fail sớm để tránh chạy demo mà không lưu được asset/event history.

```bash
env -u DATABASE_URL \
  -u PGHOST -u PGPORT -u PGDATABASE -u PGUSER -u PGPASSWORD -u PGSERVICE \
  -u DB_HOST -u DB_PORT -u DB_NAME -u DB_USER -u DB_PASSWORD \
  ./build/asset-discovery --pcap samples/arp.pcap
```

* **Kỳ vọng:** Chương trình dừng trước khi đọc PCAP và báo lỗi:
  ```text
  [CONFIG ERROR] PostgreSQL configuration is required; set DATABASE_URL or PG*/DB_* values in .env or the process environment
  ```
