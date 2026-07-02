# Hướng Dẫn Build, Deploy, Và Demo Chi Tiết (Mục Lục)

Tài liệu hướng dẫn demo đã được tổ chức lại và chia nhỏ thành **13 phần con** để giúp quá trình theo dõi, thực thi các kịch bản demo và kiểm chứng từng tính năng của hệ thống được dễ dàng, trực quan nhất.

---

## Danh Sách Các Phần Hướng Dẫn Demo

Vui lòng bấm vào liên kết của từng phần để xem hướng dẫn thực hiện chi tiết:

| Phần | Tên Tài Liệu Hướng Dẫn | Nội Dung Chính |
| :--- | :--- | :--- |
| **01** | [Phần 1: Chuẩn Bị Trước Demo](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/01-preparation.md) | Yêu cầu môi trường, kiểm tra công cụ, cấu hình biến môi trường kết nối PostgreSQL và cấu trúc CLI. |
| **02** | [Phần 2: Build Native Và Chạy Test](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/02-build-and-test.md) | Biên dịch native có/không có `libpcap`, chạy unit test bằng CTest, và kịch bản ép hiệu năng (Benchmark throughput/backends). |
| **03** | [Phần 3: Demo PCAP Offline](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/03-pcap-offline.md) | Chạy đọc file PCAP mẫu, kiểm chứng các định dạng đầu ra (Table, JSON, CSV), sử dụng bộ lọc BPF, và kiểm tra log NDJSON. |
| **04** | [Phần 4: PostgreSQL Local & Tích Hợp](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/04-postgres-local.md) | Cấu hình `.env` cho database, kiểm chứng cơ chế đồng bộ tự động, kiểm tra trùng lặp (Upsert), tự tạo bảng (Auto Schema). |
| **05** | [Phần 5: Demo Đóng Gói Docker Image](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/05-docker-image.md) | Biên dịch Docker image tối ưu (Multi-stage build), chạy thử nghiệm đọc PCAP bằng docker run, cấu hình mạng kết nối DB. |
| **06** | [Phần 6: Demo Triển Khai Với Docker Compose](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/06-docker-compose.md) | Khởi chạy dịch vụ demo PCAP tích hợp cơ sở dữ liệu tự động, kiểm tra DNS/Network nội bộ giữa các container, và cách thu dọn dữ liệu. |
| **07** | [Phần 7: Demo Live Capture Native](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/07-live-capture-native.md) | **[CỰC KỲ CHI TIẾT]** Kịch bản chạy Live Capture song song 2 Terminal: Terminal 1 capture, Terminal 2 ping/arping/dhclient sinh traffic mạng. |
| **08** | [Phần 8: Demo Live Capture Trong Docker](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/08-live-capture-docker.md) | Phân bổ quyền đặc trị mạng cho container (`--net=host`), chạy live qua Compose live profile, và lưu ý giới hạn Docker Desktop. |
| **09** | [Phần 9: Demo Xử Lý Lỗi & Xác Thực](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/09-error-handling.md) | Kiểm chứng phản hồi khi nhập sai cú pháp BPF, thiếu file, tham số xung đột, và cơ chế từ chối/báo lỗi di chuyển cho các cờ cũ. |
| **10** | [Phần 10: Demo CLI Help Menu](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/10-help-text.md) | Kiểm tra hiển thị thông tin trợ giúp sử dụng cờ `-h`/`--help` trên native và container. |
| **11** | [Phần 11: Checklist Bằng Chứng Demo](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/11-evidence-checklist.md) | Bảng tổng hợp 17 bằng chứng cần thu thập (screenshots/logs) trong quá trình thực hiện demo báo cáo. |
| **12** | [Phần 12: Hướng Dẫn Giải Quyết Sự Cố](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/12-troubleshooting.md) | Cách khắc phục các vấn đề phổ biến nhất về thư viện `libpcap`, quyền raw socket, kết nối DB cổng 5432, và drop gói tin. |
| **13** | [Phần 13: Tham Số CLI, Config Và Giá Trị Mặc Định](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/13-cli-parameters.md) | Giải thích đầy đủ CLI hiện tại, default values, YAML config/profile, precedence, env vars và các cờ đã remove. |

---

## Sơ Đồ Quy Trình Thực Hiện Demo Đề Xuất

Để có kết quả demo hoàn hảo nhất cho toàn bộ hệ thống, bạn nên đi theo trình tự:

```mermaid
graph TD
    A[01. Chuẩn Bị & Kiểm Tra Công Cụ] --> B[02. Build Native & Chạy CTest]
    B --> C[03. Phân Tích File PCAP Mẫu]
    C --> D[04. Khởi Động PostgreSQL & Đồng Bộ]
    D --> E[07. Chạy Live Capture Native + Terminal 2 Sinh Traffic]
    E --> F[05. Build Docker Image & Chạy Container]
    F --> G[06. Chạy Docker Compose Demo]
    G --> H[08. Chạy Live Capture Trong Docker]
    H --> I[13. Đối Chiếu CLI/Config Defaults]
    I --> J[11. Thu Thập Đầy Đủ 17 Bằng Chứng]
```
*(Nếu gặp bất kỳ lỗi nào trong quá trình thực hiện, vui lòng tra cứu nhanh tại **Phần 12: Troubleshooting**).*
