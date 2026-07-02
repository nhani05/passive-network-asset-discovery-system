# Phần 8: Demo Live Capture Trong Docker & Docker Compose

Tài liệu này hướng dẫn cách khởi chạy tính năng Live Capture (bắt gói tin trực tiếp từ interface card mạng host) bên trong môi trường Docker Container và thông qua Docker Compose Profiles.

---

## 1. Yêu Cầu Cấp Quyền Đặc Biệt Cho Docker Container

Để một ứng dụng chạy bên trong Docker Container có thể bắt được gói tin của card mạng thật trên host, container cần được cấp quyền quản trị mạng và sử dụng trực tiếp mạng của host (`host network`).

Các tham số bắt buộc phải truyền khi chạy `docker run`:

| Tham số | Ý nghĩa | Lý do bắt buộc |
| :--- | :--- | :--- |
| `--user 0:0` | Chạy container bằng tài khoản root | Việc mở raw socket yêu cầu quyền root trong container. |
| `--net=host` | Sử dụng network stack của máy host | Để container nhìn thấy các interface vật lý (`eth0`, `wlan0`,...) của host. |
| `--cap-add=NET_ADMIN` | Cấp capability quản trị mạng | Cho phép ứng dụng cấu hình card mạng sang chế độ Promiscuous Mode. |
| `--cap-add=NET_RAW` | Cấp capability raw socket | Bắt buộc để sử dụng backend `af-packet` và `pcap` bắt gói tin trực tiếp. |

---

## 2. Khởi Chạy Live Capture Bằng Lệnh `docker run`

Đảm bảo database PostgreSQL đang chạy trên host hoặc container khác ở cổng `5432`.

```bash
docker run --rm --user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  -e DB_HOST=localhost \
  -e DB_PORT=5432 \
  -e DB_NAME=asset_discovery \
  -e DB_USER=postgres \
  -e DB_PASSWORD=123456 \
  passive-asset-discovery \
  --profile live \
  --interface eth0
```

* **Thử nghiệm:** Sang Terminal 2 của máy host chạy ping hoặc arping để sinh traffic mạng (tương tự Phần 7).
* **Kết thúc:** Nhấn `Ctrl+C` tại terminal chạy docker để dừng graceful.

---

## 3. Khởi Chạy Bằng Docker Compose Live Profile

Chúng ta cấu hình sẵn profile tên `live` trong file `docker-compose.yml`. Dịch vụ này tự động mount biến môi trường và cấp các quyền đặc quyền cần thiết.

### 3.1. Chạy Live Capture mặc định (trên card mạng `eth0`):
```bash
CAPTURE_INTERFACE=eth0 docker compose --profile live run --rm live-capture
```

### 3.2. Chạy Live Capture trên card mạng tùy chọn (ví dụ: `enp4s0`):
```bash
CAPTURE_INTERFACE=enp4s0 docker compose --profile live run --rm live-capture
```

### 3.3. Chạy Live Capture với bộ lọc tùy chỉnh (ví dụ chỉ bắt ARP):
```bash
CAPTURE_INTERFACE=eth0 CAPTURE_FILTER="arp" docker compose --profile live run --rm live-capture
```

---

## 4. Các Biến Môi Trường Cấu Hình Live Profile

| Biến môi trường | Giá trị mặc định | Vai trò |
| :--- | :--- | :--- |
| `CAPTURE_INTERFACE` | `eth0` | Card mạng trên host dùng để capture |
| `CAPTURE_FILTER` | `arp or udp port 67 or udp port 68` | CLI override nhanh cho BPF filter; mặc định policy nằm trong `configs/live.yaml` |

---

## 5. Lưu Ý Quan Trọng Về Docker Desktop (macOS / Windows)

> [!WARNING]
> Tính năng Live Capture bên trong container **không hoạt động** trên Docker Desktop dành cho macOS hoặc Windows.
>
> **Lý do kỹ thuật:**
> * Docker Desktop chạy container bên trong một máy ảo Linux siêu nhẹ (Utility VM).
> * Khi khai báo `--net=host`, container sẽ chia sẻ network stack của **máy ảo Linux** đó, chứ không phải của hệ điều hành macOS hay Windows bên ngoài.
> * Các interface vật lý của macOS/Windows không hiển thị trong máy ảo này.
>
> **Giải pháp khắc phục:** Đối với macOS và Windows, hãy demo tính năng Live Capture bằng cách sử dụng **binary native chạy trực tiếp trên host** thay vì chạy trong container Docker.
