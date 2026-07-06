# Phần 10: Demo CLI Help Menu

Tài liệu này hướng dẫn kiểm tra help text tích hợp sẵn trong CLI chính `asset-discovery`.

## 1. Hiển Thị Trợ Giúp

```bash
./build/asset-discovery --help
./build/asset-discovery -h
```

Trong Docker:

```bash
docker run --rm passive-asset-discovery --help
```

## 2. Kết Quả Kỳ Vọng

```text
Usage:
  asset-discovery --pcap <file> [--filter <bpf>] [--sqlite <file>] [--output table|json|csv]
  asset-discovery --version

Common options:
  --pcap <file>              Read packets from a PCAP file.
  --filter <bpf>             Filter packets with a BPF expression, for example: arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353.
  --broad-ipv4-enrichment    Use a broader passive IPv4 filter for TTL OS hints unless --filter is set.
  --sqlite <file>            Save assets in a local SQLite database file.
  --output table|json|csv    Output format. Defaults to json.
  --version                  Show version information.

Default outputs:
  New asset events are written to stdout. SQLite persists asset inventory only.

  -h, --help                 Show this help text.
```

## 3. Kiểm Tra Version

```bash
./build/asset-discovery --version
```

Kỳ vọng:

```text
asset-discovery 0.1.0
```
