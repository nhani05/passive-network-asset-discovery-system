import struct
import sys


def u16(value):
    return struct.pack("!H", value)


def u32(value):
    return struct.pack("!I", value)


def mac(value):
    return bytes.fromhex(value.replace(":", ""))


def ipv4(value):
    return bytes(int(part) for part in value.split("."))


def ethernet(destination, source, ether_type, payload):
    return mac(destination) + mac(source) + u16(ether_type) + payload


def ipv4_udp_packet(source_mac, source_ip, destination_ip, source_port, destination_port, payload, destination_mac="ff:ff:ff:ff:ff:ff"):
    udp_length = 8 + len(payload)
    ip_length = 20 + udp_length
    ipv4_header = bytearray()
    ipv4_header += bytes([0x45, 0x00])
    ipv4_header += u16(ip_length)
    ipv4_header += u16(0)
    ipv4_header += u16(0)
    ipv4_header += bytes([64, 17])
    ipv4_header += u16(0)
    ipv4_header += ipv4(source_ip)
    ipv4_header += ipv4(destination_ip)
    udp = u16(source_port) + u16(destination_port) + u16(udp_length) + u16(0)
    return ethernet(destination_mac, source_mac, 0x0800, ipv4_header + udp + payload)


def arp_packet(sender_mac, sender_ip):
    payload = bytearray()
    payload += u16(1)
    payload += u16(0x0800)
    payload += bytes([6, 4])
    payload += u16(1)
    payload += mac(sender_mac)
    payload += ipv4(sender_ip)
    payload += mac("00:00:00:00:00:00")
    payload += ipv4("192.168.1.1")
    return ethernet("ff:ff:ff:ff:ff:ff", sender_mac, 0x0806, payload)


def dhcp_packet(client_mac, hostname, requested_ip):
    dhcp = bytearray(236)
    dhcp[0:4] = bytes([1, 1, 6, 0])
    dhcp[28:34] = mac(client_mac)
    dhcp += u32(0x63825363)
    dhcp += bytes([53, 1, 3])
    dhcp += bytes([12, len(hostname)]) + hostname.encode("ascii")
    dhcp += bytes([60, len("MSFT 5.0")]) + b"MSFT 5.0"
    dhcp += bytes([50, 4]) + ipv4(requested_ip)
    dhcp += bytes([255])
    return ipv4_udp_packet(client_mac, "0.0.0.0", "255.255.255.255", 68, 67, dhcp)


def ssdp_packet(source_mac):
    payload = (
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "NT: urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
        "SERVER: Linux/5.10 UPnP/1.0 DemoTV/1.0\r\n"
        "USN: uuid:demo-tv::urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
        "MANUFACTURER: Samsung\r\n"
        "MODELNAME: Smart TV\r\n"
        "\r\n"
    ).encode("ascii")
    return ipv4_udp_packet(source_mac, "192.168.1.80", "239.255.255.250", 1900, 1900, payload)


def dns_name(labels):
    data = bytearray()
    for label in labels:
        data.append(len(label))
        data += label.encode("ascii")
    data.append(0)
    return data


def mdns_packet(source_mac):
    payload = bytearray()
    payload += bytes([0x00, 0x00, 0x84, 0x00])
    payload += u16(0)
    payload += u16(2)
    payload += u16(0)
    payload += u16(0)

    payload += dns_name(["_airplay", "_tcp", "local"])
    payload += u16(12)
    payload += u16(1)
    payload += u32(120)
    ptr_rdata = dns_name(["Nam-iPhone", "_airplay", "_tcp", "local"])
    payload += u16(len(ptr_rdata))
    payload += ptr_rdata

    payload += dns_name(["Nam-iPhone", "_airplay", "_tcp", "local"])
    payload += u16(16)
    payload += u16(1)
    payload += u32(120)
    txt = b"model=iPhone"
    payload += u16(len(txt) + 1)
    payload.append(len(txt))
    payload += txt

    return ipv4_udp_packet(source_mac, "192.168.1.90", "224.0.0.251", 5353, 5353, payload, "01:00:5e:00:00:fb")


def packets():
    return [
        arp_packet("b8:27:eb:11:22:33", "192.168.1.60"),
        dhcp_packet("02:42:ac:11:00:03", "windows-laptop", "192.168.1.20"),
        ssdp_packet("00:1a:11:22:33:44"),
        mdns_packet("3c:15:c2:aa:bb:cc"),
    ]


def main():
    if len(sys.argv) != 2:
        print("usage: GenerateEnrichmentPcap.py <output>", file=sys.stderr)
        return 2
    with open(sys.argv[1], "wb") as output:
        output.write(struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        for index, packet in enumerate(packets()):
            output.write(struct.pack("<IIII", 1699606900 + index, index * 1000, len(packet), len(packet)))
            output.write(packet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
