# Phần 9: Demo Xử Lý Lỗi & Xác Thực Tham Số

Tài liệu này hướng dẫn chạy các kịch bản demo kiểm chứng khả năng tự phát hiện lỗi, xác thực tham số đầu vào và cơ chế từ chối các cờ dòng lệnh cũ (đã bị loại bỏ).

---

## 1. Lỗi Cú Pháp Bộ Lọc BPF (BPF Syntax Error)

Nếu truyền biểu thức lọc BPF sai cấu trúc, libpcap sẽ không thể dịch được:

```bash
./build/asset-discovery --pcap samples/arp.pcap --filter "invalid syntax" --output table
```

* **Kỳ vọng:** Chương trình kết thúc với mã lỗi (exit code ≠ 0) và thông báo lỗi rõ ràng:
  ```text
  error: invalid BPF filter for 'samples/arp.pcap': can't parse filter expression: syntax error
  ```

---

## 2. File PCAP Không Tồn Tại

```bash
./build/asset-discovery --pcap file-khong-ton-tai.pcap --output table
```

* **Kỳ vọng:** Chương trình báo lỗi không tìm thấy file hoặc không mở được file PCAP.

---

## 3. Thiếu Hoặc Xung Đột Nguồn Bắt Gói Tin (Input Source Validation)

Ứng dụng bắt buộc phải chọn duy nhất chế độ đọc PCAP hoặc capture Live.

### 3.1. Không chỉ định nguồn:
```bash
./build/asset-discovery --output table
```
* **Kỳ vọng:** Báo lỗi yêu cầu cung cấp đúng 1 nguồn:
  ```text
  error: provide exactly one input source: --pcap <file> or --interface <name>
  ```

### 3.2. Cung cấp cả hai nguồn cùng lúc:
```bash
./build/asset-discovery --pcap samples/arp.pcap --interface eth0 --output table
```
* **Kỳ vọng:** Báo lỗi tương tự:
  ```text
  error: provide exactly one input source: --pcap <file> or --interface <name>
  ```

---

## 4. Kiểm Chứng Cơ Chế Từ Chối Cờ Cũ (Migration Rejections)

Khi nâng cấp CLI rút gọn trong `simplify-live-runtime-defaults`, các cờ cũ nếu người dùng cố tình nhập sẽ bị chương trình phát hiện và báo lỗi hướng dẫn di chuyển rõ ràng.

### 4.1. Từ chối cờ `--duration`:
```bash
./build/asset-discovery --interface eth0 --duration 60
```
* **Kỳ vọng:**
  ```text
  error: --duration has been removed; live capture now runs until interrupted
  ```

### 4.2. Từ chối cờ `--live`:
```bash
./build/asset-discovery --interface eth0 --live
```
* **Kỳ vọng:**
  ```text
  error: --live is no longer required; --interface starts live capture
  ```

### 4.3. Từ chối cờ `--idle-timeout`:
```bash
./build/asset-discovery --interface eth0 --idle-timeout 30
```
* **Kỳ vọng:**
  ```text
  error: --idle-timeout has been removed; live capture no longer stops on idle timeout
  ```

### 4.4. Từ chối cờ `--max-assets`:
```bash
./build/asset-discovery --interface eth0 --max-assets 10
```
* **Kỳ vọng:**
  ```text
  error: --max-assets has been removed; live capture no longer stops after an asset count
  ```

### 4.5. Từ chối cờ cấu hình sự kiện `--events`:
```bash
./build/asset-discovery --pcap samples/arp.pcap --events stdout
```
* **Kỳ vọng:**
  ```text
  error: --events has been removed; realtime stdout events are enabled by default
  ```

---

## 5. Các Lỗi Định Dạng Khác

### 5.1. Định dạng đầu ra không hỗ trợ:
```bash
./build/asset-discovery --pcap samples/arp.pcap --output xml
```
* **Kỳ vọng:**
  ```text
  error: output format 'xml' is not supported; expected one of: table, json, csv
  ```

### 5.2. Tham số không nhận diện:
```bash
./build/asset-discovery --pcap samples/arp.pcap --verbose
```
* **Kỳ vọng:**
  ```text
  error: unknown argument '--verbose'
  ```

### 5.3. Bộ lọc `--filter` rỗng:
```bash
./build/asset-discovery --pcap samples/arp.pcap --filter ""
```
* **Kỳ vọng:**
  ```text
  error: --filter cannot be empty
  ```
