# Phần 6: Demo Triển Khai Với Docker Compose

Tài liệu này hướng dẫn chạy thử nghiệm tích hợp toàn bộ hệ thống (App + Database PostgreSQL) sử dụng Docker Compose.

---

## 1. Kiểm Tra Cấu Hình Docker Compose File

Kiểm tra tính hợp lệ của cú pháp file cấu hình `docker-compose.yml`:

```bash
docker compose config
```

* **Kỳ vọng:** Output in ra cấu hình chuẩn hóa của các dịch vụ (`db`, `pcap-demo`, `live-capture`), không có thông báo lỗi cú pháp.

---

## 2. Khởi Chạy Demo Tích Hợp Đọc PCAP

Chúng ta khởi chạy service `pcap-demo`. Service này được cấu hình để đợi PostgreSQL khởi động và báo trạng thái `healthy` trước khi chạy phân tích file PCAP `samples/multi-asset.pcap` và ghi vào database.

```bash
# Khởi chạy dịch vụ demo PCAP
docker compose up --build pcap-demo
```

* **Giải thích quy trình hoạt động:**
  1. Docker Compose biên dịch ứng dụng từ Dockerfile nếu chưa biên dịch.
  2. Dịch vụ cơ sở dữ liệu `db` khởi chạy trước. Hệ thống thực hiện kiểm tra sức khỏe (healthcheck) PostgreSQL.
  3. Khi `db` sẵn sàng, container `pcap-demo` khởi chạy, tự động kết nối và khởi tạo bảng trong PostgreSQL.
  4. Phân tích file PCAP, in kết quả dạng bảng ra stdout của Compose và đồng bộ dữ liệu vào DB.
  5. Sau khi hoàn tất phân tích, container `pcap-demo` được giữ ở trạng thái chạy để nhà phát triển có thể kiểm tra.

---

## 3. Kiểm Tra Cơ Sở Dữ Liệu Và Kết Nối Mạng

Hãy mở một terminal mới để chạy các lệnh kiểm chứng sau:

### 3.1. Kiểm tra trạng thái của các Container
```bash
docker compose ps
```
* **Kỳ vọng:**
  * Dịch vụ `db` ở trạng thái `running (healthy)`.
  * Dịch vụ `pcap-demo` ở trạng thái `running` hoặc `exited (0)` tùy thuộc cấu hình giữ container.

### 3.2. Truy vấn dữ liệu đã lưu trong Database
```bash
docker compose exec db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

### 3.3. Kiểm tra cấu trúc bảng (Schema) tự động tạo
```bash
docker compose exec db psql -U postgres -d asset_discovery \
  -c "\d assets"
```

### 3.4. Kiểm tra khả năng phân giải DNS nội bộ giữa các Container
```bash
docker compose exec pcap-demo ping -c 3 db
```
* **Kỳ vọng:** Gói tin ping phản hồi thành công, chứng minh container `pcap-demo` phân giải và kết nối được với dịch vụ `db` thông qua tên DNS nội bộ.

---

## 4. Dừng Và Thu Dọn Tài Nguyên Demo

Khi demo kết thúc, hãy giải phóng tài nguyên hệ thống:

```bash
# Dừng và xóa các container, mạng nội bộ được tạo
docker compose down
```

Nếu muốn xóa sạch toàn bộ dữ liệu hiện có trong Database để chạy lại một kịch bản demo hoàn toàn mới:

```bash
# Dừng dịch vụ và xóa Docker Volume lưu trữ database
docker compose down -v
```

> [!CAUTION]
> Lệnh `docker compose down -v` sẽ xóa sạch dữ liệu PostgreSQL lưu trên ổ đĩa của host. Hãy cân nhắc trước khi thực hiện.
