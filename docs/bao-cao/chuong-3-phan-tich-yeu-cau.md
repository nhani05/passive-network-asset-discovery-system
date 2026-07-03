# CHƯƠNG 3. PHÂN TÍCH YÊU CẦU

## 3.1. Giới thiệu chương

Sau khi đã trình bày cơ sở lý thuyết về phát hiện tài sản mạng thụ động ở chương trước, chương này tập trung phân tích các yêu cầu của hệ thống **Passive Network Asset Discovery System**. Việc phân tích yêu cầu là bước quan trọng trước khi thiết kế và triển khai, vì nó giúp xác định rõ hệ thống cần làm gì, phục vụ cho ai, dữ liệu đầu vào và đầu ra là gì, cũng như những ràng buộc kỹ thuật cần tuân thủ.

Hệ thống được xây dựng với mục tiêu hỗ trợ phát hiện tài sản mạng bằng cách quan sát lưu lượng có sẵn. Khác với các công cụ quét chủ động, hệ thống không gửi gói tin thăm dò đến thiết bị mà chỉ đọc, phân tích và tổng hợp thông tin từ các gói tin đã thu thập được. Vì vậy, các yêu cầu của hệ thống cần phản ánh đúng định hướng thụ động, ít xâm lấn và có khả năng hoạt động trong cả môi trường kiểm thử lẫn môi trường thực tế.

Nội dung chương này bao gồm mô tả bài toán, đối tượng sử dụng, yêu cầu chức năng, yêu cầu phi chức năng, yêu cầu dữ liệu, các ca sử dụng chính, ràng buộc kỹ thuật và tiêu chí nghiệm thu của hệ thống.

## 3.2. Mô tả bài toán

Trong một mạng nội bộ, nhiều thiết bị có thể kết nối và trao đổi dữ liệu tại các thời điểm khác nhau. Quản trị viên mạng cần biết các thiết bị này là gì, sử dụng địa chỉ IP nào, địa chỉ MAC nào, có hostname hay không, được phát hiện qua giao thức nào và xuất hiện vào thời điểm nào. Nếu không có công cụ hỗ trợ, việc kiểm kê thủ công thường mất thời gian, dễ thiếu sót và khó cập nhật khi thiết bị thay đổi liên tục.

Bài toán đặt ra là xây dựng một hệ thống có khả năng:

- Thu thập hoặc đọc lưu lượng mạng từ nguồn dữ liệu được chỉ định.
- Phân tích các gói tin mạng mà không làm thay đổi trạng thái của thiết bị trong mạng.
- Trích xuất thông tin nhận diện thiết bị từ các giao thức phù hợp.
- Gom nhiều thông tin rời rạc thành một bản ghi tài sản mạng thống nhất.
- Ghi nhận các sự kiện quan trọng như thiết bị mới, thay đổi IP, thay đổi MAC hoặc hostname mới.
- Xuất kết quả ở dạng dễ quan sát và có thể tích hợp với hệ thống khác.

Đầu vào của hệ thống có thể là file PCAP hoặc network interface. File PCAP phù hợp cho phân tích ngoại tuyến, kiểm thử và demo. Network interface phù hợp cho live capture trong môi trường thật. Đầu ra của hệ thống có thể là bảng hiển thị trên terminal, JSON, CSV, file sự kiện NDJSON hoặc bản ghi trong PostgreSQL.

**[PLACEHOLDER HÌNH 3.1: Chèn sơ đồ ngữ cảnh hệ thống, gồm nguồn đầu vào PCAP/Network Interface, hệ thống Passive Network Asset Discovery, và các đầu ra Table/JSON/CSV/NDJSON/PostgreSQL]**

**Hình 3.1. Sơ đồ ngữ cảnh của hệ thống Passive Network Asset Discovery**

## 3.3. Đối tượng sử dụng hệ thống

Hệ thống hướng đến các nhóm người dùng chính sau:

- **Quản trị viên mạng:** sử dụng hệ thống để kiểm kê thiết bị trong mạng nội bộ, theo dõi sự xuất hiện của thiết bị mới và hỗ trợ kiểm tra trạng thái tài sản mạng.
- **Nhân sự an toàn thông tin:** sử dụng kết quả phát hiện để xác định thiết bị lạ, thiết bị không nằm trong danh sách quản lý hoặc các hiện tượng bất thường liên quan đến IP và MAC.
- **Người triển khai hệ thống:** cấu hình nguồn dữ liệu, bộ lọc BPF, profile chạy, biến môi trường kết nối cơ sở dữ liệu và Docker runtime.
- **Sinh viên hoặc người nghiên cứu:** sử dụng hệ thống để tìm hiểu cách bắt gói tin, phân tích giao thức và xây dựng công cụ giám sát mạng thụ động.

Vì người dùng có thể có trình độ kỹ thuật khác nhau, hệ thống cần có giao diện dòng lệnh rõ ràng, thông báo lỗi dễ hiểu, tài liệu hướng dẫn đầy đủ và đầu ra có cấu trúc.

## 3.4. Yêu cầu chức năng

### 3.4.1. Yêu cầu chọn nguồn dữ liệu

Hệ thống cần hỗ trợ hai nguồn dữ liệu chính:

- **PCAP offline:** người dùng truyền đường dẫn file PCAP bằng tham số `--pcap <file>`. Hệ thống đọc toàn bộ file, phân tích gói tin và xuất kết quả tổng hợp cuối cùng.
- **Live capture:** người dùng truyền tên network interface bằng tham số `--interface <name>`. Hệ thống bắt gói tin trực tiếp từ interface và xử lý liên tục cho đến khi nhận tín hiệu dừng.

Hệ thống phải đảm bảo mỗi lần chạy chỉ chọn đúng một nguồn dữ liệu. Nếu người dùng không truyền nguồn dữ liệu hoặc truyền đồng thời cả `--pcap` và `--interface`, chương trình cần từ chối chạy và hiển thị thông báo lỗi phù hợp.

### 3.4.2. Yêu cầu lọc gói tin

Hệ thống cần hỗ trợ bộ lọc BPF thông qua tham số `--filter <expression>` hoặc cấu hình trong file YAML. Bộ lọc giúp giới hạn loại gói tin được đưa vào phân tích, từ đó giảm tải xử lý và tập trung vào các giao thức có giá trị cho phát hiện tài sản.

Ví dụ bộ lọc thường dùng:

```bash
arp or udp port 67 or udp port 68
```

Bộ lọc trên cho phép xử lý gói ARP và DHCP. Nếu biểu thức BPF không hợp lệ, hệ thống cần báo lỗi trước hoặc trong quá trình mở nguồn capture, không được tiếp tục chạy với cấu hình sai.

### 3.4.3. Yêu cầu phân tích gói tin

Hệ thống cần phân tích các gói tin Ethernet và trích xuất thông tin từ một số giao thức phục vụ nhận diện tài sản. Các giao thức chính bao gồm:

- **ARP:** trích xuất địa chỉ IP và MAC.
- **DHCP:** trích xuất địa chỉ MAC, địa chỉ IP được cấp phát và hostname nếu có.
- **DNS:** ghi nhận endpoint và nguồn phát hiện liên quan đến truy vấn mạng.
- **NBNS, SSDP, TCP hoặc các plugin mở rộng:** bổ sung khả năng quan sát endpoint, dịch vụ hoặc metadata khi lưu lượng có chứa thông tin phù hợp.

Việc phân tích gói tin cần kiểm tra độ dài buffer trước khi đọc dữ liệu để tránh lỗi khi gặp packet bị cắt ngắn hoặc không hợp lệ.

### 3.4.4. Yêu cầu tạo observation

Sau khi phân tích gói tin, hệ thống cần tạo ra các bản ghi quan sát tạm thời, gọi là `AssetObservation`. Một observation thể hiện một lần hệ thống nhìn thấy thông tin liên quan đến tài sản mạng.

Một observation cần có các thông tin cơ bản:

- Địa chỉ MAC nếu xác định được.
- Địa chỉ IP nếu xác định được.
- Hostname nếu giao thức cung cấp.
- Giao thức hoặc nguồn phát hiện, ví dụ `arp`, `dhcp`, `dns`.
- Thời điểm quan sát gói tin.
- Metadata bổ sung nếu có.

Observation không phải là tài sản cuối cùng. Nhiều observation có thể thuộc cùng một thiết bị và cần được gom lại ở bước sau.

### 3.4.5. Yêu cầu tổng hợp tài sản mạng

Hệ thống cần gom các observation thành danh sách tài sản mạng. Tài sản được định danh chủ yếu bằng địa chỉ MAC đã chuẩn hóa. Khi gặp observation mới:

- Nếu MAC chưa tồn tại, hệ thống tạo asset mới.
- Nếu MAC đã tồn tại, hệ thống cập nhật asset hiện có.
- Nếu observation có IP mới, IP được thêm vào danh sách IP của asset.
- Nếu observation có hostname, hostname được cập nhật hoặc ghi nhận thay đổi.
- `first_seen` được cập nhật theo timestamp sớm nhất.
- `last_seen` được cập nhật theo timestamp mới nhất.
- `discovery_sources` được tích lũy theo các giao thức đã quan sát.

Cơ chế này giúp hệ thống tránh tạo nhiều bản ghi trùng lặp cho cùng một thiết bị.

### 3.4.6. Yêu cầu phát hiện sự kiện

Ngoài việc tạo danh sách tài sản cuối cùng, hệ thống cần ghi nhận các sự kiện quan trọng trong quá trình quan sát. Các sự kiện giúp người dùng hiểu diễn biến mạng theo thời gian, không chỉ trạng thái cuối.

Các loại sự kiện cần hỗ trợ gồm:

- `new_asset`: phát hiện MAC mới lần đầu.
- `mac_changed_for_ip`: một IP được gắn với MAC khác.
- `ip_changed_for_mac`: một MAC xuất hiện với IP mới.
- `ip_mac_flip_flop`: IP và MAC thay đổi qua lại trong khoảng thời gian ngắn.
- `asset_reappeared`: thiết bị xuất hiện lại sau thời gian không hoạt động.
- `hostname_learned`: học được hostname mới.
- `hostname_changed`: hostname của một MAC thay đổi.
- `ethernet_arp_mac_mismatch`: MAC lớp Ethernet khác MAC khai báo trong ARP.
- `non_local_source_ip`: IP nguồn không thuộc dải mạng nội bộ đã cấu hình.

Sự kiện cần có mức độ nghiêm trọng, thông điệp mô tả, timestamp, IP, MAC, giao thức và metadata bổ sung nếu có.

### 3.4.7. Yêu cầu xuất kết quả

Hệ thống cần hỗ trợ nhiều định dạng đầu ra để phục vụ các mục đích khác nhau:

- **Table:** dễ đọc trực tiếp trên terminal.
- **JSON:** phù hợp cho tích hợp với công cụ khác.
- **CSV:** phù hợp cho nhập vào bảng tính hoặc xử lý dữ liệu đơn giản.
- **NDJSON event log:** ghi lịch sử sự kiện theo từng dòng JSON.
- **PostgreSQL:** lưu snapshot tài sản và lịch sử sự kiện vào cơ sở dữ liệu.

Người dùng có thể chọn định dạng summary cuối bằng tham số:

```bash
--output table
--output json
--output csv
```

Đối với event log, hệ thống ghi mặc định vào file `logs/events.ndjson` hoặc đường dẫn được cấu hình bằng biến môi trường `ASSET_DISCOVERY_EVENTS_JSON`.

### 3.4.8. Yêu cầu cấu hình hệ thống

Hệ thống cần hỗ trợ cấu hình linh hoạt qua nhiều nguồn:

- Giá trị mặc định trong chương trình.
- File `configs/default.yaml` nếu tồn tại.
- File cấu hình chỉ định bằng `--config <file>`.
- Profile chỉ định bằng `--profile <name>`, tương ứng với `configs/<name>.yaml`.
- Biến môi trường và file `.env`.
- Tham số dòng lệnh.

Thứ tự ưu tiên cấu hình cần rõ ràng, trong đó tham số dòng lệnh có độ ưu tiên cao nhất. Nguồn packet vẫn phải truyền rõ bằng CLI, không đặt trong YAML. Các thông tin nhạy cảm như mật khẩu database không truyền qua CLI mà dùng biến môi trường hoặc `.env`.

### 3.4.9. Yêu cầu lưu trữ PostgreSQL

Hệ thống cần lưu được trạng thái tài sản vào bảng `assets` và lịch sử sự kiện vào bảng `asset_events`. Việc ghi tài sản cần sử dụng cơ chế upsert để nếu chạy lại cùng một file PCAP, hệ thống không tạo trùng bản ghi theo MAC.

Các thông tin chính trong bảng `assets` gồm:

- `mac_address`
- `ip_addresses`
- `hostname`
- `first_seen`
- `last_seen`
- `discovery_sources`
- metadata bổ sung
- `updated_at`

Các thông tin chính trong bảng `asset_events` gồm:

- `event_time`
- `event_type`
- `severity`
- `ip_address`
- `mac_address`
- `old_ip`, `new_ip`, `old_mac`, `new_mac`
- `hostname`
- `protocol`
- `interface`
- `message`
- `metadata`

### 3.4.10. Yêu cầu đóng gói và triển khai

Hệ thống cần có khả năng build và chạy bằng Docker. Docker giúp chuẩn hóa môi trường triển khai, giảm phụ thuộc vào máy người dùng và thuận tiện cho demo.

Các yêu cầu triển khai gồm:

- Có `Dockerfile` để build binary và tạo image runtime.
- Có `docker-compose.yml` để chạy PostgreSQL và demo PCAP.
- Hỗ trợ chạy PCAP trong container không cần quyền đặc biệt.
- Hỗ trợ live capture trong container trên Linux host với quyền `NET_ADMIN`, `NET_RAW` và `--net=host`.

## 3.5. Yêu cầu phi chức năng

### 3.5.1. Hiệu năng

Hệ thống cần xử lý lưu lượng mạng với hiệu năng đủ tốt cho môi trường mạng nội bộ. Ở chế độ PCAP, chương trình cần đọc và xử lý file nhanh, không tạo độ trễ không cần thiết. Ở chế độ live capture, hệ thống cần tách quá trình bắt gói tin khỏi quá trình phân tích để tránh việc parser hoặc output làm chậm capture.

Thiết kế live pipeline cần sử dụng hàng đợi có giới hạn để tránh tăng bộ nhớ không kiểm soát khi lưu lượng cao. Nếu queue đầy, hệ thống cần ghi nhận số lượng batch bị drop thay vì tiếp tục cấp phát bộ nhớ vô hạn.

### 3.5.2. Tính ổn định

Hệ thống cần hoạt động ổn định khi gặp packet không hợp lệ, packet bị cắt ngắn, file PCAP rỗng, file không tồn tại hoặc filter sai cú pháp. Các lỗi này cần được xử lý rõ ràng, không làm chương trình crash không kiểm soát.

Khi live capture nhận `SIGINT` hoặc `SIGTERM`, hệ thống cần dừng graceful, đóng queue, flush event sink, ghi snapshot cuối và xuất summary.

### 3.5.3. Tính mở rộng

Hệ thống cần có kiến trúc parser dạng plugin để có thể bổ sung giao thức mới mà không phải sửa toàn bộ luồng xử lý. Parser core chỉ nên điều phối plugin, còn logic phân tích từng giao thức nằm trong module riêng.

Mô hình `AssetObservation` cần đủ linh hoạt để chứa metadata mở rộng, giúp thêm giao thức mới mà không cần thay đổi schema chính quá nhiều.

### 3.5.4. Khả năng cấu hình

Các tham số ít thay đổi như filter mặc định, backend capture, output format, event rate limit, local network và ignore network nên được đặt trong YAML. Các thông tin phụ thuộc môi trường như database host, user, password và event file path nên đi qua `.env` hoặc biến môi trường.

Thiết kế này giúp lệnh chạy hằng ngày ngắn gọn hơn, đồng thời tránh đưa thông tin nhạy cảm vào command line.

### 3.5.5. Khả năng quan sát và kiểm tra

Hệ thống cần cung cấp output dễ kiểm tra bằng người dùng và bằng test tự động. JSON và CSV cần có cấu trúc ổn định. Event NDJSON cần mỗi dòng là một JSON object độc lập để dễ phân tích bằng script hoặc công cụ log.

Live pipeline cần có metrics như số packet captured, parsed, dropped, throughput, backend selected và kernel drops nếu backend hỗ trợ. Các chỉ số này giúp đánh giá tình trạng vận hành khi chạy thực tế.

### 3.5.6. Bảo mật và quyền riêng tư

Hệ thống chỉ nên thu thập những thông tin cần thiết cho mục tiêu phát hiện tài sản. File PCAP, event log và database có thể chứa thông tin nhạy cảm như địa chỉ MAC, IP, hostname và tên dịch vụ, nên cần được quản lý phù hợp.

Thông tin kết nối database không được khuyến khích truyền qua CLI vì có thể lộ trong shell history hoặc process list. Vì vậy hệ thống sử dụng `.env`, `DATABASE_URL`, `PG*` hoặc `DB_*`.

## 3.6. Yêu cầu dữ liệu

### 3.6.1. Dữ liệu đầu vào

Dữ liệu đầu vào của hệ thống gồm:

- File PCAP chứa các gói tin mạng đã thu thập.
- Luồng packet trực tiếp từ network interface.
- Biểu thức BPF để lọc packet.
- File YAML chứa policy và runtime config.
- Biến môi trường hoặc `.env` chứa cấu hình database và event log.

Hệ thống hiện tập trung vào Ethernet datalink. Nếu file PCAP có datalink khác chưa hỗ trợ, hệ thống cần từ chối hoặc bỏ qua theo cách an toàn.

### 3.6.2. Dữ liệu trung gian

Dữ liệu trung gian gồm:

- Packet đã đọc từ PCAP hoặc interface.
- `PacketContext` chứa thông tin đã decode như Ethernet, IPv4, UDP và payload.
- `AssetObservation` sinh ra từ parser plugin.
- `AssetEvent` sinh ra từ event detector.

Dữ liệu trung gian cần được xử lý theo hướng không giữ pointer vào vùng nhớ packet đã hết hiệu lực, đặc biệt trong live capture và backend AF_PACKET.

### 3.6.3. Dữ liệu đầu ra

Dữ liệu đầu ra gồm danh sách tài sản và lịch sử sự kiện. Một tài sản mạng cần có các field chính:

| Field | Ý nghĩa |
|---|---|
| `mac_address` | Địa chỉ MAC chuẩn hóa, dùng làm khóa chính |
| `ip_addresses` | Danh sách IPv4 quan sát được |
| `hostname` | Tên thiết bị nếu thu thập được |
| `first_seen` | Thời điểm quan sát sớm nhất |
| `last_seen` | Thời điểm quan sát gần nhất |
| `discovery_sources` | Danh sách giao thức phát hiện |

## 3.7. Ca sử dụng chính

### 3.7.1. Use case 1: Phân tích file PCAP

Người dùng chạy chương trình với file PCAP:

```bash
./build/asset-discovery --profile pcap --pcap samples/multi-asset.pcap --output table
```

Hệ thống đọc file, áp dụng filter nếu có, phân tích packet, tạo observation, cập nhật asset store, ghi event, lưu database và in summary cuối.

**[PLACEHOLDER HÌNH 3.2: Chèn sequence diagram cho use case phân tích PCAP, gồm User, CLI, PacketCapture, ParserEngine, AssetMonitor, Renderer và PostgreSQL]**

**Hình 3.2. Luồng xử lý use case phân tích file PCAP**

### 3.7.2. Use case 2: Live capture trên interface

Người dùng chạy chương trình trên interface thật:

```bash
sudo ./build/asset-discovery --profile live --interface eth0
```

Hệ thống mở interface, bắt packet liên tục, xử lý qua live pipeline, ghi event thời gian thực và dừng khi người dùng nhấn `Ctrl+C` hoặc tiến trình nhận `SIGTERM`.

### 3.7.3. Use case 3: Xuất kết quả JSON để tích hợp

Người dùng chọn output JSON:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Kết quả trả về là mảng JSON gồm các asset. Định dạng này phù hợp để đưa vào script, API hoặc hệ thống xử lý dữ liệu khác.

### 3.7.4. Use case 4: Lưu kết quả vào PostgreSQL

Người dùng cấu hình `.env`:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
```

Sau đó chạy chương trình với PCAP hoặc interface. Hệ thống tự động ghi snapshot tài sản vào bảng `assets` và sự kiện vào bảng `asset_events`.

### 3.7.5. Use case 5: Chạy demo bằng Docker Compose

Người dùng chạy:

```bash
docker compose up --build pcap-demo
```

Docker Compose khởi động PostgreSQL, build image, chạy phân tích PCAP mẫu và giữ container để người dùng kiểm tra database.

## 3.8. Sơ đồ use case tổng quát

**[PLACEHOLDER HÌNH 3.3: Chèn use case diagram gồm các actor Quản trị viên mạng, Nhân sự an toàn thông tin, Người triển khai; các use case Chọn nguồn PCAP, Live capture, Cấu hình filter, Phân tích packet, Xem output, Ghi PostgreSQL, Xem event log, Chạy Docker demo]**

**Hình 3.3. Sơ đồ use case tổng quát của hệ thống**

## 3.9. Ràng buộc kỹ thuật

Hệ thống có một số ràng buộc kỹ thuật chính:

- Ngôn ngữ triển khai là C++17.
- Dự án sử dụng CMake để build.
- Packet capture phụ thuộc vào `libpcap` hoặc backend AF_PACKET trên Linux.
- Live capture cần quyền phù hợp như root hoặc `CAP_NET_RAW`.
- Docker live capture cần Linux host thật, host networking và capability mạng.
- PostgreSQL writer phụ thuộc `psql` có trong `PATH` và database reachable.
- Hệ thống hiện tập trung vào Ethernet frame.
- Phương pháp phát hiện là passive monitoring, không thực hiện scan chủ động.

## 3.10. Tiêu chí nghiệm thu

Hệ thống được xem là đáp ứng yêu cầu khi thỏa mãn các tiêu chí sau:

- Chạy được với file PCAP mẫu.
- Phát hiện được asset từ gói ARP.
- Trích xuất được hostname từ DHCP khi packet có DHCP option phù hợp.
- Xuất được kết quả ở các định dạng table, JSON và CSV.
- Ghi được event log dạng NDJSON.
- Ghi được tài sản và sự kiện vào PostgreSQL khi cấu hình database hợp lệ.
- Hỗ trợ live capture trên interface khi có quyền phù hợp.
- Build và chạy được bằng Docker.
- Có test tự động cho parser, renderer, asset store, event detector, config, CLI, capture backend và PostgreSQL writer.
- Xử lý được các lỗi phổ biến như thiếu input, file PCAP không tồn tại, filter sai và tham số CLI không hợp lệ.

## 3.11. Tổng kết chương

Chương này đã phân tích các yêu cầu chính của hệ thống Passive Network Asset Discovery System. Hệ thống cần hỗ trợ phát hiện tài sản mạng thụ động từ PCAP hoặc live interface, phân tích các giao thức có giá trị, tổng hợp observation thành asset, ghi nhận sự kiện, xuất kết quả ở nhiều định dạng và triển khai được bằng Docker.

Các yêu cầu chức năng và phi chức năng trong chương này là cơ sở để xây dựng kiến trúc hệ thống ở chương tiếp theo. Chương 4 sẽ trình bày chi tiết cách hệ thống được thiết kế thành các module, cách dữ liệu đi qua pipeline và cách các thành phần phối hợp để đáp ứng yêu cầu đã nêu.
