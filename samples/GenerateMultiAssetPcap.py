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


def arp_packet(sender_mac, sender_ip, target_mac, target_ip, operation=1):
    payload = bytearray()
    payload += u16(1)
    payload += u16(0x0800)
    payload += bytes([6, 4])
    payload += u16(operation)
    payload += mac(sender_mac)
    payload += ipv4(sender_ip)
    payload += mac(target_mac)
    payload += ipv4(target_ip)
    return ethernet("ff:ff:ff:ff:ff:ff", sender_mac, 0x0806, payload)


def dhcp_packet(client_mac, hostname, requested_ip=None, yiaddr="0.0.0.0", server_to_client=False):
    dhcp = bytearray(236)
    dhcp[0:4] = bytes([2 if server_to_client else 1, 1, 6, 0])
    dhcp[16:20] = ipv4(yiaddr)
    dhcp[28:34] = mac(client_mac)
    dhcp += u32(0x63825363)
    dhcp += bytes([53, 1, 5 if server_to_client else 3])
    dhcp += bytes([12, len(hostname)]) + hostname.encode("ascii")
    if requested_ip is not None:
        dhcp += bytes([50, 4]) + ipv4(requested_ip)
    dhcp += bytes([255])

    udp_length = 8 + len(dhcp)
    ip_length = 20 + udp_length
    source_ip = "192.168.1.1" if server_to_client else "0.0.0.0"
    destination_ip = "255.255.255.255"
    source_port = 67 if server_to_client else 68
    destination_port = 68 if server_to_client else 67

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
    return ethernet("ff:ff:ff:ff:ff:ff", client_mac, 0x0800, ipv4_header + udp + dhcp)


def ignored_udp_packet(source_mac):
    payload = b"not-dhcp"
    udp_length = 8 + len(payload)
    ip_length = 20 + udp_length
    ipv4_header = bytearray()
    ipv4_header += bytes([0x45, 0x00])
    ipv4_header += u16(ip_length)
    ipv4_header += u16(0)
    ipv4_header += u16(0)
    ipv4_header += bytes([64, 17])
    ipv4_header += u16(0)
    ipv4_header += ipv4("192.168.1.50")
    ipv4_header += ipv4("192.168.1.1")
    udp = u16(12345) + u16(53) + u16(udp_length) + u16(0)
    return ethernet("02:42:ac:11:00:01", source_mac, 0x0800, ipv4_header + udp + payload)


def ignored_ipv6_packet(source_mac):
    return ethernet("33:33:00:00:00:01", source_mac, 0x86DD, bytes(40))


def packets():
    return [
        arp_packet("02:42:ac:11:00:02", "192.168.1.10", "00:00:00:00:00:00", "192.168.1.1"),
        arp_packet("02:42:ac:11:00:01", "192.168.1.1", "02:42:ac:11:00:02", "192.168.1.10", 2),
        dhcp_packet("02:42:ac:11:00:03", "laptop-user", requested_ip="192.168.1.20"),
        arp_packet("02:42:ac:11:00:03", "192.168.1.20", "00:00:00:00:00:00", "192.168.1.1"),
        dhcp_packet("02:42:ac:11:00:04", "camera-01", yiaddr="192.168.1.30", server_to_client=True),
        ignored_udp_packet("02:42:ac:11:00:05"),
        ignored_ipv6_packet("02:42:ac:11:00:06"),
        arp_packet("02:42:ac:11:00:02", "192.168.1.11", "00:00:00:00:00:00", "192.168.1.1"),
    ]


def main():
    if len(sys.argv) != 2:
        print("usage: GenerateMultiAssetPcap.py <output>", file=sys.stderr)
        return 2

    with open(sys.argv[1], "wb") as output:
        output.write(struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        for index, packet in enumerate(packets()):
            output.write(struct.pack("<IIII", 1699606800 + index, index * 1000, len(packet), len(packet)))
            output.write(packet)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
