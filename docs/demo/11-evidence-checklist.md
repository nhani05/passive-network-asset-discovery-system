# Phần 11: Checklist Bằng Chứng Demo (Evidence Checklist)

Tài liệu này tổng hợp danh sách các ảnh chụp màn hình (screenshots) hoặc log đầu ra cần chuẩn bị để làm bằng chứng (evidence) báo cáo bàn giao.

---

## Danh Sách Bằng Chứng Cần Thu Thập

| # | Tên Bằng Chứng (Evidence) | Lệnh Thực Hiện |
| :--- | :--- | :--- |
| **1** | Kết quả CTest chạy thành công 100% | `ctest --test-dir build --output-on-failure` |
| **2** | CLI Help Text trợ giúp | `./build/asset-discovery --help` |
| **3** | Đọc PCAP ARP đơn giản dạng Bảng | `./build/asset-discovery --pcap samples/arp.pcap --output table` |
| **4** | Đọc PCAP ARP đơn giản dạng JSON | `./build/asset-discovery --pcap samples/arp.pcap --output json` |
| **5** | Đọc PCAP ARP đơn giản dạng CSV | `./build/asset-discovery --pcap samples/arp.pcap --output csv` |
| **6** | Đọc PCAP đa tài sản (Multi-asset) dạng Bảng | `./build/asset-discovery --pcap samples/multi-asset.pcap --output table` |
| **7** | Đọc PCAP đa tài sản dạng JSON | `./build/asset-discovery --pcap samples/multi-asset.pcap --output json` |
| **8** | Đọc PCAP đa tài sản lọc BPF | `./build/asset-discovery --pcap samples/multi-asset.pcap --filter "arp or udp port 67 or udp port 68" --output table` |
| **9** | Phát hiện lỗi lọc BPF sai cú pháp | `./build/asset-discovery --pcap samples/arp.pcap --filter "invalid" --output table` |
| **10** | Phát hiện lỗi định dạng output không hỗ trợ | `./build/asset-discovery --pcap samples/arp.pcap --output xml` |
| **11** | Dữ liệu bảng `assets` và `asset_events` lưu trong DB | `psql "postgresql://..." -c "select ... from assets; select ... from asset_events;"` |
| **12** | Chạy đọc PCAP trong Docker container | `docker run --rm -v ... passive-asset-discovery --pcap /samples/arp.pcap --output table` |
| **13** | Trạng thái các dịch vụ Docker Compose | `docker compose ps` |
| **14** | Kết quả script xác thực Docker runtime | `scripts/verify-docker-runtime.sh` |
| **15** | Live Capture hoạt động thực tế trên host (hoặc log từ chối cờ cũ) | `sudo ./build/asset-discovery --interface eth0` |
| **16** | Kết quả đếm số dòng trong DB khi chạy lại lần 2 (Upsert) | `psql "..." -c "select count(*) from assets;"` |
