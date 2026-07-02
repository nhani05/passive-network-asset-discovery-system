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
  asset-discovery --pcap <file> [--config <file>|--profile <name>] [--filter <bpf>] [--output table|json|csv]
  asset-discovery --interface <name> [--config <file>|--profile <name>] [--filter <bpf>] [--capture-backend auto|pcap|af-packet]
  asset-discovery --version

Common options:
  --pcap <file>              Read packets from a PCAP file.
  --interface <name>         Capture packets from a live interface until SIGINT or SIGTERM.
  --config <file>            Load policy/runtime defaults from a YAML config file.
  --profile <name>           Load configs/<name>.yaml. Cannot be combined with --config.
  --filter <bpf>             Filter packets with a BPF expression, for example: arp or udp port 67 or udp port 68.
  --capture-backend <name>   Live capture backend: auto, pcap, or af-packet. Defaults to auto.
  --output table|json|csv    Output format. Defaults to json.
  --version                  Show version information.

Advanced overrides:
  --event-rate-limit <sec>   Suppress duplicate low-value events within this window.
  --event-queue-capacity <n> Live event queue capacity. Defaults to 1024.
  --flip-flop-window <sec>   Window for detecting IP/MAC flip-flop events.
  --reappearance-threshold <sec> Threshold for asset_reappeared events.
  --local-net <cidr>         Local IPv4 network for non-local source detection. Repeatable.
  --ignore-net <cidr>        Ignored IPv4 network for non-local source detection. Repeatable.

Default outputs:
  Realtime events are written to stdout, syslog when supported, and NDJSON at
  ASSET_DISCOVERY_EVENTS_JSON or logs/events.ndjson. PostgreSQL writes use
  .env, DATABASE_URL, PG*, or DB_* environment variables when present.

  -h, --help                 Show this help text.
```

## 3. Kiểm Tra Version

```bash
./build/asset-discovery --version
```

* **Kỳ vọng:**
  ```text
  asset-discovery 0.1.0
  ```
