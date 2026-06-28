import json
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: ValidateJsonOutput.py <asset-discovery-exe> <pcap-path>", file=sys.stderr)
        return 2

    command = [sys.argv[1], "--pcap", sys.argv[2], "--output", "json"]
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout + result.stderr, file=sys.stderr)
        return result.returncode

    assets = json.loads(result.stdout)
    if not isinstance(assets, list) or len(assets) != 1:
        print(f"expected one asset, got {assets!r}", file=sys.stderr)
        return 1

    asset = assets[0]
    expected = {
        "mac_address": "02:42:ac:11:00:02",
        "ip_addresses": ["192.168.1.10"],
        "discovery_sources": ["arp"],
    }
    for key, value in expected.items():
        if asset.get(key) != value:
            print(f"expected {key}={value!r}, got {asset.get(key)!r}", file=sys.stderr)
            return 1

    for key in ("first_seen", "last_seen"):
        if not isinstance(asset.get(key), str) or not asset[key]:
            print(f"expected non-empty string field {key}, got {asset.get(key)!r}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
