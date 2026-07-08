# Sample PCAP Files

Thư mục này chứa PCAP/PCAPNG fixture dùng cho demo thủ công và test tự động của PNAD. CLI cần SQLite path khi chạy fixture:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --sqlite /tmp/pnad-sample.db \
  --output json
```

Có thể thay `--sqlite /tmp/pnad-sample.db` bằng environment:

```sh
SQLITE_DATABASE_PATH=/tmp/pnad-sample.db \
  ./build/asset-discovery --pcap samples/multi-asset.pcap --output table
```

CLI không xóa database đích. Khi chạy lại trên cùng SQLite, asset trùng MAC được upsert/merge và dữ liệu khác trong database được giữ lại.

## Fixture Chính

| File | Nội dung | Cách chạy khuyến nghị |
| --- | --- | --- |
| `arp.pcap` | Ethernet PCAP tối thiểu có một ARP request. Kỳ vọng phát hiện MAC `02:42:ac:11:00:02` và IP `192.168.1.10`. | `--output table` hoặc `--output json` |
| `multi-asset.pcap` | Fixture deterministic 8 packet: ARP request/reply, DHCP client/server, IPv4 UDP không phải DHCP và IPv6 bị bỏ qua. Kỳ vọng 4 asset hợp lệ. | Fixture chính cho smoke/demo CLI. |
| `test.pcap` | PCAPNG demo tổng hợp có ARP, DHCP, mDNS và SSDP với nhiều loại asset như laptop, iPhone, printer, camera và TV. | Dùng tốt cho demo GUI/CLI. |
| `test2.pcap` | PCAP Ethernet lớn hơn, chứa nhiều traffic thực tế cho kiểm tra merge asset và enrichment. | Dùng khi cần inventory nhiều dòng hơn. |

## Fixture Theo Protocol

| File | Nội dung | Ghi chú |
| --- | --- | --- |
| `arp-test/arp-storm.pcap` | ARP traffic lặp, một MAC xuất hiện với nhiều IP theo thời gian. | Hữu ích để kiểm tra `last_seen` và merge IP. |
| `dhcp-test/dhcp.pcap` | DHCP traffic có client MAC `00:0b:82:01:fc:42`. | Kỳ vọng hostname có thể trống nhưng IP/DHCP source được ghi nhận. |
| `dns-mdns-test/dns-mdns.pcap` | DNS/mDNS-oriented capture có ARP/DHCP/mDNS enrichment. | Filter mặc định đã gồm mDNS port 5353, không gồm DNS port 53. |
| `ssdp-test/ssdp` | SSDP fixture phát hiện router/media metadata qua UDP 1900. | File không có extension; CLI đọc được nếu libpcap đọc được, GUI file picker yêu cầu đổi/copy thành `.pcap`. |
| `nestbios-test/smb-legacy-implementation.pcapng` | Legacy NetBIOS/SMB reference fixture. | Không phải fixture demo CLI chính; dùng làm dữ liệu tham chiếu khi phát triển parser. |
| `tcp-test/chargen-tcp.pcap` | TCP SYN-ACK và IPv4 endpoint enrichment. | Chạy với `--broad-ipv4-enrichment` hoặc filter chứa `ip`. |
| `tcp-test/tfp_capture.pcapng` | PCAPNG mixed-interface edge fixture. | CLI hiện có thể từ chối vì interface types khác nhau trong cùng file. |

## Lệnh Mẫu

ARP tối thiểu:

```sh
./build/asset-discovery \
  --pcap samples/arp.pcap \
  --sqlite /tmp/pnad-arp.db \
  --output table
```

Fixture nhiều asset:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --sqlite /tmp/pnad-multi.db \
  --output json
```

TCP/IPv4 enrichment:

```sh
./build/asset-discovery \
  --pcap samples/tcp-test/chargen-tcp.pcap \
  --sqlite /tmp/pnad-tcp.db \
  --broad-ipv4-enrichment \
  --output json
```

SSDP fixture không có extension:

```sh
./build/asset-discovery \
  --pcap samples/ssdp-test/ssdp \
  --sqlite /tmp/pnad-ssdp.db \
  --output json
```

## Filter

Filter mặc định của PNAD:

```text
arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353
```

Filter này đủ cho ARP, DHCP, SSDP và mDNS demo. Với DNS port 53, LLMNR, NetBIOS name service hoặc TCP/IPv4 endpoint enrichment, truyền `--filter` phù hợp hoặc dùng `--broad-ipv4-enrichment`.
