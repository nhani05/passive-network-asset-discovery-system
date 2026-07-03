# CHƯƠNG 7. KẾT LUẬN VÀ HƯỚNG PHÁT TRIỂN

## 7.1. Giới thiệu chương

Chương này tổng kết quá trình thực hiện đề tài **Passive Network Asset Discovery System**, đánh giá mức độ hoàn thành mục tiêu, nêu các kết quả đạt được, những hạn chế còn tồn tại và đề xuất hướng phát triển trong tương lai.

Đề tài tập trung xây dựng một hệ thống phát hiện tài sản mạng bằng phương pháp thụ động. Thay vì gửi gói tin thăm dò đến thiết bị, hệ thống quan sát và phân tích lưu lượng mạng có sẵn từ file PCAP hoặc network interface. Từ đó, hệ thống trích xuất thông tin như địa chỉ MAC, địa chỉ IP, hostname, giao thức phát hiện và thời điểm quan sát để tổng hợp thành danh sách tài sản mạng.

## 7.2. Kết quả đạt được

Sau quá trình nghiên cứu, thiết kế, triển khai và kiểm thử, đề tài đã đạt được các kết quả chính sau:

### 7.2.1. Xây dựng được hệ thống phát hiện tài sản mạng thụ động

Hệ thống có thể đọc lưu lượng mạng từ file PCAP hoặc bắt gói tin trực tiếp từ network interface. Dữ liệu packet được phân tích để tạo ra các observation, sau đó tổng hợp thành danh sách asset.

Hệ thống không thực hiện quét chủ động, không gửi packet thăm dò đến thiết bị. Điều này phù hợp với mục tiêu giám sát ít xâm lấn và giảm tác động đến mạng.

### 7.2.2. Hỗ trợ phân tích nhiều giao thức

Hệ thống đã triển khai cơ chế parser plugin và có các parser phục vụ phân tích một số giao thức quan trọng như:

- ARP.
- DHCP.
- DNS.
- NBNS.
- SSDP.
- TCP.

Trong đó, ARP giúp ánh xạ IP và MAC, DHCP có thể cung cấp hostname và thông tin cấp phát IP, còn các parser khác giúp mở rộng khả năng quan sát endpoint hoặc metadata từ lưu lượng mạng.

### 7.2.3. Tổng hợp được thông tin thành asset

Hệ thống sử dụng `AssetObservation` làm dữ liệu trung gian và `AssetStore` để tổng hợp thông tin. Một asset được định danh chủ yếu bằng địa chỉ MAC và có thể chứa nhiều địa chỉ IP, hostname, thời điểm phát hiện đầu tiên, thời điểm phát hiện gần nhất và danh sách nguồn phát hiện.

Cơ chế này giúp giảm trùng lặp dữ liệu và phản ánh tốt hơn thực tế rằng một thiết bị có thể xuất hiện nhiều lần trong nhiều gói tin khác nhau.

### 7.2.4. Ghi nhận được sự kiện mạng

Bên cạnh snapshot cuối của tài sản, hệ thống còn hỗ trợ event logging. Các sự kiện như thiết bị mới, IP đổi MAC, MAC đổi IP, hostname thay đổi, IP/MAC flip-flop hoặc ARP mismatch giúp người dùng theo dõi diễn biến mạng theo thời gian.

Event được ghi ra stdout, file NDJSON, syslog nếu hỗ trợ và PostgreSQL khi database được cấu hình.

### 7.2.5. Hỗ trợ nhiều định dạng đầu ra

Hệ thống hỗ trợ các định dạng summary:

- Table.
- JSON.
- CSV.

Table phù hợp để đọc trực tiếp trên terminal. JSON phù hợp cho tích hợp với script hoặc hệ thống khác. CSV phù hợp cho lưu trữ đơn giản hoặc mở bằng bảng tính.

### 7.2.6. Tích hợp PostgreSQL

Hệ thống có khả năng lưu snapshot asset vào bảng `assets` và lịch sử sự kiện vào bảng `asset_events`. Cơ chế upsert theo `mac_address` giúp chạy lại cùng dữ liệu không tạo bản ghi asset trùng lặp.

Việc tích hợp database giúp kết quả phát hiện không chỉ tồn tại trên terminal mà có thể được lưu lại để truy vấn và phục vụ phân tích sau này.

### 7.2.7. Hỗ trợ đóng gói và triển khai bằng Docker

Đề tài đã cung cấp Dockerfile multi-stage và Docker Compose. Người dùng có thể build image, chạy phân tích PCAP trong container, chạy demo cùng PostgreSQL hoặc chạy live capture trong container trên Linux host với quyền phù hợp.

Docker giúp quá trình demo và triển khai nhất quán hơn, giảm phụ thuộc vào cấu hình máy cá nhân.

### 7.2.8. Xây dựng bộ kiểm thử tự động

Hệ thống có bộ test bằng CTest bao phủ nhiều thành phần như CLI, config, parser, renderer, asset store, event detector, PostgreSQL writer, bounded queue và live pipeline.

Bộ test giúp tăng độ tin cậy của hệ thống và hỗ trợ phát hiện lỗi khi thay đổi mã nguồn.

**[PLACEHOLDER HÌNH 7.1: Chèn sơ đồ tổng hợp các kết quả đạt được, gồm PCAP/Live Capture, Parser, Asset Store, Event Log, PostgreSQL và Docker]**

**Hình 7.1. Tổng hợp các thành phần chính đã hoàn thành của hệ thống**

## 7.3. Mức độ hoàn thành mục tiêu

Đối chiếu với mục tiêu ban đầu, hệ thống đã đáp ứng các yêu cầu chính:

| Mục tiêu | Mức độ hoàn thành |
|---|---|
| Tìm hiểu phát hiện tài sản mạng thụ động | Hoàn thành |
| Đọc dữ liệu từ file PCAP | Hoàn thành |
| Live capture từ network interface | Hoàn thành trong môi trường có quyền phù hợp |
| Phân tích ARP/DHCP để lấy IP, MAC, hostname | Hoàn thành |
| Tổng hợp observation thành asset | Hoàn thành |
| Xuất kết quả table/JSON/CSV | Hoàn thành |
| Ghi event log | Hoàn thành |
| Lưu PostgreSQL | Hoàn thành |
| Đóng gói Docker | Hoàn thành |
| Kiểm thử tự động | Hoàn thành |

Nhìn chung, hệ thống đáp ứng được yêu cầu cốt lõi của đề tài và có thêm một số chức năng mở rộng như event logging, live pipeline đa luồng, Docker Compose và benchmark.

## 7.4. Ý nghĩa thực tiễn

Hệ thống có ý nghĩa thực tiễn trong công tác quản trị mạng và an toàn thông tin. Với khả năng phát hiện tài sản từ lưu lượng thụ động, quản trị viên có thể nhanh chóng có được danh sách thiết bị xuất hiện trong mạng mà không cần thực hiện quét chủ động.

Việc ghi nhận event giúp hỗ trợ phát hiện các thay đổi bất thường như một IP được gắn với MAC khác, thiết bị đổi hostname hoặc xuất hiện thiết bị mới. Những thông tin này có thể giúp quản trị viên điều tra sự cố, kiểm tra thiết bị lạ và cải thiện quá trình quản lý tài sản.

Ngoài ra, việc hỗ trợ JSON, CSV và PostgreSQL giúp hệ thống có khả năng tích hợp với các quy trình xử lý dữ liệu khác. Docker giúp việc triển khai và demo thuận tiện hơn.

## 7.5. Hạn chế của hệ thống

Mặc dù đã đáp ứng các yêu cầu chính, hệ thống vẫn còn một số hạn chế.

Thứ nhất, hệ thống chỉ phát hiện được thiết bị có phát sinh lưu lượng trong thời gian quan sát. Đây là hạn chế tự nhiên của phương pháp thụ động. Nếu một thiết bị không gửi packet, đang tắt hoặc không xuất hiện trong file PCAP, hệ thống không thể phát hiện thiết bị đó.

Thứ hai, thông tin hostname không phải lúc nào cũng có. Hostname phụ thuộc vào các giao thức như DHCP, NBNS, mDNS hoặc các packet có chứa tên thiết bị. Nếu thiết bị không gửi thông tin này, asset chỉ có thể được nhận diện bằng IP và MAC.

Thứ ba, dữ liệu quan sát có thể không đầy đủ nếu hệ thống không đặt tại vị trí mạng phù hợp. Ví dụ, nếu capture tại một interface không thấy toàn bộ traffic của mạng LAN, danh sách asset sẽ chỉ phản ánh phạm vi lưu lượng quan sát được.

Thứ tư, live capture phụ thuộc vào quyền hệ thống và nền tảng. Backend AF_PACKET chỉ phù hợp trên Linux. Khi chạy trong Docker, live capture cần host networking và capability mạng, đồng thời Docker Desktop trên macOS/Windows có thể không phản ánh đúng môi trường Linux host.

Thứ năm, PostgreSQL writer hiện phụ thuộc vào client `psql` trong môi trường chạy. Cách này đơn giản và phù hợp với phạm vi đề tài, nhưng trong hệ thống production có thể cần thư viện PostgreSQL native để kiểm soát kết nối tốt hơn.

Thứ sáu, hệ thống chưa có giao diện dashboard trực quan. Người dùng hiện tương tác chủ yếu qua CLI, file log và database.

## 7.6. Hướng phát triển

### 7.6.1. Bổ sung parser cho nhiều giao thức hơn

Hệ thống có thể được mở rộng để phân tích thêm các giao thức có giá trị cho phát hiện tài sản như:

- mDNS.
- LLMNR.
- SNMP.
- SMB.
- HTTP User-Agent trong môi trường phù hợp.
- TLS ClientHello metadata.
- MQTT hoặc giao thức IoT phổ biến.

Việc bổ sung parser mới sẽ giúp tăng độ đầy đủ của thông tin thiết bị, đặc biệt trong môi trường có nhiều thiết bị IoT và dịch vụ nội bộ.

### 7.6.2. Bổ sung nhận diện vendor và loại thiết bị

Từ địa chỉ MAC, hệ thống có thể tra cứu OUI để suy luận nhà sản xuất thiết bị. Kết hợp với hostname, DHCP option, SSDP service hoặc NBNS name, hệ thống có thể đưa ra gợi ý loại thiết bị như laptop, camera, router, máy in hoặc thiết bị IoT.

Kết quả này không nên được xem là tuyệt đối, nhưng có thể hỗ trợ quản trị viên phân loại tài sản nhanh hơn.

### 7.6.3. Xây dựng dashboard

Một hướng phát triển quan trọng là xây dựng dashboard web để hiển thị:

- Danh sách asset.
- Timeline sự kiện.
- Bộ lọc theo IP, MAC, hostname, protocol.
- Cảnh báo thiết bị mới.
- Thống kê số lượng asset theo nguồn phát hiện.
- Trạng thái live capture và metrics.

Dashboard giúp hệ thống dễ sử dụng hơn đối với người không quen làm việc trực tiếp với CLI hoặc SQL.

**[PLACEHOLDER HÌNH 7.2: Chèn mockup dashboard tương lai gồm bảng asset, biểu đồ số lượng asset, timeline event và bộ lọc tìm kiếm]**

**Hình 7.2. Minh họa hướng phát triển dashboard quản lý tài sản mạng**

### 7.6.4. Cải thiện cơ chế lưu trữ

Trong tương lai, hệ thống có thể sử dụng thư viện PostgreSQL native thay vì gọi `psql`. Điều này giúp:

- Quản lý connection pool tốt hơn.
- Xử lý lỗi database chi tiết hơn.
- Tối ưu batch insert/update.
- Giảm phụ thuộc vào binary bên ngoài.

Ngoài ra, có thể bổ sung schema lưu lịch sử asset theo thời gian, không chỉ snapshot cuối.

### 7.6.5. Tích hợp cảnh báo

Hệ thống có thể tích hợp với các kênh cảnh báo như email, webhook, Slack, Microsoft Teams hoặc SIEM. Các event có severity cao như `ip_mac_flip_flop`, `ethernet_arp_mac_mismatch` hoặc `mac_changed_for_ip` có thể được gửi cảnh báo realtime.

### 7.6.6. Cải thiện benchmark và tối ưu hiệu năng

Live pipeline hiện đã tách capture, parser, aggregator và event writer. Tuy nhiên, có thể tiếp tục tối ưu:

- Điều chỉnh batch size tự động.
- Tối ưu cấu trúc dữ liệu trong asset store.
- Giảm copy packet trên hot path.
- Xuất metrics chuẩn Prometheus.
- So sánh hiệu năng giữa backend libpcap và AF_PACKET trên nhiều môi trường.

### 7.6.7. Bổ sung cơ chế cấu hình production

Hệ thống có thể bổ sung:

- File cấu hình mẫu cho nhiều môi trường.
- Validate cấu hình chi tiết hơn.
- Secret management tốt hơn.
- Tách cấu hình event sink theo từng đích.
- Chính sách retention cho event log và database.

### 7.6.8. Kết hợp passive và active discovery có kiểm soát

Mặc dù đề tài tập trung vào passive discovery, trong tương lai có thể bổ sung active discovery tùy chọn với phạm vi kiểm soát chặt chẽ. Ví dụ, hệ thống có thể cho phép quét nhẹ một dải mạng nhỏ khi người dùng bật rõ ràng. Tuy nhiên, tính năng này cần được thiết kế tách biệt và có cảnh báo để không làm mất bản chất ít xâm lấn của hệ thống.

## 7.7. Bài học kinh nghiệm

Trong quá trình thực hiện đề tài, một số bài học quan trọng được rút ra:

- Phân tích packet cần kiểm tra chặt chẽ độ dài dữ liệu trước khi đọc field.
- PCAP fixture rất hữu ích cho kiểm thử tự động vì cho kết quả lặp lại ổn định.
- Passive discovery cần cơ chế tổng hợp dữ liệu tốt vì thông tin thiết bị thường phân tán ở nhiều packet và giao thức.
- Event history và asset snapshot phục vụ hai nhu cầu khác nhau, nên cần lưu tách biệt.
- Live capture cần quan tâm đến concurrency, queue size và graceful shutdown.
- Docker giúp demo dễ hơn, nhưng live capture trong container có nhiều ràng buộc về nền tảng và quyền.

## 7.8. Kết luận chung

Đề tài **Passive Network Asset Discovery System** đã hoàn thành mục tiêu xây dựng một hệ thống phát hiện tài sản mạng bằng phương pháp thụ động. Hệ thống có thể đọc dữ liệu từ PCAP hoặc live interface, phân tích packet, trích xuất thông tin thiết bị, tổng hợp thành asset, ghi nhận sự kiện, xuất kết quả ở nhiều định dạng, lưu PostgreSQL và triển khai bằng Docker.

Kết quả đạt được cho thấy phương pháp phát hiện thụ động là một hướng tiếp cận phù hợp trong các môi trường cần giám sát ít xâm lấn. Dù còn một số hạn chế như phụ thuộc vào lưu lượng quan sát được và chưa có dashboard trực quan, hệ thống đã tạo được nền tảng tốt để phát triển thêm các chức năng quản lý tài sản mạng, cảnh báo bất thường và tích hợp với hệ thống giám sát an toàn thông tin trong tương lai.
