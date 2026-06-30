#!/usr/bin/env python3
import argparse
import struct
import sys
from pathlib import Path


DEFAULT_PACKET_COUNT = 5_000_000
DEFAULT_ASSET_COUNT = 4_096
DEFAULT_START_TIME = 1_699_606_800
DEFAULT_PROGRESS_EVERY = 500_000
DEFAULT_FLUSH_PACKETS = 8_192

GLOBAL_HEADER = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65_535, 1)
PACKET_HEADER = struct.Struct("<IIII")
FRAME_LENGTH = 42
BASE_IPV4 = int.from_bytes(bytes([10, 0, 0, 2]), "big")
MAX_ASSET_COUNT = int.from_bytes(bytes([10, 255, 255, 254]), "big") - BASE_IPV4 + 1


def positive_int(value):
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be an integer") from error

    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def non_negative_int(value):
    try:
        parsed = int(value, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be an integer") from error

    if parsed < 0:
        raise argparse.ArgumentTypeError("must be zero or greater")
    return parsed


def human_size(byte_count):
    size = float(byte_count)
    for unit in ["B", "KiB", "MiB", "GiB"]:
        if size < 1024.0:
            return f"{size:.1f} {unit}"
        size /= 1024.0
    return f"{size:.1f} TiB"


def build_arp_template():
    frame = bytearray(FRAME_LENGTH)
    frame[0:6] = b"\xff" * 6
    frame[12:14] = b"\x08\x06"
    frame[14:22] = struct.pack("!HHBBH", 1, 0x0800, 6, 4, 1)
    frame[32:38] = b"\x00" * 6
    frame[38:42] = bytes([10, 0, 0, 1])
    return frame


def synthetic_host(asset_id):
    mac_address = b"\x02\x42" + asset_id.to_bytes(4, "big")
    ip_address = (BASE_IPV4 + asset_id).to_bytes(4, "big")
    return mac_address, ip_address


def write_pcap(output_path, count, asset_count, start_time, flush_packets, progress_every):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    frame = build_arp_template()
    pending = bytearray()

    with output_path.open("wb") as output:
        output.write(GLOBAL_HEADER)

        for index in range(count):
            asset_id = index % asset_count
            source_mac, source_ip = synthetic_host(asset_id)
            frame[6:12] = source_mac
            frame[22:28] = source_mac
            frame[28:32] = source_ip

            timestamp_seconds = start_time + (index // 1_000_000)
            timestamp_microseconds = index % 1_000_000
            pending.extend(
                PACKET_HEADER.pack(timestamp_seconds, timestamp_microseconds, FRAME_LENGTH, FRAME_LENGTH)
            )
            pending.extend(frame)

            packet_number = index + 1
            if packet_number % flush_packets == 0:
                output.write(pending)
                pending.clear()

            if progress_every > 0 and packet_number % progress_every == 0:
                print(f"wrote {packet_number:,}/{count:,} packets", file=sys.stderr)

        if pending:
            output.write(pending)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Generate a deterministic Ethernet PCAP with many ARP packets."
    )
    parser.add_argument("output", type=Path, help="output PCAP path")
    parser.add_argument(
        "--count",
        type=positive_int,
        default=DEFAULT_PACKET_COUNT,
        help="number of packets to write",
    )
    parser.add_argument(
        "--asset-count",
        type=positive_int,
        default=DEFAULT_ASSET_COUNT,
        help="number of synthetic MAC/IP pairs to cycle through",
    )
    parser.add_argument(
        "--start-time",
        type=non_negative_int,
        default=DEFAULT_START_TIME,
        help="first packet timestamp in Unix seconds",
    )
    parser.add_argument(
        "--flush-packets",
        type=positive_int,
        default=DEFAULT_FLUSH_PACKETS,
        help="number of packet records buffered before writing to disk",
    )
    parser.add_argument(
        "--progress-every",
        type=non_negative_int,
        default=DEFAULT_PROGRESS_EVERY,
        help="print progress after this many packets; use 0 to disable",
    )
    args = parser.parse_args(argv)

    if args.asset_count > MAX_ASSET_COUNT:
        parser.error(f"--asset-count cannot exceed {MAX_ASSET_COUNT:,}")

    return args


def main(argv=None):
    args = parse_args(argv)
    estimated_size = len(GLOBAL_HEADER) + args.count * (PACKET_HEADER.size + FRAME_LENGTH)
    active_assets = min(args.asset_count, args.count)
    print(
        f"writing {args.count:,} ARP packets to {args.output} ({human_size(estimated_size)})",
        file=sys.stderr,
    )
    print(f"cycling {active_assets:,} synthetic assets", file=sys.stderr)

    write_pcap(
        args.output,
        args.count,
        args.asset_count,
        args.start_time,
        args.flush_packets,
        args.progress_every,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
