# Phần 9: Demo Xử Lý Lỗi & Xác Thực Tham Số

Tài liệu này hướng dẫn chạy các kịch bản demo kiểm chứng khả năng tự phát hiện lỗi, xác thực tham số đầu vào và cơ chế từ chối các cờ dòng lệnh cũ (đã bị loại bỏ) qua hệ thống custom exceptions và error boundary.

> [!NOTE]
> Với các lỗi xảy ra sau bước parse CLI như BPF sai hoặc file PCAP không tồn tại, hãy truyền `--sqlite <file>` hoặc đặt `SQLITE_DATABASE_PATH` trước. Runtime hiện yêu cầu SQLite path trước khi mở PCAP.

---

## 1. Lỗi Cú Pháp Bộ Lọc BPF (BPF Syntax Error)

Nếu truyền biểu thức lọc BPF sai cấu trúc, libpcap sẽ không thể dịch được:

```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --filter "invalid syntax" --output table
```

* **Kỳ vọng:** Chương trình kết thúc với mã lỗi 3 (`exit code = 3`) và thông báo lỗi rõ ràng:
  ```text
  [PCAP ERROR] invalid BPF filter for 'samples/arp.pcap': can't parse filter expression: syntax error
  ```

---

## 2. File PCAP Không Tồn Tại

```bash
./build/asset-discovery --pcap file-khong-ton-tai.pcap --sqlite pnad.db --output table
```

* **Kỳ vọng:** Chương trình kết thúc với mã lỗi 3 (`exit code = 3`) và thông báo lỗi rõ ràng:
  ```text
  [PCAP ERROR] could not open PCAP file 'file-khong-ton-tai.pcap': file-khong-ton-tai.pcap: No such file or directory
  ```

---

## 3. Thiếu Hoặc Xung Đột Nguồn Bắt Gói Tin (Input Source Validation)

Ứng dụng bắt buộc phải có `--pcap <file>`.

### 3.1. Không chỉ định nguồn:
```bash
./build/asset-discovery --sqlite pnad.db --output table
```
* **Kỳ vọng:** Báo lỗi yêu cầu cung cấp PCAP, exit với mã lỗi 2 (`exit code = 2`):
  ```text
  [CONFIG ERROR] provide input source: --pcap <file>
  ```

### 3.2. Dùng source live đã bị loại bỏ:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --interface eth0 --output table
```
* **Kỳ vọng:** Báo lỗi cờ cũ, exit với mã lỗi 2 (`exit code = 2`):
  ```text
  [CONFIG ERROR] --interface has been removed; use --pcap <file>
  ```

---

## 4. Kiểm Chứng Cơ Chế Từ Chối Cờ Cũ (Migration Rejections)

Khi nâng cấp CLI rút gọn, các cờ cũ nếu người dùng cố tình nhập sẽ bị chương trình phát hiện và báo lỗi hướng dẫn di chuyển rõ ràng kèm mã lỗi 2 (`exit code = 2`).

### 4.1. Từ chối cờ `--duration`:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --duration 60
```
* **Kỳ vọng:**
  ```text
  [CONFIG ERROR] --duration has been removed; capture uses PCAP files only
  ```

### 4.2. Từ chối cờ `--live`:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --live
```
* **Kỳ vọng:**
  ```text
  [CONFIG ERROR] --live has been removed; use --pcap <file>
  ```

### 4.3. Từ chối cờ `--idle-timeout`:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --idle-timeout 30
```
* **Kỳ vọng:**
  ```text
  [CONFIG ERROR] --idle-timeout has been removed; live capture no longer stops on idle timeout
  ```

### 4.4. Từ chối cờ `--max-assets`:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --max-assets 10
```
* **Kỳ vọng:**
  ```text
  [CONFIG ERROR] --max-assets has been removed; live capture no longer stops after an asset count
  ```

### 4.5. Từ chối cờ cấu hình sự kiện `--events`:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --events stdout
```
* **Kỳ vọng:**
  ```text
  [CONFIG ERROR] --events has been removed; realtime stdout events are enabled by default
  ```

---

## 5. Các Lỗi Định Dạng Khác

### 5.1. Định dạng đầu ra không hỗ trợ:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --output xml
```
* **Kỳ vọng:** Exit với mã lỗi 2 (`exit code = 2`):
  ```text
  [CONFIG ERROR] output format 'xml' is not supported; expected one of: table, json, csv
  ```

### 5.2. Tham số không nhận diện:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --verbose
```
* **Kỳ vọng:** Exit với mã lỗi 2 (`exit code = 2`):
  ```text
  [CONFIG ERROR] unknown argument '--verbose'
  ```

### 5.3. Bộ lọc `--filter` rỗng:
```bash
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --filter ""
```
* **Kỳ vọng:** Exit với mã lỗi 2 (`exit code = 2`):
  ```text
  [CONFIG ERROR] --filter cannot be empty
  ```

### 5.4. Dùng `--config` đã bị loại bỏ:
```bash
./build/asset-discovery --config configs/default.yaml --pcap samples/arp.pcap --sqlite pnad.db
```
* **Kỳ vọng:** Exit với mã lỗi 2 (`exit code = 2`):
  ```text
  [CONFIG ERROR] --config has been removed; configs/default.yaml is loaded automatically
  ```

### 5.5. Config chứa section capture không hợp lệ:
Nếu file YAML khai báo `capture`, chương trình sẽ từ chối vì capture cố định ở PCAP mode.

```yaml
capture:
  interface: eth0
```

* **Kỳ vọng:** Config load fail trước khi capture:
  ```text
  [CONFIG ERROR] <path>:1: section 'capture' is no longer supported
  ```
