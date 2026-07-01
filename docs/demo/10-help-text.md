# Phần 10: Demo CLI Help Menu

Tài liệu này hướng dẫn chạy kiểm tra nội dung hướng dẫn sử dụng (Help Text) tích hợp sẵn trong ứng dụng dòng lệnh.

---

## 1. Các Cách Hiển Thị Trợ Giúp

Bạn có thể yêu cầu hiển thị help menu bằng một trong hai cờ: `--help` hoặc `-h`.

### 1.1. Chạy trên Host native:
```bash
./build/asset-discovery --help
# hoặc
./build/asset-discovery -h
```

### 1.2. Chạy thông qua Docker container:
```bash
docker run --rm passive-asset-discovery --help
```

---

## 2. Kết Quả Kỳ Vọng (Expected Output)

Nội dung trợ giúp hiển thị cấu trúc lệnh tối giản mới sau khi lược bỏ các cờ cấu hình live/stop cũ:

```text
Usage:
  asset-discovery --pcap <file> [--filter <bpf>] [--output table|json|csv]
  asset-discovery --interface <name> [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv]

Options:
  --pcap <file>              Read packets from a PCAP file.
  --interface <name>         Capture packets from a live interface.
  --filter <bpf>             Filter packets with a BPF expression, for example: arp or udp port 67 or udp port 68.
  --capture-backend <name>   Live capture backend: auto, pcap, or af-packet. Defaults to auto.
  --output table|json|csv    Output format. Defaults to table.
  -h, --help                 Show this help text.
```
