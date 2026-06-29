import json
import subprocess
import sys


def main() -> int:
    result = subprocess.run(
        [sys.argv[1], "--pcap", sys.argv[2], "--output", "json"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(result.stdout + result.stderr, file=sys.stderr)
        return result.returncode
    assets = json.loads(result.stdout)
    if len(assets) != 1:
        print(f"expected one DHCP asset, got {assets!r}", file=sys.stderr)
        return 1
    asset = assets[0]
    expected = {
        "mac_address": "02:42:ac:11:00:03",
        "ip_addresses": ["192.168.1.20"],
        "hostname": "laptop-user",
        "discovery_sources": ["dhcp"],
    }
    for key, value in expected.items():
        if asset.get(key) != value:
            print(f"expected {key}={value!r}, got {asset.get(key)!r}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
