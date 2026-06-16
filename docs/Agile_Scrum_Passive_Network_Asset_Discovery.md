# Kế hoạch áp dụng Agile/Scrum cho dự án Passive Network Asset Discovery System

> Tài liệu này dùng như file kế hoạch phát triển phần mềm cho đề tài **Passive Network Asset Discovery System**.  
> Mục tiêu là giúp người thực hiện hiểu rõ: **cần làm gì, làm như thế nào, dùng công cụ nào và tại sao phải làm từng công việc** khi áp dụng Agile/Scrum trong một dự án cá nhân.

---

## 1. Tổng quan dự án

### 1.1. Tên dự án

**Passive Network Asset Discovery System**

### 1.2. Mục tiêu sản phẩm

Xây dựng một hệ thống giám sát mạng thụ động có khả năng phân tích lưu lượng mạng để phát hiện các thiết bị đang hoạt động trong mạng nội bộ.

Hệ thống cần:

- Đọc traffic từ **network interface** hoặc **file PCAP**.
- Phát hiện asset dựa trên địa chỉ **IP/MAC**.
- Ghi nhận thời điểm asset xuất hiện lần đầu và lần cuối:
  - `first_seen`
  - `last_seen`
- Phân tích các protocol discovery cơ bản:
  - ARP
  - DHCP
- Thu thập metadata cơ bản:
  - IP
  - MAC
  - hostname
  - vendor nếu có thể mở rộng
  - nguồn phát hiện: ARP/DHCP/PCAP/live interface
- Đóng gói và triển khai được bằng Docker.
- Có source code hoàn chỉnh, tài liệu thiết kế, hướng dẫn build/deploy và demo.

### 1.3. Bản chất của hệ thống

Đây là một hệ thống **passive monitoring**, nghĩa là hệ thống chỉ quan sát traffic đi qua mạng, không chủ động scan, ping hoặc gửi packet dò quét.

Điểm khác biệt:

| Cách tiếp cận | Đặc điểm |
|---|---|
| Active scanning | Chủ động gửi request để tìm thiết bị. Ví dụ: ping sweep, port scan. |
| Passive discovery | Chỉ lắng nghe traffic sẵn có trong mạng để phát hiện thiết bị. |

Vì đề tài yêu cầu **passive**, hệ thống nên tránh các hành vi như:

- Ping toàn mạng.
- Scan port.
- Gửi ARP request chủ động hàng loạt.
- Gửi DHCP request giả lập vào mạng thật.

---

## 2. Tại sao nên dùng Agile/Scrum cho dự án này?

Dự án này có nhiều phần chưa chắc chắn ngay từ đầu:

- Chưa biết chọn thư viện bắt packet nào là phù hợp nhất.
- Chưa biết dữ liệu ARP/DHCP trong file PCAP có đủ rõ không.
- Chưa biết Docker cần quyền gì để capture live interface.
- Chưa biết nên lưu asset vào SQLite, PostgreSQL, JSON hay file log.
- Chưa biết demo cuối nên chạy bằng PCAP hay live interface.

Vì vậy, nếu làm theo kiểu waterfall, chờ thiết kế hoàn chỉnh rồi mới code, dự án dễ bị chậm.

Agile/Scrum phù hợp vì:

| Lý do | Ý nghĩa trong dự án |
|---|---|
| Chia nhỏ công việc | Mỗi tuần hoàn thành một phần có thể chạy được. |
| Có demo liên tục | Sau mỗi sprint đều có kết quả kiểm chứng. |
| Dễ thay đổi công nghệ | Nếu thư viện bắt packet không phù hợp, có thể đổi sớm. |
| Giảm rủi ro | Rủi ro Docker/live capture/parse DHCP được xử lý từng bước. |
| Phù hợp làm cá nhân | Một người vẫn có thể dùng Scrum dưới dạng Scrum cá nhân. |

---

## 3. Cách hiểu Scrum khi chỉ có một người làm

Trong Scrum chuẩn, thường có các vai trò:

- Product Owner
- Scrum Master
- Developers
- Tester/QA
- DevOps
- Technical Writer

Với đề tài cá nhân, bạn đóng tất cả các vai trò này.

### 3.1. Mapping vai trò Scrum vào dự án cá nhân

| Vai trò | Khi làm nhóm | Khi bạn làm một mình | Việc cần làm |
|---|---|---|---|
| Product Owner | Xác định sản phẩm cần gì | Bạn tự xác định scope, ưu tiên feature | Viết backlog, chọn tính năng quan trọng trước |
| Scrum Master | Theo dõi quy trình Scrum | Bạn tự kiểm soát tiến độ | Daily self-check, sprint review, sprint retrospective |
| Developer | Code tính năng | Bạn tự thiết kế và lập trình | Capture packet, parse ARP/DHCP, lưu asset |
| QA/Tester | Kiểm thử | Bạn tự viết test và test thủ công | Unit test, test bằng PCAP, test Docker |
| DevOps | Build/deploy | Bạn tự đóng gói Docker | Dockerfile, docker-compose, README deploy |
| Technical Writer | Viết tài liệu | Bạn tự viết báo cáo kỹ thuật | Design document, demo guide, architecture |

### 3.2. Nguyên tắc làm việc khi một người đóng nhiều vai

Mỗi ngày bạn nên tự hỏi 3 câu giống Daily Scrum:

1. Hôm qua đã làm được gì?
2. Hôm nay cần làm gì?
3. Có vấn đề gì đang cản trở không?

Ví dụ:

```md
Ngày: 2026-06-xx

Hôm qua:
- Đã đọc được packet từ file PCAP.
- Đã in ra thông tin MAC/IP từ ARP packet.

Hôm nay:
- Tạo struct Asset.
- Lưu asset vào SQLite.

Blocker:
- Chưa parse được DHCP hostname.
```

---

## 4. Product Vision

### 4.1. Tuyên bố sản phẩm

**Passive Network Asset Discovery System** là một công cụ giúp phát hiện thiết bị trong mạng nội bộ bằng cách phân tích traffic thụ động từ file PCAP hoặc network interface, từ đó cung cấp danh sách asset với thông tin IP, MAC, thời điểm xuất hiện và metadata từ ARP/DHCP.

### 4.2. Người dùng mục tiêu

| Người dùng | Nhu cầu |
|---|---|
| Sinh viên thực hiện đề tài | Có demo rõ ràng, có báo cáo kỹ thuật, có Docker deploy |
| Giảng viên/chấm điểm | Kiểm tra hệ thống có phát hiện asset từ traffic hay không |
| Đơn vị an toàn thông tin/network team | Quan sát thiết bị xuất hiện trong mạng mà không cần scan chủ động |
| Developer hệ thống | Có kiến trúc đủ rõ để mở rộng thêm protocol sau này |

### 4.3. Giá trị sản phẩm

Hệ thống giúp:

- Tạo inventory thiết bị mạng ở mức cơ bản.
- Phát hiện thiết bị mới xuất hiện trong mạng.
- Theo dõi thời điểm thiết bị bắt đầu/ngừng xuất hiện.
- Hỗ trợ phân tích bảo mật mạng nội bộ.
- Là nền tảng để mở rộng sang phát hiện bất thường sau này.

---

## 5. Product Goal

Sau khi hoàn thành dự án, hệ thống cần đạt được:

```md
Một hệ thống chạy được bằng Docker, có thể đọc traffic từ file PCAP hoặc network interface,
phân tích ARP/DHCP để phát hiện asset, lưu thông tin asset và hiển thị danh sách asset
với IP, MAC, hostname, first_seen, last_seen.
```

---

## 6. Đề xuất kiến trúc kỹ thuật

### 6.1. Kiến trúc tổng thể

```text
+-------------------------+
| Input Source            |
| - PCAP file             |
| - Network interface     |
+------------+------------+
             |
             v
+-------------------------+
| Packet Capture Layer    |
| - Read packet           |
| - Normalize timestamp   |
+------------+------------+
             |
             v
+-------------------------+
| Protocol Parser Layer   |
| - ARP Parser            |
| - DHCP Parser           |
+------------+------------+
             |
             v
+-------------------------+
| Asset Discovery Engine  |
| - Identify asset        |
| - Update first_seen     |
| - Update last_seen      |
| - Merge metadata        |
+------------+------------+
             |
             v
+-------------------------+
| Storage Layer           |
| - SQLite/PostgreSQL     |
| - JSON/CSV export       |
+------------+------------+
             |
             v
+-------------------------+
| Output / CLI / API      |
| - List assets           |
| - Show detail           |
| - Export result         |
+-------------------------+
```

### 6.2. Các module chính

| Module | Công việc | Tại sao cần |
|---|---|---|
| Capture Module | Đọc packet từ PCAP hoặc interface | Đây là đầu vào của toàn bộ hệ thống |
| ARP Parser | Lấy IP/MAC từ ARP packet | ARP là protocol quan trọng để phát hiện thiết bị LAN |
| DHCP Parser | Lấy hostname, MAC, IP lease nếu có | DHCP cung cấp metadata hữu ích cho asset |
| Asset Engine | Gom packet thành asset duy nhất | Tránh mỗi packet tạo ra một thiết bị trùng lặp |
| Storage | Lưu asset đã phát hiện | Cần dữ liệu để xem lại, demo và báo cáo |
| CLI/API | Hiển thị danh sách asset | Người dùng cần cách tương tác với hệ thống |
| Docker | Đóng gói môi trường chạy | Đảm bảo dễ deploy, dễ demo |
| Documentation | Mô tả thiết kế và hướng dẫn chạy | Đây là yêu cầu đầu ra của đề tài |

---

## 7. Tech stack đề xuất

Vì đề tài liên quan đến system programming và xử lý packet, nên ưu tiên C++.

### 7.1. Lựa chọn chính

| Thành phần | Công nghệ đề xuất | Lý do |
|---|---|---|
| Ngôn ngữ chính | C++17/C++20 | Hiệu năng tốt, phù hợp xử lý packet |
| Build system | CMake | Chuẩn phổ biến cho dự án C++ |
| Packet capture | libpcap hoặc PcapPlusPlus | Hỗ trợ đọc PCAP và capture interface |
| Storage đơn giản | SQLite | Nhẹ, dễ demo, không cần server riêng |
| Storage mở rộng | PostgreSQL | Phù hợp nếu muốn hệ thống lớn hơn |
| CLI | argparse/cxxopts hoặc tự xử lý argv | Dễ demo bằng terminal |
| Logging | spdlog hoặc logging tự viết | Theo dõi quá trình capture/parse |
| Unit test | GoogleTest hoặc Catch2 | Kiểm thử parser và asset engine |
| Container | Docker, docker-compose | Đóng gói và triển khai |
| Network analysis | Wireshark, tcpdump | Tạo/kiểm tra file PCAP |
| Quản lý task | GitHub Projects hoặc Notion Kanban | Theo dõi backlog/sprint |
| Version control | Git, GitHub | Quản lý source code |
| Tài liệu | Markdown, Mermaid, README.md | Dễ viết báo cáo và tài liệu kỹ thuật |

### 7.2. Vì sao không nên bắt đầu quá phức tạp?

Không nên bắt đầu ngay với:

- Web dashboard phức tạp.
- Kafka/Elasticsearch.
- Database phân tán.
- Realtime streaming UI.

Lý do:

- Đề tài tối thiểu cần phát hiện asset và Docker.
- Làm quá rộng sẽ tăng rủi ro không hoàn thành.
- Nên ưu tiên core engine trước, UI sau.

---

## 8. Scrum Artifacts cần tạo

### 8.1. Product Backlog

Product Backlog là danh sách toàn bộ việc cần làm cho sản phẩm.

Nên lưu trong:

- Notion Kanban
- GitHub Projects
- Markdown file: `docs/plans/product-backlog.md`

Các cột đề xuất:

| Cột | Ý nghĩa |
|---|---|
| Backlog | Việc chưa làm |
| Ready | Việc đã đủ rõ để làm |
| In Progress | Đang làm |
| Review/Test | Đã code, đang test |
| Done | Hoàn thành |

### 8.2. Sprint Backlog

Sprint Backlog là danh sách việc chọn làm trong một sprint.

Ví dụ sprint 1:

```md
Sprint 1 Goal:
Đọc được packet từ PCAP và phát hiện asset từ ARP.

Sprint Backlog:
- Tạo project C++ với CMake.
- Tích hợp thư viện đọc PCAP.
- Parse ARP packet.
- Tạo Asset struct.
- In danh sách asset ra console.
```

### 8.3. Increment

Increment là phần sản phẩm chạy được sau mỗi sprint.

Ví dụ:

| Sprint | Increment |
|---|---|
| Sprint 1 | CLI đọc PCAP và phát hiện asset từ ARP |
| Sprint 2 | Parse DHCP và lưu asset vào SQLite |
| Sprint 3 | Chạy được Docker và capture live interface |
| Sprint 4 | Hoàn thiện demo, tài liệu, test, báo cáo |

### 8.4. Definition of Ready

Một task được coi là Ready khi:

- Mục tiêu rõ ràng.
- Có input/output cụ thể.
- Biết file/module cần sửa.
- Có tiêu chí hoàn thành.
- Có dữ liệu test nếu cần.

Ví dụ task chưa Ready:

```md
Làm phần DHCP.
```

Task Ready:

```md
Implement DHCP parser để lấy hostname từ DHCP Option 12 trong packet DHCP Discover/Request.
Input: packet UDP port 67/68.
Output: hostname, client MAC, requested IP nếu có.
Acceptance Criteria:
- Parse được hostname từ file sample_dhcp.pcap.
- Có unit test cho packet có hostname và packet không có hostname.
```

### 8.5. Definition of Done

Một task được coi là Done khi:

- Code chạy được.
- Không làm hỏng chức năng cũ.
- Có test hoặc bằng chứng test thủ công.
- Có log hoặc output kiểm chứng.
- Đã commit Git.
- Nếu là feature chính thì đã cập nhật README/tài liệu.
- Nếu liên quan Docker thì đã test trong container.

---

## 9. Product Backlog chi tiết

### 9.1. Epic 1: Khởi tạo dự án và môi trường phát triển

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E1-T1 | Tạo repository và cấu trúc thư mục | Tạo repo GitHub, chia thư mục `src`, `include`, `tests`, `docs`, `docker`, `samples` | Git, GitHub | Giúp dự án rõ ràng, dễ quản lý |
| E1-T2 | Tạo project CMake | Viết `CMakeLists.txt`, build thử chương trình hello world | CMake, compiler C++ | Là nền tảng để build source code |
| E1-T3 | Chọn thư viện capture packet | So sánh libpcap và PcapPlusPlus, chọn một thư viện chính | libpcap/PcapPlusPlus | Ảnh hưởng trực tiếp đến cách đọc packet |
| E1-T4 | Tạo README ban đầu | Ghi mục tiêu, cách build, cách chạy cơ bản | Markdown | Người khác có thể hiểu dự án nhanh |
| E1-T5 | Tạo Dockerfile skeleton | Container build được project, chưa cần đầy đủ feature | Docker | Kiểm tra sớm khả năng đóng gói |

### 9.2. Epic 2: Đọc packet từ PCAP

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E2-T1 | Đọc file PCAP từ command line | CLI nhận tham số `--pcap samples/test.pcap` | C++, libpcap/PcapPlusPlus | PCAP là input dễ demo và test nhất |
| E2-T2 | In thông tin packet cơ bản | In timestamp, protocol, length | Logging/console | Kiểm tra chương trình đọc packet đúng |
| E2-T3 | Xử lý lỗi file không tồn tại | Nếu file sai thì báo lỗi rõ ràng | C++ exception/error code | Tăng độ ổn định |
| E2-T4 | Tạo sample PCAP | Dùng tcpdump/Wireshark tạo file chứa ARP/DHCP | tcpdump, Wireshark | Cần dữ liệu test cố định |
| E2-T5 | Viết test chạy với PCAP mẫu | Chạy CLI và kiểm tra output | shell script, CTest | Đảm bảo tính năng không bị hỏng |

### 9.3. Epic 3: Parse ARP để phát hiện asset

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E3-T1 | Nhận diện ARP packet | Kiểm tra EtherType `0x0806` | C++ parser | ARP là protocol chính để biết IP/MAC |
| E3-T2 | Lấy sender IP/MAC | Đọc trường sender protocol address và sender hardware address | ARP header parser | Đây là thông tin asset cơ bản |
| E3-T3 | Lấy target IP/MAC nếu hợp lệ | Parse target fields | ARP parser | Một số packet cho thêm thông tin thiết bị khác |
| E3-T4 | Tạo mô hình Asset | Struct/class gồm MAC, IP list, first_seen, last_seen, hostname | C++ class | Cần object trung tâm để gom thông tin |
| E3-T5 | Cập nhật asset từ ARP | Nếu MAC đã có thì update IP/last_seen, nếu chưa có thì tạo mới | AssetDiscoveryEngine | Tránh trùng lặp asset |
| E3-T6 | Hiển thị danh sách asset | CLI command `list-assets` hoặc in cuối chương trình | Console/JSON output | Cần output để demo |

### 9.4. Epic 4: Parse DHCP để bổ sung metadata

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E4-T1 | Nhận diện DHCP packet | Kiểm tra UDP port 67/68 | UDP parser | DHCP chứa hostname và IP lease |
| E4-T2 | Parse DHCP header | Lấy `chaddr`, `yiaddr`, `ciaddr` | DHCP parser | Xác định client MAC/IP |
| E4-T3 | Parse DHCP options | Đọc option 12 hostname, option 50 requested IP, option 53 message type | DHCP option parser | Metadata quan trọng nằm trong options |
| E4-T4 | Gộp DHCP metadata vào asset | Dùng MAC làm khóa chính, update hostname/IP | Asset Engine | Kết hợp nhiều nguồn thông tin |
| E4-T5 | Xử lý packet thiếu hostname | Nếu không có hostname thì bỏ qua field đó | Defensive coding | Traffic thực tế không phải lúc nào đầy đủ |
| E4-T6 | Test với PCAP DHCP mẫu | Dùng file có DHCP Discover/Request/Offer | Wireshark, test script | Đảm bảo parser hoạt động đúng |

### 9.5. Epic 5: Asset Discovery Engine

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E5-T1 | Thiết kế khóa định danh asset | Ưu tiên MAC làm primary key, IP là thuộc tính có thể thay đổi | C++ model | IP có thể đổi qua DHCP, MAC ổn định hơn |
| E5-T2 | Quản lý first_seen | Khi asset mới xuất hiện thì set timestamp đầu tiên | timestamp từ packet | Biết asset bắt đầu xuất hiện khi nào |
| E5-T3 | Quản lý last_seen | Mỗi lần thấy packet mới thì update last_seen | timestamp từ packet | Biết asset còn hoạt động gần đây không |
| E5-T4 | Gộp nhiều IP cho một MAC | Dùng set/list IP history | STL container | Một thiết bị có thể đổi IP |
| E5-T5 | Tránh duplicate asset | Kiểm tra MAC trước khi tạo mới | map/unordered_map | Dữ liệu sạch hơn |
| E5-T6 | Tính trạng thái asset | Nếu quá thời gian không thấy thì coi là inactive | configurable timeout | Hỗ trợ theo dõi thiết bị ngừng xuất hiện |

### 9.6. Epic 6: Storage và export dữ liệu

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E6-T1 | Chọn storage mặc định | Dùng SQLite cho bản demo | SQLite | Nhẹ, dễ chạy trong Docker |
| E6-T2 | Thiết kế schema bảng `assets` | Gồm MAC, IP, hostname, first_seen, last_seen, source | SQL | Lưu dữ liệu có cấu trúc |
| E6-T3 | Lưu asset vào database | Insert nếu mới, update nếu đã có | SQLite C/C++ API | Dữ liệu không mất khi chương trình kết thúc |
| E6-T4 | Export JSON/CSV | CLI option `--export assets.json` | JSON library/CSV writer | Dễ đưa vào báo cáo/demo |
| E6-T5 | Migration database | Tạo script `schema.sql` | SQL file | Người khác dễ tạo DB |
| E6-T6 | Test dữ liệu lưu đúng | Chạy PCAP mẫu rồi query DB | sqlite3 CLI | Đảm bảo kết quả đáng tin cậy |

### 9.7. Epic 7: CLI và trải nghiệm sử dụng

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E7-T1 | Thiết kế CLI command | Ví dụ `asset-discovery --pcap file.pcap` | cxxopts/argv | Người dùng dễ chạy |
| E7-T2 | Thêm mode live capture | Ví dụ `--interface eth0` | libpcap | Đáp ứng yêu cầu traffic thực tế |
| E7-T3 | Thêm output JSON | Ví dụ `--output json` | nlohmann/json | Dễ demo, dễ tích hợp |
| E7-T4 | Thêm log level | `--verbose`, `--quiet` | logging | Dễ debug |
| E7-T5 | Viết help command | `--help` hiển thị hướng dẫn | CLI parser | Người dùng không cần đọc code |

Ví dụ CLI mục tiêu:

```bash
./asset-discovery --pcap samples/arp_dhcp.pcap --output table
./asset-discovery --pcap samples/arp_dhcp.pcap --output json
./asset-discovery --interface eth0 --duration 60
```

### 9.8. Epic 8: Docker và deployment

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E8-T1 | Viết Dockerfile | Build project C++ trong image | Docker | Đáp ứng yêu cầu triển khai Docker |
| E8-T2 | Multi-stage build | Stage build và stage runtime riêng | Docker | Image gọn hơn |
| E8-T3 | Mount thư mục PCAP | Dùng volume `/samples` | Docker volume | Chạy demo bằng file PCAP |
| E8-T4 | Chạy live interface trong Docker | Thêm quyền `NET_ADMIN`, `NET_RAW`, hoặc host network nếu cần | docker run, compose | Capture live thường cần quyền mạng |
| E8-T5 | Viết docker-compose | Cấu hình app + volume + network mode | docker-compose | Người khác chạy dễ hơn |
| E8-T6 | Test Docker end-to-end | Build image, chạy PCAP mẫu, kiểm tra output | Docker, shell script | Đảm bảo demo ổn định |

Ví dụ chạy Docker với PCAP:

```bash
docker build -t passive-asset-discovery .
docker run --rm -v ./samples:/samples passive-asset-discovery \
  --pcap /samples/arp_dhcp.pcap --output table
```

Ví dụ chạy live capture:

```bash
docker run --rm --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery --interface eth0 --duration 60
```

### 9.9. Epic 9: Testing và kiểm thử

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E9-T1 | Unit test ARP parser | Test packet ARP mẫu | GoogleTest/Catch2 | Parser sai thì asset sai |
| E9-T2 | Unit test DHCP option parser | Test option hostname, requested IP | GoogleTest/Catch2 | DHCP options dễ parse sai |
| E9-T3 | Test Asset Engine | Test update first_seen/last_seen | Unit test | Logic gom asset là cốt lõi |
| E9-T4 | Integration test PCAP | Chạy toàn bộ app với PCAP mẫu | CTest/shell script | Đảm bảo pipeline hoạt động |
| E9-T5 | Docker test | Build và run container | Docker | Đảm bảo deploy được |
| E9-T6 | Manual test bằng Wireshark | So sánh output app với Wireshark | Wireshark | Kiểm chứng kết quả khách quan |

### 9.10. Epic 10: Documentation và demo

| ID | User Story / Task | Làm như thế nào | Công cụ | Tại sao cần làm |
|---|---|---|---|---|
| E10-T1 | Viết README | Mô tả project, build, run, example output | Markdown | Người khác biết dùng hệ thống |
| E10-T2 | Viết design document | Mô tả kiến trúc, module, data flow | Markdown, Mermaid | Đáp ứng yêu cầu tài liệu thiết kế |
| E10-T3 | Viết deployment guide | Hướng dẫn Docker build/run | Markdown | Đáp ứng yêu cầu build/deploy |
| E10-T4 | Chuẩn bị demo script | Các bước demo PCAP và live interface | Markdown | Tránh lỗi khi thuyết trình |
| E10-T5 | Chuẩn bị hình ảnh minh họa | Screenshot CLI, Docker, Wireshark | Screenshot tool | Làm báo cáo trực quan |
| E10-T6 | Viết limitations/future work | Nêu giới hạn và hướng phát triển | Markdown | Báo cáo có tính học thuật hơn |

---

## 10. Kế hoạch Sprint đề xuất

Giả sử bạn có khoảng 4 tuần, nên chia thành 4 sprint, mỗi sprint 1 tuần. Nếu thời gian ít hơn, vẫn giữ thứ tự ưu tiên: core trước, Docker và tài liệu sau.

---

## 11. Sprint 0: Chuẩn bị dự án

### 11.1. Sprint Goal

Thiết lập môi trường làm việc, repository, cấu trúc thư mục và backlog ban đầu.

### 11.2. Công việc cần làm

| Task | Làm như thế nào | Công cụ | Tại sao cần |
|---|---|---|---|
| Tạo repo GitHub | Tạo repository mới, branch `main` và `develop` | GitHub, Git | Quản lý source code |
| Tạo cấu trúc thư mục | Tạo `src`, `include`, `tests`, `docs`, `samples`, `docker` | File system | Dự án rõ ràng |
| Tạo project CMake | Build được chương trình C++ rỗng | CMake, g++/clang/MSVC | Kiểm tra môi trường build |
| Tạo Notion/GitHub Project | Tạo board Scrum/Kanban | Notion/GitHub Projects | Theo dõi tiến độ |
| Chọn thư viện packet | Test nhanh libpcap/PcapPlusPlus | C++ package manager | Giảm rủi ro kỹ thuật |
| Tạo sample PCAP | Capture ARP/DHCP hoặc tải sample hợp lệ | Wireshark/tcpdump | Có dữ liệu test |

### 11.3. Output cuối sprint

- Repo chạy được.
- Build được chương trình C++.
- Có backlog ban đầu.
- Có ít nhất 1 file PCAP mẫu.
- Có README skeleton.

### 11.4. Checklist Sprint Review

- Chạy được `cmake -S . -B build`.
- Chạy được `cmake --build build`.
- Chạy được binary demo hello world.
- Có board quản lý task.
- Có sample PCAP trong `samples/`.

### 11.5. Retrospective

Tự trả lời:

- Môi trường build có vấn đề gì?
- Thư viện packet có dễ dùng không?
- Có cần đổi hướng công nghệ sớm không?

---

## 12. Sprint 1: Đọc PCAP và phát hiện asset từ ARP

### 12.1. Sprint Goal

Chương trình đọc được file PCAP, parse được ARP packet và phát hiện asset dựa trên IP/MAC.

### 12.2. Công việc cần làm

| Task | Làm như thế nào | Công cụ | Tại sao cần |
|---|---|---|---|
| Implement PCAP reader | Mở file PCAP, duyệt từng packet | libpcap/PcapPlusPlus | Đây là input chính |
| Nhận diện ARP | Check EtherType ARP | C++ parser | ARP giúp lấy IP/MAC |
| Parse sender IP/MAC | Lấy trường sender IP/MAC trong ARP | ARP header | Thông tin asset cơ bản |
| Tạo Asset model | Class/struct `Asset` | C++ | Lưu thông tin thiết bị |
| Tạo Asset Engine | Dùng MAC làm key, update IP/time | STL unordered_map | Gom packet thành asset |
| In asset ra console | Output bảng đơn giản | CLI | Có demo sớm |
| Test với PCAP mẫu | Chạy app với file ARP | Wireshark, test script | Kiểm chứng kết quả |

### 12.3. Output cuối sprint

Chạy được lệnh:

```bash
./asset-discovery --pcap samples/arp.pcap
```

Output kỳ vọng:

```text
MAC                IP              FIRST_SEEN           LAST_SEEN
aa:bb:cc:dd:ee:ff  192.168.1.10    2026-06-xx 10:01     2026-06-xx 10:05
```

### 12.4. Acceptance Criteria

- Đọc được file PCAP hợp lệ.
- Bỏ qua packet không phải ARP mà không crash.
- Parse được ít nhất sender IP và sender MAC.
- Nếu cùng MAC xuất hiện nhiều lần, chỉ tạo một asset.
- `last_seen` được cập nhật theo packet mới nhất.
- Có README hướng dẫn chạy mode PCAP.

### 12.5. Rủi ro sprint

| Rủi ro | Cách xử lý |
|---|---|
| PCAP không có ARP | Tạo PCAP mới bằng `arping` hoặc Wireshark |
| Parse sai byte order | So sánh với Wireshark |
| Không build được thư viện | Chuyển sang thư viện đơn giản hơn hoặc dùng libpcap trực tiếp |

---

## 13. Sprint 2: Parse DHCP, lưu dữ liệu và export

### 13.1. Sprint Goal

Bổ sung khả năng parse DHCP để lấy hostname/IP metadata và lưu asset vào SQLite hoặc export JSON/CSV.

### 13.2. Công việc cần làm

| Task | Làm như thế nào | Công cụ | Tại sao cần |
|---|---|---|---|
| Nhận diện DHCP | Kiểm tra UDP port 67/68 | UDP parser | DHCP chứa metadata thiết bị |
| Parse DHCP header | Lấy client MAC, IP liên quan | DHCP parser | Kết nối DHCP packet với asset |
| Parse DHCP options | Lấy hostname, requested IP, message type | DHCP option parser | Hostname thường nằm trong option 12 |
| Gộp ARP/DHCP data | Dùng MAC làm khóa để update asset | Asset Engine | Một asset có thể xuất hiện qua nhiều protocol |
| Thiết kế SQLite schema | Tạo bảng `assets` | SQLite | Lưu kết quả bền vững |
| Implement DB repository | Insert/update asset | SQLite API | Tách logic lưu trữ khỏi parser |
| Export JSON/CSV | Thêm option CLI | JSON/CSV writer | Dễ demo và đưa vào báo cáo |
| Test end-to-end | Chạy PCAP có ARP + DHCP | Wireshark, test script | Đảm bảo output đúng |

### 13.3. Schema SQLite đề xuất

```sql
CREATE TABLE IF NOT EXISTS assets (
    mac_address TEXT PRIMARY KEY,
    ip_addresses TEXT,
    hostname TEXT,
    first_seen TEXT,
    last_seen TEXT,
    discovery_sources TEXT,
    updated_at TEXT
);
```

Nếu muốn lưu lịch sử IP rõ hơn:

```sql
CREATE TABLE IF NOT EXISTS asset_ip_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mac_address TEXT,
    ip_address TEXT,
    first_seen TEXT,
    last_seen TEXT,
    source TEXT
);
```

### 13.4. Output cuối sprint

Chạy được:

```bash
./asset-discovery --pcap samples/arp_dhcp.pcap --db assets.db --export assets.json
```

Có output:

```json
[
  {
    "mac_address": "aa:bb:cc:dd:ee:ff",
    "ip_addresses": ["192.168.1.10"],
    "hostname": "laptop-user",
    "first_seen": "2026-06-xxT10:01:00",
    "last_seen": "2026-06-xxT10:05:00",
    "discovery_sources": ["ARP", "DHCP"]
  }
]
```

### 13.5. Acceptance Criteria

- Parse được DHCP packet ở UDP port 67/68.
- Lấy được hostname nếu DHCP Option 12 tồn tại.
- Không crash nếu DHCP packet không có hostname.
- Asset từ ARP và DHCP được merge theo MAC.
- Lưu được asset vào SQLite.
- Export được JSON hoặc CSV.

---

## 14. Sprint 3: Live capture và Docker

### 14.1. Sprint Goal

Hệ thống chạy được bằng Docker và hỗ trợ capture từ network interface.

### 14.2. Công việc cần làm

| Task | Làm như thế nào | Công cụ | Tại sao cần |
|---|---|---|---|
| Implement live capture | CLI nhận `--interface eth0` | libpcap/PcapPlusPlus | Đáp ứng yêu cầu traffic thực tế |
| Thêm duration/filter | `--duration 60`, filter ARP/DHCP | libpcap filter | Tránh chạy vô hạn khi demo |
| Viết Dockerfile | Build binary trong container | Docker | Đáp ứng yêu cầu triển khai |
| Viết docker-compose | Mount PCAP, cấu hình network | Docker Compose | Demo dễ hơn |
| Test PCAP trong Docker | Chạy container với file PCAP | Docker | Đảm bảo môi trường sạch vẫn chạy |
| Test live capture trong Docker | Dùng host network/capabilities | Docker | Kiểm chứng triển khai thực tế |
| Ghi chú quyền Docker | Viết rõ cần `NET_RAW`, `NET_ADMIN` hoặc host network | README | Tránh lỗi khi người khác chạy |

### 14.3. Dockerfile mẫu định hướng

```dockerfile
FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y \
    build-essential cmake git libpcap-dev sqlite3 libsqlite3-dev

WORKDIR /app
COPY . .
RUN cmake -S . -B build && cmake --build build -j

FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y \
    libpcap0.8 sqlite3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/asset-discovery /usr/local/bin/asset-discovery

ENTRYPOINT ["asset-discovery"]
```

### 14.4. Output cuối sprint

Chạy PCAP bằng Docker:

```bash
docker build -t passive-asset-discovery .
docker run --rm -v ./samples:/samples passive-asset-discovery \
  --pcap /samples/arp_dhcp.pcap --output table
```

Chạy live capture:

```bash
docker run --rm --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery --interface eth0 --duration 60
```

### 14.5. Acceptance Criteria

- Docker image build thành công.
- Container chạy được với file PCAP.
- Có hướng dẫn rõ cho live capture.
- Nếu live capture phụ thuộc hệ điều hành, tài liệu phải nêu giới hạn.
- Có docker-compose hoặc lệnh Docker đầy đủ.

---

## 15. Sprint 4: Hoàn thiện, kiểm thử, tài liệu và demo

### 15.1. Sprint Goal

Hoàn thiện sản phẩm để nộp: source code, tài liệu thiết kế, hướng dẫn build/deploy, Docker, demo và báo cáo.

### 15.2. Công việc cần làm

| Task | Làm như thế nào | Công cụ | Tại sao cần |
|---|---|---|---|
| Refactor code | Tách module capture/parser/engine/storage | C++ | Code dễ hiểu, dễ bảo vệ |
| Viết unit test | Test ARP, DHCP, Asset Engine | GoogleTest/Catch2 | Tăng độ tin cậy |
| Viết integration test | Chạy PCAP mẫu và kiểm tra output | CTest/shell script | Kiểm chứng toàn hệ thống |
| Hoàn thiện README | Build, run, Docker, example output | Markdown | Bắt buộc cho người dùng |
| Viết Design Document | Architecture, data flow, schema, sequence | Markdown, Mermaid | Đáp ứng yêu cầu tài liệu thiết kế |
| Viết Demo Guide | Kịch bản demo từng bước | Markdown | Trình bày tự tin |
| Chuẩn bị screenshot | CLI, Docker, Wireshark, DB result | Screenshot | Bổ sung báo cáo |
| Viết limitations | Nêu giới hạn và hướng phát triển | Markdown | Báo cáo khách quan |
| Tag release | Tạo version `v1.0-demo` | Git | Đóng gói trạng thái nộp bài |

### 15.3. Output cuối sprint

- Source code hoàn chỉnh.
- Dockerfile hoặc docker-compose.
- README đầy đủ.
- Design document.
- Demo script.
- Sample PCAP.
- Test result.
- Release/tag cuối.

### 15.4. Acceptance Criteria

- Clone repo về máy mới vẫn build được.
- Docker image build được.
- Demo PCAP chạy ổn định.
- Output có IP, MAC, first_seen, last_seen.
- Nếu có DHCP hostname thì hiển thị hostname.
- Có tài liệu giải thích kiến trúc.
- Có hướng dẫn deploy rõ ràng.

---

## 16. Scrum ceremonies áp dụng cho cá nhân

### 16.1. Sprint Planning

Thời điểm: đầu mỗi tuần.

Việc cần làm:

1. Xác định Sprint Goal.
2. Chọn task từ Product Backlog.
3. Chia nhỏ task nếu task quá lớn.
4. Ước lượng độ khó.
5. Xác định output cuối sprint.

Mẫu ghi chép:

```md
# Sprint Planning - Sprint 1

Sprint Goal:
Đọc được PCAP và phát hiện asset từ ARP.

Selected Backlog Items:
- Implement PCAP reader
- Parse ARP packet
- Create Asset model
- Print asset table

Risks:
- PCAP sample thiếu ARP
- Parse sai format ARP

Definition of Done:
- CLI chạy được với sample PCAP
- Output có MAC/IP/first_seen/last_seen
```

### 16.2. Daily Scrum cá nhân

Thời điểm: mỗi ngày 5-10 phút.

Mẫu:

```md
# Daily Scrum - YYYY-MM-DD

Yesterday:
- ...

Today:
- ...

Blockers:
- ...

Decision:
- ...
```

### 16.3. Sprint Review

Thời điểm: cuối sprint.

Bạn tự demo lại sản phẩm cho chính mình.

Câu hỏi cần trả lời:

- Sprint Goal có đạt không?
- Có tính năng nào chạy được?
- Có bằng chứng demo không?
- Có cần đổi backlog không?

Mẫu:

```md
# Sprint Review - Sprint 2

Done:
- Parse được DHCP hostname.
- Lưu asset vào SQLite.
- Export JSON.

Demo Evidence:
- Screenshot output CLI.
- File assets.json.
- Database assets.db.

Not Done:
- Chưa export CSV.

Decision:
- Đẩy CSV export sang Sprint 4 nếu còn thời gian.
```

### 16.4. Sprint Retrospective

Thời điểm: sau Sprint Review.

Mẫu:

```md
# Sprint Retrospective - Sprint 2

What went well:
- Parser ARP ổn định.
- SQLite dễ tích hợp.

What did not go well:
- DHCP option parsing mất nhiều thời gian hơn dự kiến.

Action items:
- Viết test trước cho từng DHCP option.
- Dùng Wireshark đối chiếu từng field.
```

---

## 17. Quy trình Git đề xuất

### 17.1. Branching model

Vì đây là dự án cá nhân, không cần Git Flow quá phức tạp.

Đề xuất:

```text
main
develop
feature/init-project
feature/pcap-reader
feature/arp-parser
feature/dhcp-parser
feature/asset-engine
feature/storage-sqlite
feature/docker-deploy
feature/docs-demo
```

### 17.2. Quy tắc branch

| Branch | Ý nghĩa |
|---|---|
| `main` | Bản ổn định để nộp/demo |
| `develop` | Bản phát triển chính |
| `feature/*` | Mỗi tính năng một nhánh |
| `fix/*` | Sửa lỗi |
| `docs/*` | Viết tài liệu |

### 17.3. Quy trình làm việc

```bash
git checkout develop
git pull origin develop

git checkout -b feature/arp-parser

# code, test
git add .
git commit -m "feat: parse ARP packets and update asset table"

git checkout develop
git merge feature/arp-parser
git push origin develop
```

### 17.4. Quy ước commit message

| Prefix | Khi dùng |
|---|---|
| `feat:` | Thêm tính năng mới |
| `fix:` | Sửa lỗi |
| `docs:` | Cập nhật tài liệu |
| `test:` | Thêm/sửa test |
| `refactor:` | Sửa cấu trúc code không đổi behavior |
| `build:` | Cập nhật build/Docker/CMake |
| `chore:` | Việc phụ trợ |

Ví dụ:

```text
feat: add pcap file reader
feat: parse arp sender ip and mac
feat: merge dhcp hostname into asset metadata
build: add dockerfile for runtime deployment
docs: add demo guide
test: add unit tests for asset discovery engine
```

---

## 18. Cấu trúc thư mục đề xuất

```text
passive-network-asset-discovery/
├── CMakeLists.txt
├── README.md
├── Dockerfile
├── docker-compose.yml
├── docs/
│   ├── agile-scrum-plan.md
│   ├── design-document.md
│   ├── deployment-guide.md
│   ├── demo-guide.md
│   └── sprint-reports/
│       ├── sprint-0.md
│       ├── sprint-1.md
│       ├── sprint-2.md
│       ├── sprint-3.md
│       └── sprint-4.md
├── include/
│   ├── capture/
│   ├── parser/
│   ├── asset/
│   ├── storage/
│   └── cli/
├── src/
│   ├── main.cpp
│   ├── capture/
│   │   ├── PcapFileReader.cpp
│   │   └── LiveCapture.cpp
│   ├── parser/
│   │   ├── ArpParser.cpp
│   │   └── DhcpParser.cpp
│   ├── asset/
│   │   ├── Asset.cpp
│   │   └── AssetDiscoveryEngine.cpp
│   ├── storage/
│   │   └── SQLiteAssetRepository.cpp
│   └── cli/
│       └── CliOptions.cpp
├── tests/
│   ├── test_arp_parser.cpp
│   ├── test_dhcp_parser.cpp
│   ├── test_asset_engine.cpp
│   └── integration/
│       └── test_pcap_end_to_end.sh
├── samples/
│   ├── arp_sample.pcap
│   └── arp_dhcp_sample.pcap
├── scripts/
│   ├── build.sh
│   ├── run_pcap_demo.sh
│   └── run_docker_demo.sh
└── db/
    └── schema.sql
```

---

## 19. Data model đề xuất

### 19.1. Asset object

```cpp
struct Asset {
    std::string mac_address;
    std::set<std::string> ip_addresses;
    std::string hostname;
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    std::set<std::string> discovery_sources;
};
```

### 19.2. Asset update logic

```text
Khi packet ARP/DHCP được parse:
1. Lấy MAC.
2. Nếu MAC chưa tồn tại:
   - Tạo asset mới.
   - first_seen = packet timestamp.
   - last_seen = packet timestamp.
3. Nếu MAC đã tồn tại:
   - Update last_seen.
   - Thêm IP mới nếu có.
   - Bổ sung hostname nếu có.
   - Bổ sung discovery source.
```

### 19.3. Vì sao dùng MAC làm khóa chính?

Vì trong mạng nội bộ:

- IP có thể thay đổi do DHCP.
- Một thiết bị có thể nhận IP mới.
- MAC thường ổn định hơn IP trong phạm vi LAN.

Tuy nhiên cần ghi rõ giới hạn:

- MAC có thể bị randomization trên một số thiết bị.
- MAC có thể bị spoof.
- Một thiết bị có thể có nhiều network interface.

---

## 20. Kanban board mẫu

### 20.1. Các cột

```text
Backlog -> Ready -> In Progress -> Review/Test -> Done
```

### 20.2. Card mẫu

```md
Title:
Parse DHCP hostname option

Description:
Là developer, tôi muốn parse DHCP Option 12 để lấy hostname của thiết bị,
để asset inventory hiển thị tên thiết bị dễ đọc hơn.

Tasks:
- Detect UDP 67/68.
- Parse DHCP fixed header.
- Iterate DHCP options.
- Extract option 12 hostname.
- Update AssetDiscoveryEngine.

Acceptance Criteria:
- PCAP có DHCP hostname thì output hiển thị hostname.
- PCAP không có hostname không làm app crash.
- Có unit test cho DHCP option parser.

Tools:
- C++
- Wireshark
- GoogleTest
- Sample PCAP

Priority:
High

Estimate:
5 story points
```

---

## 21. Ước lượng độ khó bằng Story Point

### 21.1. Quy ước

| Point | Ý nghĩa |
|---|---|
| 1 | Rất nhỏ, làm nhanh |
| 2 | Nhỏ, ít rủi ro |
| 3 | Trung bình |
| 5 | Khó hoặc cần nghiên cứu |
| 8 | Rất khó, nên chia nhỏ |

### 21.2. Ví dụ ước lượng

| Task | Story Point | Lý do |
|---|---:|---|
| Tạo README skeleton | 1 | Dễ, ít rủi ro |
| Tạo CMake project | 2 | Cần cấu hình build |
| Đọc PCAP file | 5 | Phụ thuộc thư viện packet |
| Parse ARP | 3 | Header đơn giản |
| Parse DHCP options | 8 | Options biến độ dài, dễ lỗi |
| Asset Engine | 5 | Cần xử lý merge dữ liệu |
| SQLite storage | 5 | Cần schema và repository |
| Dockerfile | 3 | Trung bình |
| Live capture Docker | 8 | Có rủi ro quyền mạng |

---

## 22. Quản lý rủi ro

| Rủi ro | Mức độ | Dấu hiệu | Cách xử lý |
|---|---|---|---|
| Không capture live được trong Docker | Cao | Permission denied khi mở interface | Ưu tiên PCAP demo, ghi rõ live cần quyền |
| DHCP parser phức tạp | Cao | Không lấy được hostname | Scope tối thiểu: parse option 12, 50, 53 |
| PCAP mẫu thiếu dữ liệu | Trung bình | Output ít asset | Tạo PCAP bằng Wireshark/tcpdump |
| Build C++ lỗi trên máy khác | Trung bình | Thiếu libpcap/sqlite | Dùng Docker làm môi trường chuẩn |
| Quá tải scope | Cao | Thêm UI/web quá sớm | Giữ MVP: CLI + Docker + docs |
| Asset bị trùng | Trung bình | Một MAC xuất hiện nhiều dòng | Dùng MAC làm key và merge IP |
| Timestamp sai | Trung bình | first_seen/last_seen không đúng | Dùng timestamp của packet thay vì thời gian hiện tại khi đọc PCAP |
| Thiếu tài liệu | Cao | Code chạy nhưng khó chấm | Viết README/design song song mỗi sprint |

---

## 23. MVP cần hoàn thành trước

MVP là phiên bản tối thiểu nhưng có thể demo được.

### 23.1. MVP bắt buộc

- Đọc file PCAP.
- Parse ARP.
- Phát hiện asset theo IP/MAC.
- Có first_seen/last_seen.
- In danh sách asset ra console.
- Chạy được bằng Docker.
- Có README hướng dẫn chạy.

### 23.2. MVP mở rộng

- Parse DHCP hostname.
- Lưu SQLite.
- Export JSON/CSV.
- Live capture interface.
- Test tự động.
- Demo guide.

### 23.3. Không nên làm trước MVP

- Web dashboard.
- Login/authentication.
- Distributed storage.
- Machine learning.
- Threat detection nâng cao.
- Full network inventory platform.

---

## 24. Kịch bản demo cuối

### 24.1. Demo bằng PCAP

```bash
docker build -t passive-asset-discovery .

docker run --rm -v ./samples:/samples passive-asset-discovery \
  --pcap /samples/arp_dhcp_sample.pcap --output table
```

Trình bày:

1. Mở Wireshark để cho thấy PCAP có ARP/DHCP.
2. Chạy container.
3. Hiển thị danh sách asset.
4. Giải thích các cột:
   - MAC
   - IP
   - hostname
   - first_seen
   - last_seen
   - source
5. Export JSON/CSV nếu có.

### 24.2. Demo live capture

```bash
docker run --rm --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery --interface eth0 --duration 60 --output table
```

Trình bày:

1. Chọn interface.
2. Chạy capture trong 60 giây.
3. Tạo traffic ARP/DHCP nếu cần.
4. Hiển thị asset phát hiện được.
5. Giải thích giới hạn quyền khi chạy Docker.

### 24.3. Demo database

```bash
sqlite3 assets.db "SELECT * FROM assets;"
```

Trình bày:

- Dữ liệu được lưu lại sau khi phân tích.
- Có thể truy vấn lại asset.
- Có thể export để làm báo cáo.

---

## 25. Checklist nộp bài

### 25.1. Source code

- [ ] Có repository đầy đủ.
- [ ] Code chia module rõ ràng.
- [ ] Có CMake build.
- [ ] Không hard-code đường dẫn tuyệt đối.
- [ ] Có sample PCAP hoặc hướng dẫn tạo PCAP.

### 25.2. Core feature

- [ ] Đọc được PCAP.
- [ ] Capture được live interface hoặc có giải thích rõ nếu môi trường hạn chế.
- [ ] Parse được ARP.
- [ ] Parse được DHCP cơ bản.
- [ ] Phát hiện asset mới.
- [ ] Cập nhật first_seen.
- [ ] Cập nhật last_seen.
- [ ] Hiển thị IP/MAC/hostname nếu có.

### 25.3. Docker

- [ ] Có Dockerfile.
- [ ] Có docker-compose nếu cần.
- [ ] Build image thành công.
- [ ] Chạy được PCAP demo trong container.
- [ ] Có hướng dẫn quyền khi live capture.

### 25.4. Documentation

- [ ] README.md.
- [ ] Design document.
- [ ] Deployment guide.
- [ ] Demo guide.
- [ ] Agile/Scrum plan.
- [ ] Sprint reports.
- [ ] Limitations/Future work.

### 25.5. Testing

- [ ] Unit test parser.
- [ ] Unit test asset engine.
- [ ] Integration test PCAP.
- [ ] Docker run test.
- [ ] Có screenshot/log demo.

---

## 26. Template Sprint Report

Tạo file:

```text
docs/sprint-reports/sprint-1.md
```

Nội dung mẫu:

```md
# Sprint 1 Report

## Sprint Goal

Đọc được file PCAP và phát hiện asset từ ARP traffic.

## Planned Tasks

| Task | Status | Note |
|---|---|---|
| Implement PCAP reader | Done | Đọc được packet |
| Parse ARP | Done | Lấy sender IP/MAC |
| Create Asset model | Done | Dùng MAC làm key |
| Print asset table | Done | Output console |

## Demo Evidence

Command:

```bash
./asset-discovery --pcap samples/arp_sample.pcap
```

Output:

```text
MAC                IP              FIRST_SEEN        LAST_SEEN
aa:bb:cc:dd:ee:ff  192.168.1.10    ...               ...
```

## Issues

- Ban đầu PCAP không có ARP packet.
- Đã tạo lại PCAP bằng Wireshark.

## Retrospective

What went well:
- Cấu trúc parser rõ ràng.

What can be improved:
- Cần viết test sớm hơn.

Action items:
- Thêm unit test cho ARP parser trong sprint sau.
```

---

## 27. Template User Story

```md
# User Story: Detect asset from ARP traffic

As a network analyst,
I want the system to detect assets from ARP traffic,
so that I can know which devices appear in the local network.

## Acceptance Criteria

- Given a PCAP file containing ARP packets,
  when I run the system,
  then the system outputs at least MAC address and IP address.
- If the same MAC appears multiple times,
  the system updates `last_seen` instead of creating duplicate assets.
- If the packet is not ARP,
  the system ignores it without crashing.

## Technical Notes

- Use MAC address as primary asset key.
- Use packet timestamp for first_seen/last_seen.
- Compare output with Wireshark.
```

---

## 28. Template Task Card

```md
Title:
Implement ARP parser

Description:
Parse ARP packets to extract sender MAC, sender IP, target MAC and target IP.

Why:
ARP traffic is one of the primary sources for passive asset discovery in a LAN.

How:
- Check Ethernet type equals 0x0806.
- Parse ARP header fields.
- Convert MAC/IP bytes to string.
- Return parsed ARP metadata object.

Tools:
- C++
- libpcap/PcapPlusPlus
- Wireshark
- GoogleTest

Acceptance Criteria:
- Parse sender MAC/IP correctly from sample ARP packet.
- Ignore malformed ARP packet safely.
- Unit test passes.
```

---

## 29. Các câu hỏi tự kiểm tra khi bảo vệ

### 29.1. Về Agile/Scrum

**Câu hỏi:** Vì sao bạn dùng Agile/Scrum cho đề tài này?

**Trả lời gợi ý:**  
Vì đề tài có nhiều rủi ro kỹ thuật như chọn thư viện capture, xử lý PCAP/live traffic, Docker permission và parse protocol. Agile/Scrum giúp chia nhỏ thành từng sprint, mỗi sprint tạo ra một phần chạy được, nhờ đó giảm rủi ro và dễ điều chỉnh scope.

---

**Câu hỏi:** Một người làm thì áp dụng Scrum như thế nào?

**Trả lời gợi ý:**  
Em tự đóng nhiều vai trò. Khi là Product Owner, em quản lý backlog và ưu tiên tính năng. Khi là Developer, em code. Khi là Scrum Master, em tự kiểm tra tiến độ qua daily self-check, sprint review và retrospective. Khi là QA/DevOps, em test và đóng gói Docker.

---

### 29.2. Về kỹ thuật

**Câu hỏi:** Vì sao dùng passive discovery thay vì active scan?

**Trả lời gợi ý:**  
Vì đề tài yêu cầu hệ thống giám sát thụ động. Passive discovery không gửi packet dò quét vào mạng, ít gây ảnh hưởng hệ thống hơn và phù hợp cho môi trường cần quan sát traffic có sẵn.

---

**Câu hỏi:** Vì sao dùng ARP và DHCP?

**Trả lời gợi ý:**  
ARP giúp ánh xạ IP với MAC trong mạng LAN, rất hữu ích để phát hiện thiết bị. DHCP cung cấp thêm metadata như hostname, requested IP hoặc message type, giúp enrich thông tin asset.

---

**Câu hỏi:** Vì sao dùng MAC làm khóa chính cho asset?

**Trả lời gợi ý:**  
IP có thể thay đổi do DHCP, trong khi MAC thường ổn định hơn trong phạm vi mạng LAN. Vì vậy hệ thống dùng MAC làm khóa chính và lưu IP như thuộc tính có thể có nhiều giá trị theo thời gian.

---

**Câu hỏi:** Docker live capture cần lưu ý gì?

**Trả lời gợi ý:**  
Container thường không có quyền capture interface mặc định. Khi chạy live capture có thể cần `--net=host`, `--cap-add=NET_ADMIN`, `--cap-add=NET_RAW` hoặc chạy với quyền phù hợp. Vì vậy demo bằng PCAP ổn định hơn và live capture được coi là phần mở rộng kiểm chứng thực tế.

---

## 30. Kết luận

Áp dụng Agile/Scrum cho dự án này giúp bạn quản lý công việc một cách có hệ thống dù chỉ làm một mình. Thay vì cố hoàn thành toàn bộ hệ thống trong một lần, bạn chia nhỏ thành các sprint:

1. Chuẩn bị môi trường.
2. Đọc PCAP và parse ARP.
3. Parse DHCP, lưu dữ liệu và export.
4. Docker/live capture.
5. Hoàn thiện test, tài liệu và demo.

Cách làm này giúp đảm bảo mỗi giai đoạn đều tạo ra kết quả chạy được, dễ kiểm chứng, dễ báo cáo và giảm rủi ro khi phát triển phần mềm.
