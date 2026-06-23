# Hướng Dẫn Sprint 0

Sprint 0 dùng để chuẩn bị nền móng cho dự án: cấu trúc thư mục, hệ thống build, tích hợp thư viện đọc packet, sample PCAP và cách kiểm chứng môi trường chạy.

Ở Sprint 0, chương trình **chưa phân tích packet thật**. Mục tiêu chính là đảm bảo dự án build được, chạy được CLI, nhận được đường dẫn PCAP và đã sẵn sàng cho Sprint 1.

## Mục Tiêu Sprint 0

- Tạo cấu trúc repository C++ rõ ràng.
- Cấu hình project CMake có thể build được.
- Tích hợp thư viện packet capture để chuẩn bị đọc PCAP/live capture.
- Thêm file PCAP mẫu có nội dung ARP hợp lệ.
- Viết tài liệu hướng dẫn chạy và kiểm chứng Sprint 0.

## Cấu Trúc Thư Mục

```text
.
|-- CMakeLists.txt
|-- README.md
|-- docker/
|-- docs/
|-- include/
|   |-- capture/
|   `-- cli/
|-- openspec/
|-- samples/
|-- scripts/
|-- src/
|   |-- asset/
|   |-- capture/
|   |-- cli/
|   |-- output/
|   |-- parser/
|   `-- main.cpp
`-- tests/
```

### Các File Ở Thư Mục Gốc

- `CMakeLists.txt`: Cấu hình build chính của dự án. File này định nghĩa executable `asset-discovery`, chuẩn C++17, test bằng CTest và cách tìm thư viện libpcap/Npcap SDK.
- `README.md`: Mô tả tổng quan đề tài và yêu cầu chính.
- `.gitignore`: Bỏ qua build output, file tạm, log, cache và các file môi trường cục bộ.

### Thư Mục Source Code

- `src/main.cpp`: Điểm bắt đầu của chương trình. File này nhận argument từ CLI, gọi parser để kiểm tra tham số, kiểm tra packet backend và in ra mode đang chạy.
- `src/cli/`: Chứa phần xử lý argument dòng lệnh, ví dụ `--pcap`, `--interface`, `--duration`, `--output`, `--help`.
- `include/cli/`: Chứa header tương ứng cho phần CLI.
- `src/capture/`: Chứa scaffold cho backend đọc packet. Trong Sprint 0, phần này chỉ kiểm tra libpcap/Npcap có khả dụng hay không.
- `include/capture/`: Chứa header tương ứng cho capture backend.
- `src/parser/`: Dành cho parser Ethernet, ARP, DHCP ở các sprint sau.
- `src/asset/`: Dành cho asset model và logic gom/merge asset ở các sprint sau.
- `src/output/`: Dành cho output dạng bảng và JSON ở các sprint sau.

### Thư Mục Test Và Dữ Liệu Mẫu

- `tests/`: Chứa cấu hình CTest cho các test cơ bản của CLI.
- `samples/`: Chứa dữ liệu mẫu dùng cho demo và test.
- `samples/arp.pcap`: File PCAP tối thiểu chứa một ARP request hợp lệ.
- `samples/README.md`: Mô tả nội dung file PCAP mẫu.

### Thư Mục Hỗ Trợ

- `docs/`: Chứa tài liệu sprint, yêu cầu dự án và thiết kế.
- `docker/`: Dành cho Dockerfile hoặc docker-compose ở các sprint sau.
- `scripts/`: Dành cho script tự động hóa, ví dụ script tạo sample hoặc kiểm tra demo.
- `openspec/`: Chứa tài liệu OpenSpec mô tả yêu cầu và thiết kế hành vi hệ thống.

## CLI Là Gì Trong Dự Án Này?

CLI là viết tắt của `Command Line Interface`, nghĩa là giao diện dòng lệnh.

Trong dự án này, thay vì có giao diện đồ họa, người dùng chạy chương trình bằng PowerShell hoặc terminal:

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap
```

Ý nghĩa:

- `asset-discovery.exe`: Chương trình chính của dự án.
- `--pcap samples\arp.pcap`: Yêu cầu chương trình chạy ở mode đọc file PCAP.
- `--help`: Hiển thị hướng dẫn sử dụng CLI.

CLI phù hợp cho giai đoạn đầu vì dễ build, dễ test, dễ chạy trong Docker và dễ tự động hóa.

## Thư Viện Packet

Dự án dùng API tương thích libpcap.

- Linux/macOS: dùng `libpcap`.
- Windows: dùng Npcap runtime và Npcap SDK.

Trong workspace hiện tại, đường dẫn Npcap SDK là:

```text
C:\npcap-sdk-1.16
```

CMake dùng biến `NPCAP_ROOT` để trỏ tới SDK này. Option `ASSET_DISCOVERY_REQUIRE_PCAP=ON` giúp CMake fail ngay nếu không tìm thấy thư viện packet, nhờ đó kết quả kiểm chứng Sprint 0 rõ ràng hơn.

## Luồng Chạy Của Dự Án Trong Sprint 0

Khi chạy lệnh:

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap
```

luồng xử lý hiện tại là:

```text
PowerShell command
    |
    v
asset-discovery.exe
    |
    v
src/main.cpp
    |
    v
parse CLI arguments
    |
    v
check Npcap/libpcap availability
    |
    v
print selected mode/output
    |
    v
exit
```

Chi tiết từng bước:

1. PowerShell gọi binary `asset-discovery.exe`.
2. `src/main.cpp` nhận các argument dòng lệnh.
3. Code trong `src/cli/Arguments.cpp` phân tích argument.
4. Chương trình kiểm tra người dùng có truyền đúng một input source hay không: `--pcap <file>` hoặc `--interface <name>`.
5. Code trong `src/capture/PacketCapture.cpp` kiểm tra lúc build có bật packet backend hay không.
6. Nếu CMake tìm thấy Npcap SDK, macro `ASSET_DISCOVERY_HAS_PCAP=1` được bật.
7. Chương trình in mode đang chạy và output format.
8. Chương trình kết thúc.

Output hiện tại:

```text
mode=pcap path=samples\arp.pcap
output=table
```

Điều quan trọng: ở Sprint 0, chương trình **chưa mở file PCAP để đọc packet**, **chưa parse ARP**, và **chưa tạo asset**. Các phần đó thuộc Sprint 1.

Luồng mong muốn ở Sprint 1 sẽ mở rộng thành:

```text
PCAP file
    |
    v
PCAP reader
    |
    v
Ethernet parser
    |
    v
ARP parser
    |
    v
Asset observation
    |
    v
Asset engine
    |
    v
Table/JSON output
```

## Cách Chạy Kiểm Chứng Sprint 0

Chạy các lệnh dưới đây từ thư mục gốc của repository.

### 1. Configure CMake Với Npcap SDK

```powershell
cmake -S . -B build-npcap -DNPCAP_ROOT=C:\npcap-sdk-1.16 -DASSET_DISCOVERY_REQUIRE_PCAP=ON
```

Kết quả mong đợi:

```text
-- Found Npcap SDK: C:\npcap-sdk-1.16
-- Configuring done
-- Generating done
```

Nếu bước này lỗi, kiểm tra:

- Có file `C:\npcap-sdk-1.16\Include\pcap.h`.
- Có file `C:\npcap-sdk-1.16\Lib\x64\wpcap.lib`.
- Có file `C:\npcap-sdk-1.16\Lib\x64\Packet.lib`.
- Đã cài Npcap runtime trên Windows.

### 2. Build CLI

```powershell
cmake --build build-npcap --config Debug
```

Kết quả mong đợi có dòng tương tự:

```text
asset-discovery.vcxproj -> ...\build-npcap\Debug\asset-discovery.exe
```

### 3. Chạy Test

```powershell
ctest --test-dir build-npcap -C Debug --output-on-failure
```

Kết quả mong đợi:

```text
100% tests passed, 0 tests failed out of 2
```

Các test Sprint 0 hiện kiểm tra:

- `asset-discovery --help` chạy thành công.
- Chạy chương trình mà không truyền input source thì fail đúng như mong đợi.

### 4. Chạy Help Của Binary

```powershell
.\build-npcap\Debug\asset-discovery.exe --help
```

Kết quả mong đợi:

- Chương trình in hướng dẫn sử dụng CLI.
- Exit code là `0`.

### 5. Chạy Với File PCAP Mẫu

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap
```

Kết quả mong đợi ở scaffold hiện tại:

```text
mode=pcap path=samples\arp.pcap
output=table
```

Không nên xuất hiện cảnh báo:

```text
libpcap-unavailable backend is not available
```

Nếu cảnh báo này xuất hiện, nghĩa là binary đang được build khi chưa tìm thấy Npcap/libpcap.

## Thông Tin File PCAP Mẫu

`samples/arp.pcap` là file PCAP hợp lệ tối thiểu, chứa một ARP request:

```text
magic=d4c3b2a1
linktype=1
packet_len=42
ethertype=0806
arp_opcode=0001
sender_mac=02:42:ac:11:00:02
sender_ip=192.168.1.10
target_ip=192.168.1.1
```

File này giúp Sprint 1 có input ổn định để triển khai:

- Đọc file PCAP.
- Nhận diện Ethernet frame.
- Parse ARP packet.
- Tạo asset observation.

## Checklist Sprint 0

- [x] Repository có nhánh local `main` và `develop`.
- [x] Có cấu trúc thư mục: `src`, `include`, `tests`, `docs`, `samples`, `docker`, `scripts`.
- [x] CMake configure thành công.
- [x] CMake build được executable `asset-discovery`.
- [x] CLI help chạy thành công.
- [x] Test CTest cơ bản pass.
- [x] Tích hợp Npcap SDK qua `NPCAP_ROOT`.
- [x] Có sample ARP PCAP trong `samples/`.
- [x] Có README skeleton.
- [x] Có external Scrum/Kanban board. Nên bổ sung link hoặc bằng chứng vào tài liệu dự án để dễ kiểm chứng.

## Giới Hạn Sau Sprint 0

- Chương trình chưa đọc nội dung file PCAP; hiện mới nhận và in đường dẫn.
- Chưa có ARP parser.
- Chưa có DHCP parser.
- Chưa có asset model.
- Chưa có logic merge/update asset.
- Chưa có output dạng bảng dữ liệu asset hoặc JSON thật.
- Chưa có Docker packaging hoàn chỉnh.

---

# Sprint 0 Guide

Sprint 0 prepares the project foundation: directory structure, build system, packet library integration, sample PCAP input, and verification steps for the local environment.

In Sprint 0, the program **does not analyze real packets yet**. The main goal is to make sure the project can be built, the CLI can run, a PCAP path can be accepted, and the codebase is ready for Sprint 1.

## Sprint 0 Goal

- Create a clear C++ repository structure.
- Configure a CMake project that can build successfully.
- Integrate a packet capture library for future PCAP/live capture work.
- Add a valid ARP sample PCAP file.
- Document how to run and verify Sprint 0.

## Project Structure

```text
.
|-- CMakeLists.txt
|-- README.md
|-- docker/
|-- docs/
|-- include/
|   |-- capture/
|   `-- cli/
|-- openspec/
|-- samples/
|-- scripts/
|-- src/
|   |-- asset/
|   |-- capture/
|   |-- cli/
|   |-- output/
|   |-- parser/
|   `-- main.cpp
`-- tests/
```

### Root Files

- `CMakeLists.txt`: Main build configuration. It defines the `asset-discovery` executable, C++17 settings, CTest integration, and libpcap/Npcap SDK discovery.
- `README.md`: High-level project overview and requirements.
- `.gitignore`: Ignores local build output, temporary files, logs, caches, and local environment files.

### Source Directories

- `src/main.cpp`: Program entry point. It receives CLI arguments, validates them, checks the packet backend, and prints the selected run mode.
- `src/cli/`: CLI argument parsing implementation for options such as `--pcap`, `--interface`, `--duration`, `--output`, and `--help`.
- `include/cli/`: Header files for the CLI module.
- `src/capture/`: Packet capture backend scaffold. In Sprint 0 it only reports whether libpcap/Npcap is available.
- `include/capture/`: Header files for the capture backend.
- `src/parser/`: Reserved for Ethernet, ARP, DHCP, and future protocol parsers.
- `src/asset/`: Reserved for the asset model and asset merge/update logic.
- `src/output/`: Reserved for table and JSON output renderers.

### Test And Sample Data Directories

- `tests/`: CTest configuration for basic CLI behavior.
- `samples/`: Sample input files for demos and tests.
- `samples/arp.pcap`: Minimal PCAP file containing one valid ARP request.
- `samples/README.md`: Documents the sample PCAP content.

### Support Directories

- `docs/`: Sprint notes, requirements, and design documents.
- `docker/`: Reserved for Dockerfile or docker-compose files in later sprints.
- `scripts/`: Reserved for automation scripts such as sample generation or demo checks.
- `openspec/`: OpenSpec artifacts describing expected system requirements and behavior.

## What CLI Means In This Project

CLI means `Command Line Interface`.

In this project, instead of using a graphical interface, users run the program from PowerShell or a terminal:

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap
```

Meaning:

- `asset-discovery.exe`: The main program.
- `--pcap samples\arp.pcap`: Tells the program to run in PCAP file mode.
- `--help`: Prints CLI usage instructions.

CLI is a good fit for the early stages because it is easy to build, test, automate, and run inside Docker.

## Packet Library

The project uses libpcap-compatible APIs.

- Linux/macOS: use `libpcap`.
- Windows: use Npcap runtime and Npcap SDK.

In this workspace, the Npcap SDK path is:

```text
C:\npcap-sdk-1.16
```

CMake uses the `NPCAP_ROOT` variable to locate this SDK. The option `ASSET_DISCOVERY_REQUIRE_PCAP=ON` makes CMake fail immediately if the packet library cannot be found, which makes Sprint 0 verification explicit.

## Runtime Flow In Sprint 0

When running:

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap
```

the current flow is:

```text
PowerShell command
    |
    v
asset-discovery.exe
    |
    v
src/main.cpp
    |
    v
parse CLI arguments
    |
    v
check Npcap/libpcap availability
    |
    v
print selected mode/output
    |
    v
exit
```

Detailed steps:

1. PowerShell starts the `asset-discovery.exe` binary.
2. `src/main.cpp` receives the command-line arguments.
3. `src/cli/Arguments.cpp` parses and validates the arguments.
4. The program checks that exactly one input source is provided: `--pcap <file>` or `--interface <name>`.
5. `src/capture/PacketCapture.cpp` checks whether the packet backend was enabled at build time.
6. If CMake found the Npcap SDK, `ASSET_DISCOVERY_HAS_PCAP=1` is enabled.
7. The program prints the selected mode and output format.
8. The program exits.

Current output:

```text
mode=pcap path=samples\arp.pcap
output=table
```

Important: in Sprint 0, the program **does not open and read the PCAP file yet**, **does not parse ARP yet**, and **does not create assets yet**. Those parts belong to Sprint 1.

The expected Sprint 1 flow will expand to:

```text
PCAP file
    |
    v
PCAP reader
    |
    v
Ethernet parser
    |
    v
ARP parser
    |
    v
Asset observation
    |
    v
Asset engine
    |
    v
Table/JSON output
```

## How To Verify Sprint 0

Run the following commands from the repository root.

### 1. Configure CMake With Npcap SDK

```powershell
cmake -S . -B build-npcap -DNPCAP_ROOT=C:\npcap-sdk-1.16 -DASSET_DISCOVERY_REQUIRE_PCAP=ON
```

Expected result:

```text
-- Found Npcap SDK: C:\npcap-sdk-1.16
-- Configuring done
-- Generating done
```

If this step fails, check:

- `C:\npcap-sdk-1.16\Include\pcap.h` exists.
- `C:\npcap-sdk-1.16\Lib\x64\wpcap.lib` exists.
- `C:\npcap-sdk-1.16\Lib\x64\Packet.lib` exists.
- Npcap runtime is installed on Windows.

### 2. Build The CLI

```powershell
cmake --build build-npcap --config Debug
```

Expected output includes:

```text
asset-discovery.vcxproj -> ...\build-npcap\Debug\asset-discovery.exe
```

### 3. Run Tests

```powershell
ctest --test-dir build-npcap -C Debug --output-on-failure
```

Expected result:

```text
100% tests passed, 0 tests failed out of 2
```

Current Sprint 0 tests verify:

- `asset-discovery --help` runs successfully.
- Running without an input source fails as expected.

### 4. Run Binary Help

```powershell
.\build-npcap\Debug\asset-discovery.exe --help
```

Expected result:

- The program prints CLI usage.
- Exit code is `0`.

### 5. Run With The Sample PCAP

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap
```

Expected output for the current scaffold:

```text
mode=pcap path=samples\arp.pcap
output=table
```

The program should not print:

```text
libpcap-unavailable backend is not available
```

If that warning appears, the binary was built without finding Npcap/libpcap.

## Sample PCAP Details

`samples/arp.pcap` is a minimal valid PCAP containing one ARP request:

```text
magic=d4c3b2a1
linktype=1
packet_len=42
ethertype=0806
arp_opcode=0001
sender_mac=02:42:ac:11:00:02
sender_ip=192.168.1.10
target_ip=192.168.1.1
```

This gives Sprint 1 a stable input for implementing:

- PCAP file reading.
- Ethernet frame detection.
- ARP packet parsing.
- Asset observation creation.

## Sprint 0 Checklist

- [x] Repository has local `main` and `develop` branches.
- [x] Project directories exist: `src`, `include`, `tests`, `docs`, `samples`, `docker`, and `scripts`.
- [x] CMake configures successfully.
- [x] CMake builds the `asset-discovery` executable.
- [x] CLI help runs successfully.
- [x] Basic CTest tests pass.
- [x] Npcap SDK is integrated through `NPCAP_ROOT`.
- [x] Sample ARP PCAP exists in `samples/`.
- [x] README skeleton exists.
- [x] External Scrum/Kanban board exists. Add a link or evidence to project documentation so it can be verified later.

## Limitations After Sprint 0

- The program does not read PCAP file contents yet; it only accepts and prints the path.
- ARP parser is not implemented yet.
- DHCP parser is not implemented yet.
- Asset model is not implemented yet.
- Asset merge/update logic is not implemented yet.
- Real asset table output and JSON output are not implemented yet.
- Docker packaging is not complete yet.
