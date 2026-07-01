#!/usr/bin/env python3

import struct
import sys


def mac_bytes(value):
    return bytes(int(part, 16) for part in value.split(":"))


def ip_bytes(value):
    return bytes(int(part) for part in value.split("."))


def arp_frame(source_mac, source_ip):
    ethernet = (
        mac_bytes("ff:ff:ff:ff:ff:ff")
        + mac_bytes(source_mac)
        + struct.pack("!H", 0x0806)
    )
    arp = (
        struct.pack("!HHBBH", 1, 0x0800, 6, 4, 1)
        + mac_bytes(source_mac)
        + ip_bytes(source_ip)
        + mac_bytes("00:00:00:00:00:00")
        + ip_bytes("192.168.1.1")
    )
    return ethernet + arp


def packet_record(ts_sec, payload):
    return struct.pack("<IIII", ts_sec, 0, len(payload), len(payload)) + payload


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: GenerateMacChangePcap.py <output>")

    packets = [
        packet_record(1, arp_frame("aa:bb:cc:dd:ee:ff", "192.168.1.12")),
        packet_record(2, arp_frame("11:22:33:44:55:66", "192.168.1.12")),
    ]
    with open(sys.argv[1], "wb") as output:
        output.write(struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        for packet in packets:
            output.write(packet)


if __name__ == "__main__":
    main()
