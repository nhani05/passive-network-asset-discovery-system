# Đề Xuất Hướng Cải Tiến Lập Trình Hệ Thống

Tài liệu này đề xuất các hướng phát triển tiếp theo để Passive Network Asset Discovery System trở thành một dự án mạnh hơn về lập trình hệ thống: xử lý packet hiệu năng cao, kiểm soát tài nguyên chặt chẽ, concurrency rõ ràng, parser an toàn, runtime có khả năng quan sát tốt, và triển khai gần với môi trường production hơn.

## Bối Cảnh Hiện Tại

Hệ thống hiện đã có nền tảng tốt cho một project C++17:

- Đọc packet từ PCAP hoặc network interface thông qua libpcap/Npcap.
- Hỗ trợ BPF filter để giảm traffic trước khi parse.
- Có pipeline live capture kiểu producer-consumer với bounded queue, parser worker pool, aggregator single-writer, và metrics cơ bản.
- Parser được tổ chức theo plugin tĩnh cho ARP, DHCP, DNS.
- Asset state được gộp theo MAC address và xuất ra table, JSON, CSV, hoặc PostgreSQL.
- Có test CTest, sample PCAP, Dockerfile, Docker Compose, và tài liệu build/deploy/demo.

Điểm cần cải tiến không phải là thay toàn bộ kiến trúc, mà là nâng từng tầng theo hướng hệ thống hơn: ít copy hơn, ít lock contention hơn, đo được bottleneck, chịu lỗi tốt hơn, và có cơ chế kiểm thử parser nghiêm ngặt hơn.

## Mục Tiêu Cải Tiến

| Mục tiêu | Ý nghĩa |
| --- | --- |
| Tăng throughput | Xử lý được nhiều packet hơn mỗi giây, nhất là khi live capture trên interface thật. |
| Giảm packet drop | Có backpressure, batch sizing, và thống kê drop rõ ràng ở từng stage. |
| Giảm copy và allocation | Dùng view, buffer pool, memory arena, và batch ownership rõ ràng. |
| Tăng độ an toàn parser | Chống packet malformed, truncation, integer overflow, và fuzz crash. |
| Tăng khả năng quan sát | Có metrics, benchmark, profiling, latency histogram, và báo cáo bottleneck. |
| Tăng tính production | Có service mode, config file, privilege handling, storage retry, và graceful shutdown. |

## Hướng 1: Capture Backend Hiệu Năng Cao

Libpcap là baseline tốt vì portable và dễ demo. Tuy nhiên, để dự án nổi bật hơn về lập trình hệ thống, nên tách rõ capture abstraction và bổ sung backend tối ưu cho Linux.

Đề xuất:

- Giữ `PacketCapture` hiện tại làm backend portable.
- Tạo interface capture backend độc lập, ví dụ `CaptureBackend`, để main flow không phụ thuộc trực tiếp vào libpcap.
- Thêm Linux backend dùng `AF_PACKET` với `PACKET_RX_RING`/`TPACKET_V3`.
- Dùng memory-mapped ring buffer để giảm syscall và giảm copy khi đọc packet.
- Duy trì thống kê per-backend: packet received, packet copied, kernel drops, queue drops, batch drops.
- Cho phép chọn backend bằng CLI hoặc config: `pcap`, `af-packet`, `auto`.

Lợi ích:

- Cho thấy năng lực làm việc với Linux networking primitives.
- Tách rõ portability và performance path.
- Có thể benchmark công bằng giữa libpcap và native Linux backend.

Rủi ro:

- `AF_PACKET` phụ thuộc Linux, không thay thế hoàn toàn libpcap/Npcap.
- Cần quản lý memory ring cẩn thận để tránh đọc packet sau khi frame đã được kernel tái sử dụng.

## Hướng 2: Packet View Và Zero-Copy Parsing

Parser hiện nên được đẩy dần về mô hình chỉ đọc trên vùng nhớ packet, tránh copy payload không cần thiết.

Đề xuất:

- Chuẩn hóa kiểu `PacketView` hoặc `ByteView` dựa trên pointer + length.
- Parser chỉ nhận view bất biến, không sở hữu memory.
- Decode lazy: chỉ decode IPv4/UDP/DHCP/DNS khi plugin thật sự cần.
- Không tạo `std::string` sớm cho field nhị phân như MAC/IP; chỉ format khi output hoặc storage cần.
- Dùng kiểu thời gian chuẩn như `std::chrono` hoặc struct timestamp riêng, không lưu timestamp nội bộ dưới dạng string.

Lợi ích:

- Giảm allocation và copy trên hot path.
- Parser dễ fuzz hơn vì input là buffer + length rõ ràng.
- Dữ liệu nội bộ sạch hơn, tránh format text quá sớm.

## Hướng 3: Concurrency Và Backpressure

Pipeline hiện có bounded queue và worker pool, đây là nền tảng tốt. Bước tiếp theo là đo và giảm contention.

Đề xuất:

- Dùng batch cố định kích thước có thể cấu hình: số packet mỗi batch, số observation mỗi batch.
- Thử nghiệm SPSC/MPSC ring queue cho đường capture-to-parser và parser-to-aggregator.
- Đưa policy drop thành cấu hình rõ ràng: drop newest, drop oldest, block with timeout.
- Shard asset aggregation theo hash của MAC address để giảm bottleneck single-writer khi traffic lớn.
- Mỗi shard có `AssetStore` riêng, cuối phiên merge thành output thống nhất.
- Có thể pin worker vào CPU core khi benchmark để đo ổn định hơn.

Lợi ích:

- Tránh để aggregator single-writer trở thành điểm nghẽn khi số parser worker tăng.
- Biến backpressure thành hành vi có thể giải thích và kiểm chứng.
- Dễ tạo benchmark theo từng stage.

Tiêu chí đo:

- Packet/s tổng.
- Observation/s.
- Queue high watermark.
- Drop rate từng queue.
- Latency từ capture đến apply observation.
- CPU utilization theo thread.

## Hướng 4: Quản Lý Bộ Nhớ Theo Kiểu Hệ Thống

Khi xử lý packet liên tục, allocation nhỏ lẻ trên hot path dễ gây jitter và giảm throughput.

Đề xuất:

- Dùng object pool hoặc `std::pmr` cho batch, observation, và metadata tạm.
- Tối ưu container trong `AssetStore`: tránh copy vector/string nhiều lần khi merge observation.
- Chuẩn hóa MAC address thành kiểu binary 6 byte nội bộ, chỉ render thành text ở boundary output.
- Lưu IPv4 dưới dạng `uint32_t` hoặc struct riêng thay vì string.
- Dùng small-vector style container cho danh sách IP/source thường có ít phần tử.
- Thêm giới hạn memory: số asset tối đa, số IP tối đa trên một asset, TTL/aging cho asset cũ.

Lợi ích:

- Hot path ít phụ thuộc allocator mặc định.
- Asset state compact hơn.
- Có kiểm soát khi traffic bất thường hoặc cố tình gây nhiễu.

## Hướng 5: Parser Robustness Và Fuzzing

Packet parser là vùng rủi ro cao nhất vì input đến từ mạng hoặc PCAP không tin cậy.

Đề xuất:

- Thêm fuzz target cho Ethernet, ARP, DHCP, DNS, và `PacketParserFacade`.
- Chạy ASAN/UBSAN trong CI cho test và fuzz corpus nhỏ.
- Chạy TSAN cho pipeline concurrency ở test riêng.
- Thêm corpus PCAP malformed: frame cắt ngắn, header length sai, DHCP option thiếu terminator, DNS name compression loop.
- Thêm property test đơn giản: parser không crash, không đọc ngoài buffer, không tạo observation thiếu MAC hợp lệ.
- Đặt rule parser: mọi offset phải kiểm tra length trước khi đọc, mọi length từ packet phải được validate trước khi cộng offset.

Lợi ích:

- Tăng chất lượng lập trình hệ thống rõ rệt.
- Dễ chứng minh parser an toàn hơn bằng evidence tự động.
- Giảm lỗi khó tái hiện khi chạy trên traffic thật.

## Hướng 6: Storage Và Durability

PostgreSQL writer hiện ghi qua `psql`, phù hợp demo và giảm dependency C++. Để production hơn, nên tách storage thành pipeline bất đồng bộ.

Đề xuất:

- Tạo `AssetSink` interface cho output/storage.
- Thêm batch writer: gom nhiều asset trước khi upsert.
- Dùng native PostgreSQL client library ở giai đoạn sau nếu muốn loại bỏ phụ thuộc shell `psql`.
- Có retry/backoff khi database tạm thời không reachable.
- Có local spool file dạng append-only khi database down, sau đó replay.
- Tách output stdout khỏi storage side effect để CLI dễ kiểm soát hơn.

Lợi ích:

- Không để database chậm làm ảnh hưởng capture path.
- Có câu chuyện rõ ràng về durability khi chạy dài hạn.
- Dễ thêm sink mới như file NDJSON, Kafka, hoặc HTTP collector.

## Hướng 7: Observability, Benchmark Và Profiling

Một dự án hệ thống mạnh cần đo được mình đang nhanh hay chậm ở đâu.

Đề xuất:

- Thêm metrics endpoint hoặc metrics file định kỳ cho service mode.
- Ghi metrics theo stage: capture, parse, aggregate, render, storage.
- Thêm latency histogram thay vì chỉ tổng counter.
- Thêm benchmark replay PCAP với nhiều profile: ARP-only, DHCP-heavy, DNS-heavy, mixed traffic, malformed traffic.
- Thêm tài liệu benchmark reproducible: CPU, kernel, compiler, build type, PCAP size, filter, backend, worker count.
- Dùng `perf`, flamegraph, hoặc sanitizer reports làm evidence tối ưu.

Chỉ số nên theo dõi:

- Packet/s.
- Bytes/s.
- Observation/s.
- p50/p95/p99 latency.
- Drops ở kernel, capture queue, observation queue.
- Memory peak.
- CPU time theo thread.

## Hướng 8: Runtime Security Và Privilege Handling

Live capture thường cần quyền cao. Dự án nên thể hiện khả năng giảm quyền sau khi mở capture handle.

Đề xuất:

- Tách bước mở interface khỏi bước xử lý packet.
- Sau khi mở capture socket, drop privilege nếu chạy trên Linux.
- Tài liệu rõ capability cần dùng: `CAP_NET_RAW`, `CAP_NET_ADMIN`.
- Thêm allowlist interface trong config để tránh capture nhầm interface nhạy cảm.
- Giới hạn output metadata để tránh vô tình lưu dữ liệu riêng tư.
- Với Docker, tiếp tục chạy user không phải root cho PCAP mode; chỉ cấp capability cho live mode.
- Xem xét seccomp/AppArmor profile tối thiểu cho container live capture.

Lợi ích:

- Dự án không chỉ chạy được, mà còn có tư duy hardening.
- Phù hợp hơn khi demo trong môi trường thật.

## Hướng 9: Mở Rộng Protocol Một Cách Có Kiểm Soát

Mở rộng protocol giúp phát hiện asset tốt hơn, nhưng phải giữ nguyên nguyên tắc passive và tránh thu thập quá mức.

Ưu tiên protocol:

| Protocol | Giá trị asset discovery | Ghi chú |
| --- | --- | --- |
| IPv6 NDP | Phát hiện asset IPv6, router, neighbor | Cần mở rộng data model cho IPv6. |
| mDNS | Hostname và service nội bộ | Cần chính sách privacy rõ ràng. |
| LLMNR/NBNS | Hostname trong LAN Windows | Hữu ích khi DHCP thiếu hostname. |
| TLS ClientHello | SNI để gợi ý service | Không decrypt, chỉ metadata thụ động. |
| HTTP User-Agent | Gợi ý loại thiết bị | Chỉ nên bật khi có cấu hình explicit. |

Đề xuất kỹ thuật:

- Giữ parser plugin stateless hoặc thread-safe.
- Mỗi protocol phải có fixture PCAP riêng.
- Mỗi protocol phải có fuzz target hoặc malformed packet tests.
- Metadata mới phải đi qua `AssetObservation::metadata` trước khi thêm field chính thức.

## Hướng 10: Service Mode Và Cấu Hình Dài Hạn

CLI hiện phù hợp demo và batch processing. Nếu chạy lâu dài, nên có service mode.

Đề xuất:

- Thêm config file dạng TOML/YAML/JSON tối giản cho interface, filter, backend, worker count, output sink, database, metrics.
- Thêm graceful shutdown bằng signal handling.
- Flush queue và storage trước khi thoát.
- Có log level: error, warn, info, debug.
- Có health status: capture running, database reachable, last packet time, drop rate.
- Tài liệu chạy bằng systemd trên Linux.

Lợi ích:

- Chuyển dự án từ demo tool thành daemon có thể vận hành.
- Dễ tích hợp vào môi trường lab hoặc hệ thống giám sát nội bộ.

## Lộ Trình Đề Xuất

### Giai Đoạn 1: Củng Cố Nền Tảng

- Chuẩn hóa `PacketView` và timestamp nội bộ.
- Thêm parser fuzz tests cho ARP/DHCP/DNS.
- Thêm ASAN/UBSAN build preset hoặc CI job.
- Bổ sung benchmark tài liệu hóa rõ ràng.
- Chuẩn hóa metrics theo stage trong live pipeline.

Kết quả mong đợi: parser an toàn hơn, benchmark lặp lại được, và hot path dễ tối ưu tiếp.

### Giai Đoạn 2: Tối Ưu Pipeline

- Thêm cấu hình batch size, worker count, queue capacity, drop policy.
- Thử ring queue hoặc queue tối ưu hơn cho pipeline.
- Shard `AssetStore` theo MAC hash.
- Tối ưu memory representation cho MAC/IP/source.
- Thêm storage batch writer.

Kết quả mong đợi: throughput tăng, contention giảm, memory ổn định hơn khi traffic lớn.

### Giai Đoạn 3: Linux Native Capture

- Tách capture backend interface.
- Giữ libpcap backend làm portable baseline.
- Thêm backend `AF_PACKET`/`TPACKET_V3` cho Linux.
- So sánh benchmark libpcap và native backend trên cùng PCAP replay hoặc traffic lab.
- Thêm privilege drop và capability documentation cho live mode.

Kết quả mong đợi: dự án thể hiện rõ năng lực lập trình hệ thống ở tầng kernel/userspace boundary.

### Giai Đoạn 4: Vận Hành Và Mở Rộng

- Thêm service mode và config file.
- Thêm metrics endpoint hoặc metrics export định kỳ.
- Thêm IPv6 NDP, mDNS, LLMNR/NBNS theo thứ tự ưu tiên.
- Thêm local spool/retry cho storage.
- Viết tài liệu vận hành và hardening.

Kết quả mong đợi: hệ thống có thể chạy lâu hơn, quan sát tốt hơn, và mở rộng protocol mà không phá kiến trúc hiện tại.

## Tiêu Chí Đánh Giá Thành Công

Một cải tiến nên được xem là hoàn tất khi có đủ:

- Code thay đổi rõ phạm vi, không làm phức tạp main flow quá mức.
- Unit test hoặc integration test tương ứng.
- Benchmark trước/sau nếu thay đổi hiệu năng.
- Metrics hoặc log giúp xác nhận hành vi runtime.
- Tài liệu cập nhật nếu thay đổi CLI, Docker, storage, hoặc cách vận hành.
- Không làm giảm tính portable của baseline PCAP mode.

## Kết Luận

Hướng phát triển tốt nhất là giữ kiến trúc hiện tại làm baseline ổn định, sau đó nâng cấp theo từng tầng: parser an toàn hơn, pipeline đo được bottleneck, memory ít allocation hơn, capture backend gần kernel hơn, và runtime có khả năng vận hành dài hạn. Như vậy dự án không chỉ đáp ứng yêu cầu passive asset discovery, mà còn thể hiện rõ năng lực lập trình hệ thống trong C++: xử lý I/O, quản lý memory, concurrency, kernel interface, security boundary, và observability.
