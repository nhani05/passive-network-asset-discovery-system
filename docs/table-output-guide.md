# Hướng Dẫn Chạy Và Kiểm Thử Table Output

Tài liệu này mô tả cách chạy và kiểm chứng table output đã triển khai từ vấn đề #9.

## Vấn Đề #9 Bổ Sung Gì

Chế độ `--output table` render kết quả discovery trực tiếp ra terminal. Phần triển khai hiện tại:

- in bảng asset với các cột `MAC`, `IPs`, `First Seen`, `Last Seen`, `Sources`
- gom nhiều IP và source theo thứ tự ổn định
- dùng thứ tự asset ổn định từ asset state
- in empty state rõ ràng khi input hợp lệ nhưng không phát hiện asset
- tích hợp table renderer vào CLI ở chế độ đọc PCAP và live interface

JSON output và interactive UI chưa được triển khai trong slice này.

## Điều Kiện Cần Có

- CMake 3.16 trở lên.
- Trình biên dịch hỗ trợ C++17.
- Gói phát triển libpcap trên Linux/macOS, hoặc Npcap SDK trên Windows, để bật khả năng đọc PCAP.

Project vẫn build được khi không có libpcap nếu `ASSET_DISCOVERY_REQUIRE_PCAP=OFF`, nhưng binary ở chế độ đó không thể chạy `--pcap`.

## Biên Dịch

Từ thư mục gốc của repository:

```sh
cmake -S . -B build
cmake --build build
```

Trên Windows với Npcap SDK, cấu hình `NPCAP_ROOT` trỏ tới thư mục SDK:

```powershell
cmake -S . -B build-npcap -DNPCAP_ROOT="C:\Path\To\Npcap-SDK"
cmake --build build-npcap
```

## Chạy PCAP Mẫu Ở Table Mode

Linux/macOS:

```sh
./build/asset-discovery --pcap samples/arp.pcap --output table
```

Kết quả mong đợi:

```text
MAC                IPs                     First Seen        Last Seen         Sources
--------------------------------------------------------------------------------------
02:42:ac:11:00:02  192.168.1.10            1699606784.0      1699606784.0      arp
```

Windows PowerShell:

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap --output table
```

`table` là output mặc định, nên lệnh sau cho cùng kết quả:

```sh
./build/asset-discovery --pcap samples/arp.pcap
```

## Empty State

Với PCAP hợp lệ nhưng không có asset từ ARP observation, table mode in thông báo ổn định:

```text
No assets discovered.
```

Fixture PCAP rỗng được sinh trong build khi có libpcap và test được bật. Có thể chạy trực tiếp sau khi build:

```sh
./build/asset-discovery --pcap build/tests/empty.pcap --output table
```

Empty state chỉ áp dụng cho input đọc được nhưng không tạo asset. Các lỗi như thiếu file, PCAP không đọc được, hoặc binary không có backend libpcap vẫn trả về lỗi thay vì empty state.

## Kiểm Chứng Tập Trung

Chạy riêng các test liên quan đến vấn đề #9:

```sh
ctest --test-dir build -R 'table-renderer-tests|asset-discovery-pcap-sample|asset-discovery-pcap-empty-table' --output-on-failure
```

Kết quả mong đợi:

```text
100% tests passed, 0 tests failed out of 3
```

Các test này kiểm tra:

- renderer unit test cho một asset, nhiều IP/source và empty state
- CLI đọc `samples/arp.pcap` và in asset table
- CLI đọc PCAP rỗng và in empty state

## Chạy Toàn Bộ Test Suite

Chạy toàn bộ CTest suite:

```sh
ctest --test-dir build --output-on-failure
```

Khi build có libpcap, suite hiện bao gồm parser, asset state, table renderer và các test CLI PCAP:

```text
100% tests passed, 0 tests failed out of 9
```

## Giới Hạn Hiện Tại

- `--output json` được parse nhưng chưa có implementation; CLI trả lỗi khi dùng format này.
- Live interface mode cần quyền capture phù hợp với hệ điều hành và backend libpcap/Npcap.
- Table output hiện phản ánh asset phát hiện từ parser đã triển khai, trong đó source đang được kiểm chứng là `arp`.
