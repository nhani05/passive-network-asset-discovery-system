# Sample PCAP Files

## arp.pcap

`arp.pcap` is a minimal Ethernet PCAP containing one ARP request.

- Sender MAC: `02:42:ac:11:00:02`
- Sender IP: `192.168.1.10`
- Target IP: `192.168.1.1`
- Link type: Ethernet

Use it as the first deterministic input for PCAP reader and ARP parser work:

```powershell
.\build-npcap\Debug\asset-discovery.exe --pcap samples\arp.pcap --output table
```

## multi-asset.pcap

`multi-asset.pcap` là file Ethernet PCAP có nhiều gói tin để kiểm thử parser và
quá trình gom nhóm asset chính xác hơn:

- 8 packet, gồm ARP request/reply, DHCP client/server, IPv4 UDP không phải DHCP,
  và IPv6 bị bỏ qua.
- 4 asset hợp lệ được phát hiện.
- MAC `02:42:ac:11:00:02` có 2 địa chỉ IP ARP: `192.168.1.10` và
  `192.168.1.11`.
- MAC `02:42:ac:11:00:03` xuất hiện từ cả ARP và DHCP với hostname
  `laptop-user`.
- MAC `02:42:ac:11:00:04` xuất hiện từ DHCP với hostname `camera-01`.

Tạo lại fixture bằng lệnh:

```bash
python3 samples/GenerateMultiAssetPcap.py samples/multi-asset.pcap
```

Chạy thử:

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap --output json
```
