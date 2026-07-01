# Phần 7: Demo Live Capture Native Trên Host

Tài liệu này hướng dẫn chi tiết từng bước kịch bản chạy thử nghiệm bắt gói tin trực tiếp (Live Capture) trên máy host Linux. Quy trình sử dụng mô hình hai Terminal song song: một Terminal để capture gói tin và một Terminal để sinh traffic thử nghiệm.

---

## 1. Yêu Cầu Trước Khi Thực Hiện

* **Quyền hạn:** Live capture yêu cầu quyền tương tác trực tiếp với Network Interface Card (NIC) ở chế độ Promiscuous Mode. Bạn cần chạy lệnh bằng quyền `sudo` hoặc cấp quyền raw socket cho binary:
  ```bash
  sudo setcap cap_net_raw,cap_net_admin=eip ./build/asset-discovery
  ```
* **Cơ sở dữ liệu:** Demo này tích hợp lưu trữ tự động. Hãy đảm bảo database PostgreSQL đang chạy và file `.env` đã có sẵn ở thư mục gốc:
  ```bash
  # Khởi động DB nếu chưa chạy
  docker compose up -d db
  ```

---

## 2. Quy Trình Chạy Demo Chi Tiết (Kịch Bản 2 Terminal)

### Bước 1: Khởi Chạy Capture (Terminal 1)

Tìm tên interface mạng của bạn bằng lệnh `ip -br link` (ví dụ ở đây dùng `eth0`). Chạy lệnh sau ở Terminal 1:

```bash
# Khởi chạy live capture trên interface eth0, lọc ARP và DHCP
sudo ./build/asset-discovery --interface eth0 \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

* **Trạng thái kỳ vọng ban đầu:**
  * Terminal sẽ dừng lại và hiển thị log thông báo bắt đầu lắng nghe gói tin trên interface `eth0`.
  * Nếu mạng hiện tại đang tĩnh (không có máy nào gửi ARP hay xin DHCP), màn hình sẽ chưa xuất hiện log sự kiện nào.

---

### Bước 2: Tạo Giao Lộ Mạng / Sinh Traffic (Terminal 2)

Hãy mở một cửa sổ Terminal mới (Terminal 2) để chủ động tạo ra các gói tin ARP và DHCP. Hệ thống của chúng ta sẽ lập tức bắt được và phân tích chúng:

#### 1. Giả lập traffic ARP bằng lệnh Ping Gateway:
Lệnh này gửi gói tin ICMP tới IP gateway của mạng, buộc hệ điều hành phải thực hiện phân giải địa chỉ ARP (ARP Request & Reply):
```bash
ping -c 5 $(ip route | awk '/default/ {print $3}')
```

#### 2. Giả lập traffic ARP trực tiếp bằng công cụ `arping`:
Gửi gói tin ARP request trực tiếp tới một IP cụ thể trong mạng:
```bash
sudo arping -c 3 192.168.1.1
```

#### 3. Giả lập DHCP handshake bằng `dhclient`:
Yêu cầu cấp phát lại hoặc làm mới IP từ DHCP Server. Lệnh này sinh ra các gói tin DHCP Request/Discover chứa nhiều metadata:
```bash
sudo dhclient -v eth0
```

---

### Bước 3: Quan Sát Kết Quả Thời Gian Thực (Terminal 1)

Khi bạn thực hiện các lệnh tạo traffic ở Terminal 2, hãy nhìn lại màn hình **Terminal 1**.

* **Kỳ vọng hiển thị:**
  Các sự kiện phát hiện thiết bị sẽ xuất hiện liên tục trên màn hình theo thời gian thực dưới định dạng chuẩn:
  ```text
  1699606800.1234 INFO new_asset ip=192.168.1.50 mac=02:42:ac:11:00:22 protocol=arp iface=eth0
  1699606805.5678 INFO asset_reappeared ip=192.168.1.1 mac=02:42:ac:11:00:01 protocol=arp iface=eth0
  ```
  Đồng thời, mỗi sự kiện này cũng được ghi đè/nối thêm vào file log NDJSON ở đường dẫn `logs/events.ndjson`. Bạn có thể mở Terminal thứ 3 để giám sát file này:
  ```bash
  tail -f logs/events.ndjson
  ```

---

### Bước 4: Dừng Graceful Bằng Tổ Hợp Phím Ctrl+C

Khi bạn đã thu thập đủ bằng chứng demo, hãy nhấn tổ hợp phím **`Ctrl+C`** tại **Terminal 1** để dừng chương trình.

* **Quy trình shutdown an toàn (Graceful Stop):**
  1. Chương trình dừng nhận gói tin mới từ card mạng.
  2. Toàn bộ các gói tin còn sót lại trong hàng đợi (bounded queue) sẽ được Parser Workers giải quyết nốt.
  3. Bộ ghi dữ liệu (Writer thread) thực hiện lưu trữ toàn bộ các thay đổi tài sản và sự kiện chưa lưu vào PostgreSQL.
  4. In bảng tổng hợp các thiết bị phát hiện được trong suốt phiên chạy ra màn hình:
     ```text
     MAC                IPs                     Hostname          First Seen        Last Seen         Sources
     --------------------------------------------------------------------------------------------------------
     02:42:ac:11:00:01  192.168.1.1                               1699606800.0      1699606805.0      arp
     02:42:ac:11:00:22  192.168.1.50                              1699606800.0      1699606800.0      arp
     ```

---

## 3. Truy Vấn Database Để Xác Minh

Sau khi kết thúc, chạy các câu lệnh kiểm tra PostgreSQL:

```bash
# Truy vấn danh sách thiết bị lưu trữ cuối cùng
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, discovery_sources from assets;"

# Truy vấn lịch sử sự kiện (Event History Log)
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select event_type, severity, ip_address, mac_address, message from asset_events order by id;"
```

* **Kỳ vọng:** Dữ liệu trong DB trùng khớp hoàn toàn với bảng kết quả in ra terminal khi dừng.

---

## 4. Chọn Lựa Backend Live Capture (Tuning Performance)

Bạn có thể ép buộc hệ thống sử dụng một backend bắt gói tin cụ thể để so sánh:

* **Backend libpcap:** (Tương thích cao, chạy qua libpcap API)
  ```bash
  sudo ./build/asset-discovery --interface eth0 --capture-backend pcap
  ```
* **Backend AF_PACKET:** (Hiệu năng cao trên Linux, bỏ qua các bước copy dư thừa nhờ Ring Buffer)
  ```bash
  sudo ./build/asset-discovery --interface eth0 --capture-backend af-packet
  ```
  *(Lưu ý: af-packet yêu cầu quyền root hoàn toàn hoặc `CAP_NET_RAW`).*
