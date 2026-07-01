# Phần 3: Demo PCAP Offline

Tài liệu này hướng dẫn các kịch bản demo chạy offline sử dụng các file capture gói tin có sẵn (`samples/*.pcap`) để phân tích tài sản và sinh ra các định dạng đầu ra khác nhau.

---

## 1. Demo với file PCAP ARP Đơn Giản (`samples/arp.pcap`)

File `samples/arp.pcap` chỉ chứa 1 gói tin ARP request duy nhất từ địa chỉ MAC `02:42:ac:11:00:02` ánh xạ tới IP `192.168.1.10`.

### 1.1. Định dạng đầu ra Bảng (Table Output)

```bash
./build/asset-discovery --pcap samples/arp.pcap --output table
```

* **Kỳ vọng output:**
  ```text
  MAC                IPs                     Hostname          First Seen        Last Seen         Sources
  --------------------------------------------------------------------------------------------------------
  02:42:ac:11:00:02  192.168.1.10                              1699606784.0      1699606784.0      arp
  ```
  *(Hostname bị trống do giao thức ARP không mang thông tin tên thiết bị).*

### 1.2. Định dạng đầu ra JSON (JSON Output)

```bash
./build/asset-discovery --pcap samples/arp.pcap --output json
```

* **Kỳ vọng output:**
  ```json
  [
    {
      "mac_address": "02:42:ac:11:00:02",
      "ip_addresses": ["192.168.1.10"],
      "first_seen": "1699606784.0",
      "last_seen": "1699606784.0",
      "discovery_sources": ["arp"]
    }
  ]
  ```

### 1.3. Định dạng đầu ra CSV (CSV Output)

```bash
./build/asset-discovery --pcap samples/arp.pcap --output csv
```

* **Kỳ vọng output:**
  ```csv
  mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources
  02:42:ac:11:00:02,192.168.1.10,,1699606784.0,1699606784.0,arp
  ```

---

## 2. Demo với file PCAP Đa Thiết Bị (`samples/multi-asset.pcap`)

File `samples/multi-asset.pcap` chứa luồng gói tin phức tạp hơn, bao gồm cả gói tin ARP và DHCP từ nhiều thiết bị khác nhau.

### 2.1. Chạy xuất định dạng Bảng

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap --output table
```

* **Kỳ vọng output:**
  ```text
  MAC                IPs                     Hostname          First Seen        Last Seen         Sources
  --------------------------------------------------------------------------------------------------------
  02:42:ac:11:00:01  192.168.1.1                               1699606801.1000   1699606801.1000   arp
  02:42:ac:11:00:02  192.168.1.10,192.168.1.11                 1699606800.0      1699606807.7000   arp
  02:42:ac:11:00:03  192.168.1.20            laptop-user       1699606802.2000   1699606803.3000   arp,dhcp
  02:42:ac:11:00:04  192.168.1.30            camera-01         1699606804.4000   1699606804.4000   dhcp
  ```

* **Giải thích kết quả phân tích:**
  * Thiết bị `02:42:ac:11:00:01` chỉ được phát hiện qua giao thức ARP.
  * Thiết bị `02:42:ac:11:00:02` có 2 IP vì thiết bị này gửi gói tin ARP chứa các IP khác nhau tại các thời điểm khác nhau.
  * Thiết bị `02:42:ac:11:00:03` được phát hiện qua cả 2 nguồn ARP và DHCP, đồng thời trích xuất được hostname `laptop-user` từ DHCP Option 12.
  * Thiết bị `02:42:ac:11:00:04` chỉ được phát hiện từ luồng DHCP, thu được hostname `camera-01`.

### 2.2. Chạy xuất định dạng JSON

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap --output json
```

### 2.3. Chạy xuất định dạng CSV

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap --output csv
```

---

## 3. Thử Nghiệm BPF Filter Trên Gói Tin

Bộ lọc BPF cho phép lọc trực tiếp các gói tin ở tầng thấp trước khi đưa vào bộ phân tích.

### 3.1. Chỉ phân tích các gói tin ARP

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap --filter "arp" --output table
```

* **Kỳ vọng:** Bảng kết quả chỉ chứa các thiết bị được phát hiện từ ARP. Thiết bị `02:42:ac:11:00:04` (chỉ có trong DHCP) sẽ biến mất. Nguồn phát hiện của thiết bị `02:42:ac:11:00:03` chỉ còn lại `arp`.

### 3.2. Phân tích cả ARP và DHCP

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

* **Kỳ vọng:** Kết quả thu được đầy đủ cả 4 thiết bị tương tự khi không sử dụng bộ lọc.

---

## 4. Theo Dõi Sự Kiện Realtime (Event Replay NDJSON)

Trong lúc chương trình chạy phân tích PCAP, các sự kiện phát hiện thay đổi trạng thái của thiết bị (như phát hiện thiết bị mới, thiết bị đổi IP, thay đổi MAC) sẽ tự động in ra màn hình và ghi nhận vào file log NDJSON mặc định (`logs/events.ndjson`):

```bash
# Xóa file log cũ nếu có
rm -f logs/events.ndjson

# Chạy phân tích PCAP
./build/asset-discovery --pcap samples/multi-asset.pcap --output table

# Kiểm tra log sự kiện đã ghi nhận
cat logs/events.ndjson
```

* **Kỳ vọng:** File `logs/events.ndjson` chứa danh sách các JSON line mô tả sự kiện phát hiện thiết bị theo trình tự thời gian, ví dụ:
  ```json
  {"event_time":1699606800.0,"event_type":"new_asset","severity":"info","mac_address":"02:42:ac:11:00:02","ip_address":"192.168.1.10","discovery_source":"arp","message":"New asset discovered: ip=192.168.1.10 mac=02:42:ac:11:00:02 source=arp"}
  ```
