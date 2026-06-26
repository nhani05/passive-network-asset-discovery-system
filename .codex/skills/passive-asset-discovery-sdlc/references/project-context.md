# Project context

## Required outcome

Build a Docker-deployable passive monitor that reads a network interface or PCAP file, discovers LAN assets from IP/MAC traffic, records first/last observation times, and extracts basic ARP/DHCP metadata such as hostname. Deliver source, design documentation, build/deployment instructions, Docker artifacts, and a demonstration.

## Current implementation

- C++17 CMake executable: `asset-discovery`.
- `src/cli/Arguments.cpp` owns CLI parsing. The input modes are mutually exclusive: `--pcap <file>` or `--interface <name> --duration <seconds>`; table and JSON output are supported selections.
- `capture::PacketCaptureBackend` currently reports whether libpcap/Npcap was available at build time. Packet ingestion and protocol parsing are not implemented yet.
- CMake finds libpcap through pkg-config on Unix-like systems or Npcap with `-DNPCAP_ROOT=...` on Windows. Without it, the CLI scaffold builds; `-DASSET_DISCOVERY_REQUIRE_PCAP=ON` makes absence a configuration failure.
- CTest contains CLI smoke tests. `samples/arp.pcap` has one Ethernet ARP request from MAC `02:42:ac:11:00:02` / IP `192.168.1.10` to target IP `192.168.1.1`.

## Development commands

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/asset-discovery --pcap samples/arp.pcap
```

Adapt the executable path and generator for the host platform. Do not assume live capture is available without libpcap/Npcap and sufficient privileges.

## Suggested delivery sequence

1. Implement offline PCAP ingestion and Ethernet/ARP parsing against deterministic fixtures.
2. Add the asset model, identity/merge rules, timestamps, table output, and stable JSON output.
3. Add DHCP metadata parsing and robust bounds/error handling.
4. Implement live capture through the same packet-processing path.
5. Add Docker packaging, deployment documentation, and a repeatable demo.
