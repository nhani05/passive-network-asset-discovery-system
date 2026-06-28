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
