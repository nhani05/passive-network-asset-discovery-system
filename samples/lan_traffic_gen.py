#!/usr/bin/env python3
import argparse
import random
import socket
import threading
import time
from typing import List, Tuple

from scapy.all import (
    ARP,
    BOOTP,
    DHCP,
    DNS,
    DNSQR,
    Ether,
    IP,
    UDP,
    TCP,
    RandShort,
    conf,
    get_if_addr,
    get_if_hwaddr,
    send,
    sendp,
)


STOP = threading.Event()


def mac_to_bytes(mac: str) -> bytes:
    return bytes(int(x, 16) for x in mac.split(":"))


def jitter_sleep(base: float, jitter: float = 0.25):
    delay = base * random.uniform(1 - jitter, 1 + jitter)
    time.sleep(max(delay, 0.05))


def get_default_gateway() -> str:
    try:
        route = conf.route.route("0.0.0.0")
        return route[2]
    except Exception:
        return "192.168.1.1"


def arp_loop(iface: str, target_ip: str, interval: float):
    while not STOP.is_set():
        pkt = Ether(dst="ff:ff:ff:ff:ff:ff") / ARP(
            op="who-has",
            pdst=target_ip,
        )
        sendp(pkt, iface=iface, verbose=False)
        print(f"[ARP] who-has {target_ip}")
        jitter_sleep(interval)


def dhcp_loop(iface: str, interval: float):
    """
    Gửi DHCP Discover bằng MAC thật của máy, không random MAC.
    Tần suất nên thấp để tránh làm phiền DHCP server.
    """
    mac = get_if_hwaddr(iface)
    chaddr = mac_to_bytes(mac)

    while not STOP.is_set():
        xid = random.randint(1, 0xFFFFFFFF)

        pkt = (
            Ether(src=mac, dst="ff:ff:ff:ff:ff:ff")
            / IP(src="0.0.0.0", dst="255.255.255.255")
            / UDP(sport=68, dport=67)
            / BOOTP(chaddr=chaddr, xid=xid, flags=0x8000)
            / DHCP(
                options=[
                    ("message-type", "discover"),
                    ("param_req_list", [1, 3, 6, 15, 51, 58, 59]),
                    "end",
                ]
            )
        )

        sendp(pkt, iface=iface, verbose=False)
        print("[DHCP] discover")
        jitter_sleep(interval)


def mdns_loop(iface: str, interval: float):
    while not STOP.is_set():
        qnames = [
            "_services._dns-sd._udp.local",
            "_http._tcp.local",
            "_ipp._tcp.local",
            "_airplay._tcp.local",
            "_googlecast._tcp.local",
        ]

        qname = random.choice(qnames)

        pkt = (
            IP(dst="224.0.0.251", ttl=255)
            / UDP(sport=random.randint(49152, 65535), dport=5353)
            / DNS(
                rd=0,
                qd=DNSQR(qname=qname, qtype="PTR"),
            )
        )

        send(pkt, iface=iface, verbose=False)
        print(f"[mDNS] query {qname}")
        jitter_sleep(interval)


def ssdp_loop(iface: str, interval: float):
    while not STOP.is_set():
        st_values = [
            "ssdp:all",
            "upnp:rootdevice",
            "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
            "urn:schemas-upnp-org:service:WANIPConnection:1",
        ]

        st = random.choice(st_values)

        payload = (
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            'MAN: "ssdp:discover"\r\n'
            "MX: 2\r\n"
            f"ST: {st}\r\n"
            "\r\n"
        ).encode()

        pkt = (
            IP(dst="239.255.255.250", ttl=2)
            / UDP(sport=random.randint(49152, 65535), dport=1900)
            / payload
        )

        send(pkt, iface=iface, verbose=False)
        print(f"[SSDP] M-SEARCH {st}")
        jitter_sleep(interval)


def tcp_loop(targets: List[Tuple[str, int]], interval: float):
    while not STOP.is_set():
        host, port = random.choice(targets)

        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(1.5)
            s.connect((host, port))

            payloads = [
                b"HEAD / HTTP/1.1\r\nHost: test.local\r\nConnection: close\r\n\r\n",
                b"GET / HTTP/1.1\r\nHost: test.local\r\nConnection: close\r\n\r\n",
                b"hello\r\n",
                b"ping\r\n",
            ]

            payload = random.choice(payloads)
            s.sendall(payload)
            print(f"[TCP] connect/send {host}:{port}")

            try:
                s.recv(128)
            except Exception:
                pass

            s.close()

        except Exception as e:
            print(f"[TCP] failed {host}:{port} - {e}")

        jitter_sleep(interval)


def tcp_syn_loop(iface: str, targets: List[Tuple[str, int]], interval: float):
    """
    Tạo SYN packet nhẹ để có thêm traffic TCP ở Layer 3/4.
    Không dùng tốc độ cao.
    """
    while not STOP.is_set():
        host, port = random.choice(targets)

        pkt = IP(dst=host) / TCP(
            sport=RandShort(),
            dport=port,
            flags="S",
            seq=random.randint(1, 0xFFFFFFFF),
        )

        send(pkt, iface=iface, verbose=False)
        print(f"[TCP-SYN] {host}:{port}")
        jitter_sleep(interval)


def parse_targets(values: List[str]) -> List[Tuple[str, int]]:
    targets = []

    for value in values:
        if ":" not in value:
            raise ValueError(f"Target sai định dạng: {value}. Dùng IP:PORT")

        host, port = value.rsplit(":", 1)
        targets.append((host, int(port)))

    return targets


def main():
    parser = argparse.ArgumentParser(
        description="Controlled LAN traffic generator: ARP, DHCP, TCP, mDNS, SSDP"
    )

    parser.add_argument("--iface", required=True, help="Interface, ví dụ: eth0, en0, wlan0")
    parser.add_argument("--arp-target", default=None, help="IP để ARP who-has, mặc định là default gateway")

    parser.add_argument("--tcp-target", action="append", default=[], help="Đích TCP dạng IP:PORT. Có thể khai báo nhiều lần")
    parser.add_argument("--enable-dhcp", action="store_true", help="Bật DHCP Discover tần suất thấp")
    parser.add_argument("--enable-tcp-syn", action="store_true", help="Bật TCP SYN packet nhẹ")

    parser.add_argument("--arp-interval", type=float, default=5.0)
    parser.add_argument("--dhcp-interval", type=float, default=60.0)
    parser.add_argument("--mdns-interval", type=float, default=4.0)
    parser.add_argument("--ssdp-interval", type=float, default=6.0)
    parser.add_argument("--tcp-interval", type=float, default=3.0)
    parser.add_argument("--tcp-syn-interval", type=float, default=5.0)

    args = parser.parse_args()

    iface = args.iface
    my_ip = get_if_addr(iface)
    my_mac = get_if_hwaddr(iface)
    arp_target = args.arp_target or get_default_gateway()

    print(f"Interface : {iface}")
    print(f"Local IP  : {my_ip}")
    print(f"Local MAC : {my_mac}")
    print(f"ARP target: {arp_target}")
    print("Dừng bằng Ctrl+C")

    threads = []

    threads.append(threading.Thread(target=arp_loop, args=(iface, arp_target, args.arp_interval), daemon=True))
    threads.append(threading.Thread(target=mdns_loop, args=(iface, args.mdns_interval), daemon=True))
    threads.append(threading.Thread(target=ssdp_loop, args=(iface, args.ssdp_interval), daemon=True))

    if args.enable_dhcp:
        threads.append(threading.Thread(target=dhcp_loop, args=(iface, args.dhcp_interval), daemon=True))

    if args.tcp_target:
        targets = parse_targets(args.tcp_target)
        threads.append(threading.Thread(target=tcp_loop, args=(targets, args.tcp_interval), daemon=True))

        if args.enable_tcp_syn:
            threads.append(threading.Thread(target=tcp_syn_loop, args=(iface, targets, args.tcp_syn_interval), daemon=True))

    for t in threads:
        t.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping...")
        STOP.set()
        time.sleep(1)


if __name__ == "__main__":
    main()