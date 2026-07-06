# Phần 11: Checklist Bằng Chứng Demo

Tài liệu này tổng hợp các lệnh/log cần chuẩn bị làm bằng chứng bàn giao.

| # | Evidence | Lệnh |
| :--- | :--- | :--- |
| **1** | CTest chạy thành công | `ctest --test-dir build --output-on-failure` |
| **2** | CLI help text | `./build/asset-discovery --help` |
| **3** | PCAP ARP dạng table | `./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --output table` |
| **4** | PCAP ARP dạng JSON | `./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --output json` |
| **5** | PCAP ARP dạng CSV | `./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --output csv` |
| **6** | PCAP đa asset dạng table | `./build/asset-discovery --pcap samples/multi-asset.pcap --sqlite pnad.db --output table` |
| **7** | PCAP đa asset dạng JSON | `./build/asset-discovery --pcap samples/multi-asset.pcap --sqlite pnad.db --output json` |
| **8** | PCAP đa asset với BPF filter | `./build/asset-discovery --pcap samples/multi-asset.pcap --sqlite pnad.db --filter "arp or udp port 67 or udp port 68" --output table` |
| **9** | BPF sai cú pháp được báo lỗi | `./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --filter "invalid" --output table` |
| **10** | Output format không hỗ trợ được báo lỗi | `./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --output xml` |
| **11** | SQLite có bảng `assets`, không có `asset_events` | `sqlite3 pnad.db ".tables"` |
| **12** | Query asset inventory trong SQLite | `sqlite3 pnad.db "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"` |
| **13** | Docker container chạy PCAP + SQLite | `docker run --rm -v "$PWD/samples:/samples:ro" -v "$PWD/.docker-data:/data" -e SQLITE_DATABASE_PATH=/data/pnad.db passive-asset-discovery --pcap /samples/arp.pcap --output table` |
| **14** | Compose PCAP demo | `docker compose up --build pcap-demo` |
| **15** | Docker runtime verification | `scripts/verify-docker-runtime.sh` |
| **16** | Config YAML duy nhất | `cat configs/default.yaml` |
| **17** | CLI/config defaults được đối chiếu | `./build/asset-discovery --help`, `./build/asset-discovery --version`, `docs/demo/13-cli-parameters.md` |
