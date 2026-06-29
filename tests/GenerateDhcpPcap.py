import struct
import sys


def u16(value):
    return struct.pack("!H", value)


def u32(value):
    return struct.pack("!I", value)


def build_packet():
    mac = bytes.fromhex("0242ac110003")
    hostname = b"laptop-user"
    dhcp = bytearray(236)
    dhcp[0:4] = bytes([1, 1, 6, 0])
    dhcp[28:34] = mac
    dhcp += u32(0x63825363)
    dhcp += bytes([53, 1, 1])
    dhcp += bytes([12, len(hostname)]) + hostname
    dhcp += bytes([50, 4, 192, 168, 1, 20])
    dhcp += bytes([255])

    udp_length = 8 + len(dhcp)
    ip_length = 20 + udp_length
    ethernet = b"\xff" * 6 + mac + b"\x08\x00"
    ipv4 = bytearray()
    ipv4 += bytes([0x45, 0x00])
    ipv4 += u16(ip_length)
    ipv4 += u16(0)
    ipv4 += u16(0)
    ipv4 += bytes([64, 17])
    ipv4 += u16(0)
    ipv4 += bytes([0, 0, 0, 0, 255, 255, 255, 255])
    udp = u16(68) + u16(67) + u16(udp_length) + u16(0)
    return ethernet + ipv4 + udp + dhcp


def main():
    if len(sys.argv) != 2:
        print("usage: GenerateDhcpPcap.py <output>", file=sys.stderr)
        return 2
    packet = build_packet()
    with open(sys.argv[1], "wb") as output:
        output.write(struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        output.write(struct.pack("<IIII", 1699606800, 0, len(packet), len(packet)))
        output.write(packet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
