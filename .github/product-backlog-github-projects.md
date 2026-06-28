# Product Backlog - GitHub Projects

Tài liệu này định nghĩa product backlog cho dự án Passive Network Asset Discovery System theo cách có thể chuyển trực tiếp thành GitHub Issues và quản lý bằng GitHub Projects.

## Project Setup

Project name: `Passive Network Asset Discovery`

Recommended view:
- `Board`: nhóm theo `Status`
- `Table`: hiển thị `Priority`, `Size`, `Milestone`, `Component`
- `Roadmap`: nhóm theo `Milestone`

Recommended fields:

| Field | Type | Values |
| --- | --- | --- |
| Status | Single select | Backlog, Ready, In progress, In review, Done |
| Priority | Single select | P0, P1, P2 |
| Size | Single select | S, M, L |
| Component | Single select | CLI, Capture, Parser, Asset State, Output, Tests, Delivery, Docs |
| Milestone | Single select | M1 Offline PCAP MVP, M2 Asset Metadata, M3 Live Capture, M4 Delivery |

Recommended labels:
- `type:feature`
- `type:test`
- `type:docs`
- `type:delivery`
- `component:cli`
- `component:capture`
- `component:parser`
- `component:asset-state`
- `component:output`
- `priority:p0`
- `priority:p1`
- `priority:p2`

## Milestones

| Milestone | Goal | Done when |
| --- | --- | --- |
| M1 Offline PCAP MVP | Read an Ethernet PCAP and discover assets from ARP traffic deterministically. | `asset-discovery --pcap samples/arp.pcap --output table` reports at least one asset with MAC, IP, first seen, and last seen. |
| M2 Asset Metadata | Enrich assets with protocol metadata and machine-readable output. | ARP and DHCP metadata are merged into stable asset records and JSON output is deterministic. |
| M3 Live Capture | Capture real traffic from an interface for a bounded duration. | `asset-discovery --interface <name> --duration <seconds>` captures packets and emits the same asset model as PCAP mode. |
| M4 Delivery | Package, document, and demonstrate the system. | Docker image, build/deploy docs, design docs, and demo steps are verified. |

## Backlog Items

### 1. Implement PCAP file reader

GitHub issue title: `Implement PCAP file reader`

User outcome: Người dùng có thể chạy chương trình với `--pcap <file>` để đọc packet từ file PCAP thay vì chỉ in mode cấu hình.

In scope:
- Mở và đọc file PCAP bằng libpcap khi `ASSET_DISCOVERY_HAS_PCAP=1`.
- Chuẩn hóa packet thành cấu trúc nội bộ gồm timestamp, link type, raw bytes.
- Trả lỗi rõ ràng khi file không tồn tại, không đọc được, hoặc link type chưa hỗ trợ.

Out of scope:
- Live capture.
- Phân tích protocol chi tiết ngoài việc chuyển packet thô vào pipeline.

Acceptance criteria:
- `asset-discovery --pcap samples/arp.pcap` đọc được ít nhất một packet.
- File PCAP không tồn tại trả exit code khác 0 và thông báo lỗi có đường dẫn.
- Build vẫn thành công khi không có libpcap nếu `ASSET_DISCOVERY_REQUIRE_PCAP=OFF`.
- Có test hoặc fixture validation cho `samples/arp.pcap`.

Metadata:
- Status: Ready
- Priority: P0
- Size: M
- Component: Capture
- Milestone: M1 Offline PCAP MVP
- Labels: `type:feature`, `component:capture`, `priority:p0`

Dependencies:
- libpcap/Npcap integration hiện có trong CMake.

Risks:
- Khác biệt API hoặc link flags giữa Linux, macOS, Windows/Npcap.

Validation:
- CMake configure/build with and without pcap requirement.
- CLI run against `samples/arp.pcap`.

### 2. Add Ethernet frame decoder

GitHub issue title: `Add Ethernet frame decoder`

User outcome: Hệ thống nhận biết được MAC nguồn, MAC đích và EtherType từ traffic Ethernet.

In scope:
- Decode Ethernet II frame.
- Expose source MAC, destination MAC, EtherType, payload.
- Bỏ qua frame quá ngắn với lỗi/metric có kiểm soát.

Out of scope:
- VLAN tags, IPv6 extension details, non-Ethernet link types.

Acceptance criteria:
- ARP sample frame trả đúng source MAC `02:42:ac:11:00:02`.
- Frame ngắn không crash.
- Decoder có unit test với byte fixture deterministic.

Metadata:
- Status: Ready
- Priority: P0
- Size: S
- Component: Parser
- Milestone: M1 Offline PCAP MVP
- Labels: `type:feature`, `component:parser`, `priority:p0`

Dependencies:
- PCAP reader hoặc byte-level unit fixture.

Risks:
- Cần tránh đọc lệch buffer và phụ thuộc alignment.

Validation:
- Unit tests for valid, truncated, and unsupported EtherType frames.

### 3. Parse ARP packets into observations

GitHub issue title: `Parse ARP packets into asset observations`

User outcome: Asset xuất hiện trong ARP traffic được phát hiện bằng MAC/IP.

In scope:
- Parse ARP over Ethernet/IPv4.
- Extract sender MAC, sender IP, target IP, operation.
- Emit asset observation từ sender MAC/IP.

Out of scope:
- DHCP parsing.
- Asset merge policy nâng cao.

Acceptance criteria:
- `samples/arp.pcap` tạo observation cho MAC `02:42:ac:11:00:02` và IP `192.168.1.10`.
- ARP packet không hợp lệ bị bỏ qua an toàn.
- Có test cho ARP request hợp lệ và packet thiếu byte.

Metadata:
- Status: Ready
- Priority: P0
- Size: M
- Component: Parser
- Milestone: M1 Offline PCAP MVP
- Labels: `type:feature`, `component:parser`, `priority:p0`

Dependencies:
- Ethernet decoder.

Risks:
- Endianness của field ARP và IPv4 cần xử lý rõ.

Validation:
- Unit tests and CLI smoke test with `samples/arp.pcap`.

### 4. Maintain asset state with first seen and last seen

GitHub issue title: `Maintain asset state with first seen and last seen`

User outcome: Người dùng thấy danh sách asset ổn định, có thời điểm xuất hiện đầu tiên và gần nhất.

In scope:
- Asset key ưu tiên MAC address.
- Merge IP observations vào cùng asset.
- Track `first_seen` và `last_seen` từ packet timestamps.

Out of scope:
- Persistent database.
- Expiration policy cho asset offline.

Acceptance criteria:
- Nhiều observation cùng MAC cập nhật `last_seen` nhưng giữ nguyên `first_seen`.
- Asset record chứa MAC, IP set/list, first seen, last seen.
- Có unit test cho merge và timestamp ordering.

Metadata:
- Status: Ready
- Priority: P0
- Size: M
- Component: Asset State
- Milestone: M1 Offline PCAP MVP
- Labels: `type:feature`, `component:asset-state`, `priority:p0`

Dependencies:
- ARP observations.

Risks:
- Cần định dạng timestamp nhất quán giữa table và JSON.

Validation:
- Unit tests for new asset, repeated asset, and added IP address.

### 5. Render asset table output

GitHub issue title: `Render asset table output`

User outcome: Người dùng đọc được kết quả discovery trực tiếp từ terminal.

In scope:
- Render table gồm MAC, IPs, first seen, last seen, source protocols.
- Empty state rõ ràng khi không phát hiện asset.
- Tích hợp với `--output table`.

Out of scope:
- JSON output.
- Interactive UI.

Acceptance criteria:
- `asset-discovery --pcap samples/arp.pcap --output table` in một dòng asset.
- Cột không bị lệch với dữ liệu cơ bản.
- Empty PCAP hoặc không có ARP in thông báo không có asset.

Metadata:
- Status: Ready
- Priority: P0
- Size: S
- Component: Output
- Milestone: M1 Offline PCAP MVP
- Labels: `type:feature`, `component:output`, `priority:p0`

Dependencies:
- Asset state.

Risks:
- Snapshot tests dễ giòn nếu spacing thay đổi.

Validation:
- CLI integration test checks key substrings, not full spacing.

### 6. Add deterministic CLI integration tests for PCAP mode

GitHub issue title: `Add deterministic CLI integration tests for PCAP mode`

User outcome: Nhóm phát triển biết ngay khi chức năng offline PCAP bị regression.

In scope:
- CTest integration test chạy binary với `samples/arp.pcap`.
- Verify exit code and expected MAC/IP output.
- Test invalid PCAP path.

Out of scope:
- Live capture tests requiring privileged interface access.

Acceptance criteria:
- `ctest` fails if ARP sample no longer reports expected asset.
- Invalid path test validates non-zero exit.
- Tests documented enough to run locally.

Metadata:
- Status: Ready
- Priority: P0
- Size: S
- Component: Tests
- Milestone: M1 Offline PCAP MVP
- Labels: `type:test`, `priority:p0`

Dependencies:
- PCAP reader, ARP parser, table output.

Risks:
- Test needs to skip or adapt when libpcap is unavailable.

Validation:
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

### 7. Implement JSON output

GitHub issue title: `Implement JSON output for discovered assets`

User outcome: Kết quả discovery có thể được dùng bởi script, pipeline hoặc dashboard khác.

In scope:
- Tích hợp `--output json`.
- Emit JSON array/object deterministic.
- Escape strings correctly.

Out of scope:
- External JSON dependency unless project chooses one explicitly.
- Streaming JSON.

Acceptance criteria:
- JSON output parse được bằng JSON parser chuẩn.
- Asset fields include `mac`, `ips`, `first_seen`, `last_seen`, `protocols`, optional `hostname`.
- Output ordering deterministic for tests.

Metadata:
- Status: Backlog
- Priority: P1
- Size: M
- Component: Output
- Milestone: M2 Asset Metadata
- Labels: `type:feature`, `component:output`, `priority:p1`

Dependencies:
- Asset state.

Risks:
- Handwritten JSON dễ lỗi escaping; cân nhắc dùng thư viện nhỏ nếu phù hợp.

Validation:
- Unit or integration test parses output and checks fields.

### 8. Parse DHCP metadata

GitHub issue title: `Parse DHCP metadata into asset records`

User outcome: Asset có thêm hostname và thông tin DHCP cơ bản khi traffic chứa DHCP.

In scope:
- Decode IPv4/UDP packets carrying DHCP.
- Extract client MAC, requested IP, hostname option, message type.
- Merge DHCP metadata vào asset state.

Out of scope:
- Full DHCP server behavior reconstruction.
- IPv6 DHCPv6.

Acceptance criteria:
- DHCP fixture tạo hoặc cập nhật asset với hostname khi option tồn tại.
- DHCP packet thiếu option hostname vẫn tạo observation bằng client MAC.
- Packet malformed không crash.

Metadata:
- Status: Backlog
- Priority: P1
- Size: L
- Component: Parser
- Milestone: M2 Asset Metadata
- Labels: `type:feature`, `component:parser`, `priority:p1`

Dependencies:
- Ethernet decoder.
- IPv4 and UDP minimal decoders.
- Asset state merge.

Risks:
- DHCP options variable-length parsing dễ lỗi bounds.

Validation:
- Unit tests for DHCP discover/request fixture and malformed options.

### 9. Add IPv4 and UDP minimal decoders

GitHub issue title: `Add IPv4 and UDP minimal decoders for DHCP support`

User outcome: Parser có nền tảng để nhận diện DHCP và metadata IP/port liên quan.

In scope:
- Decode IPv4 header length, protocol, source/destination IP.
- Decode UDP source/destination port and payload.
- Reject truncated headers safely.

Out of scope:
- TCP parsing.
- Fragment reassembly.

Acceptance criteria:
- Valid IPv4/UDP fixture returns expected addresses and ports.
- Non-UDP IPv4 packets are ignored cleanly.
- Fragmented packet behavior documented as unsupported or skipped.

Metadata:
- Status: Backlog
- Priority: P1
- Size: M
- Component: Parser
- Milestone: M2 Asset Metadata
- Labels: `type:feature`, `component:parser`, `priority:p1`

Dependencies:
- Ethernet decoder.

Risks:
- IPv4 options and fragmentation can complicate parsing.

Validation:
- Unit tests for minimum header, options header, truncated header, non-UDP.

### 10. Define asset model and output contract documentation

GitHub issue title: `Document asset model and output contract`

User outcome: Người dùng và người phát triển hiểu chính xác các field asset có nghĩa gì.

In scope:
- Document asset fields and source protocols.
- Document table and JSON output examples.
- Clarify timestamp format and merge rules.

Out of scope:
- Full system design document.

Acceptance criteria:
- Docs include one ARP-only example.
- Docs include one DHCP-enriched example.
- Field names match actual JSON output.

Metadata:
- Status: Backlog
- Priority: P1
- Size: S
- Component: Docs
- Milestone: M2 Asset Metadata
- Labels: `type:docs`, `priority:p1`

Dependencies:
- Asset model and JSON output decisions.

Risks:
- Documentation can drift if not updated with output tests.

Validation:
- Compare documented examples with CLI output from fixtures.

### 11. Implement bounded live capture

GitHub issue title: `Implement bounded live capture from interface`

User outcome: Người dùng có thể discovery asset từ traffic thật trên network interface trong thời gian giới hạn.

In scope:
- Open interface by name using libpcap.
- Capture until `--duration` expires.
- Feed captured packets into same parser/state pipeline as PCAP mode.
- Report permission/open errors clearly.

Out of scope:
- Daemon mode.
- Continuous service with persistence.

Acceptance criteria:
- `--interface <name> --duration 5` exits after approximately 5 seconds.
- Capture open failure returns non-zero exit with useful error.
- Shared pipeline avoids duplicate parser logic.

Metadata:
- Status: Backlog
- Priority: P1
- Size: L
- Component: Capture
- Milestone: M3 Live Capture
- Labels: `type:feature`, `component:capture`, `priority:p1`

Dependencies:
- PCAP reader pipeline.
- Parser and asset state.

Risks:
- Live capture may require elevated permissions or container capabilities.

Validation:
- Manual smoke test on known interface.
- Automated tests cover duration logic through injectable clock/source where practical.

### 12. Add capture filter support

GitHub issue title: `Add optional BPF capture filter support`

User outcome: Người dùng có thể giới hạn traffic cần xử lý, ví dụ chỉ ARP/DHCP.

In scope:
- Add CLI option such as `--filter <bpf>`.
- Apply filter to PCAP and live capture when libpcap is available.
- Report invalid filter syntax.

Out of scope:
- Custom filter language.

Acceptance criteria:
- `--filter arp` works for sample ARP PCAP.
- Invalid filter returns non-zero exit and libpcap error.
- Usage text documents filter syntax as BPF.

Metadata:
- Status: Backlog
- Priority: P2
- Size: M
- Component: CLI
- Milestone: M3 Live Capture
- Labels: `type:feature`, `component:cli`, `component:capture`, `priority:p2`

Dependencies:
- PCAP/live capture implementation.

Risks:
- Windows/Npcap filter behavior needs validation.

Validation:
- Integration test with ARP fixture and filter.

### 13. Create Docker image for CLI execution

GitHub issue title: `Create Docker image for asset discovery CLI`

User outcome: Người dùng build và chạy hệ thống bằng Docker như yêu cầu đề tài.

In scope:
- Dockerfile installs build/runtime dependencies.
- Build C++ project inside image.
- Document commands for PCAP file mode.

Out of scope:
- Kubernetes/deployment orchestration.

Acceptance criteria:
- `docker build` succeeds from repository root.
- Container can run against mounted `samples/arp.pcap`.
- Image contains libpcap runtime dependency.

Metadata:
- Status: Backlog
- Priority: P0
- Size: M
- Component: Delivery
- Milestone: M4 Delivery
- Labels: `type:delivery`, `priority:p0`

Dependencies:
- Offline PCAP MVP.

Risks:
- Live capture in Docker requires capabilities and host networking.

Validation:
- `docker build -t passive-asset-discovery .`
- `docker run --rm -v "$PWD/samples:/samples:ro" passive-asset-discovery --pcap /samples/arp.pcap`

### 14. Add Docker Compose for live capture demo

GitHub issue title: `Add Docker Compose live capture demo configuration`

User outcome: Người dùng có cấu hình mẫu để chạy live capture bằng container khi môi trường hỗ trợ.

In scope:
- Compose file with documented capabilities/network mode.
- Environment or command override for interface/duration.
- Clear warning about privileges.

Out of scope:
- Production service deployment.

Acceptance criteria:
- Compose config validates.
- README documents how to choose interface.
- Failure mode without privileges is documented.

Metadata:
- Status: Backlog
- Priority: P1
- Size: S
- Component: Delivery
- Milestone: M4 Delivery
- Labels: `type:delivery`, `priority:p1`

Dependencies:
- Docker image.
- Live capture.

Risks:
- Platform-specific Docker networking behavior.

Validation:
- `docker compose config`
- Manual run on Linux host where packet capture is permitted.

### 15. Write system design documentation

GitHub issue title: `Write system design documentation`

User outcome: Người chấm hoặc người vận hành hiểu kiến trúc, luồng dữ liệu và giới hạn của hệ thống.

In scope:
- Architecture overview.
- Packet flow: capture, decode, observation, asset state, output.
- Error handling and unsupported protocols.
- Build/deploy assumptions.

Out of scope:
- API reference for every function.

Acceptance criteria:
- Design doc reflects actual code modules.
- Includes diagram or clear text flow.
- Documents why PCAP processing is deterministic test baseline.

Metadata:
- Status: Backlog
- Priority: P0
- Size: M
- Component: Docs
- Milestone: M4 Delivery
- Labels: `type:docs`, `priority:p0`

Dependencies:
- Stable module boundaries from MVP implementation.

Risks:
- Writing docs before implementation stabilizes can cause drift.

Validation:
- Review doc against source tree and CLI behavior.

### 16. Write build, deploy, and demo guide

GitHub issue title: `Write build, deploy, and demo guide`

User outcome: Người dùng có thể tự build, chạy, và demo hệ thống từ repo sạch.

In scope:
- Native build instructions for CMake.
- Docker build/run instructions.
- Demo steps for sample PCAP and live capture.
- Troubleshooting for libpcap/Npcap permissions.

Out of scope:
- Full operations runbook.

Acceptance criteria:
- Commands are copy-pasteable from a clean checkout.
- Guide distinguishes Linux/macOS/Windows where needed.
- Demo verifies expected ARP sample output.

Metadata:
- Status: Backlog
- Priority: P0
- Size: M
- Component: Docs
- Milestone: M4 Delivery
- Labels: `type:docs`, `type:delivery`, `priority:p0`

Dependencies:
- Docker image.
- Offline PCAP MVP.
- Live capture for live demo section.

Risks:
- Platform-specific setup instructions can become stale.

Validation:
- Run the documented build and sample demo commands end to end.

## Suggested Sprint Order

Sprint 1:
- Implement PCAP file reader.
- Add Ethernet frame decoder.
- Parse ARP packets into asset observations.
- Maintain asset state with first seen and last seen.
- Render asset table output.
- Add deterministic CLI integration tests for PCAP mode.

Sprint 2:
- Implement JSON output.
- Add IPv4 and UDP minimal decoders.
- Parse DHCP metadata.
- Define asset model and output contract documentation.

Sprint 3:
- Implement bounded live capture.
- Add capture filter support.

Sprint 4:
- Create Docker image for CLI execution.
- Add Docker Compose live capture demo configuration.
- Write system design documentation.
- Write build, deploy, and demo guide.

## GitHub CLI Notes

The current environment has `gh` installed, but the configured GitHub token is invalid. After re-authentication, create the GitHub Project and issues from this backlog.

Useful commands:

```bash
gh auth login -h github.com
gh project create --owner nhani05 --title "Passive Network Asset Discovery"
gh issue create --title "Implement PCAP file reader" --label "type:feature,component:capture,priority:p0" --body-file /tmp/issue-body.md
```

GitHub Projects field values such as `Status`, `Priority`, `Size`, `Component`, and `Milestone` can then be assigned from the Project table view.
