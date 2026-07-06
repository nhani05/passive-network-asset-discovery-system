import json
import os
import subprocess
import sys
import tempfile


def by_mac(assets):
    return {asset.get("mac_address"): asset for asset in assets}


def parse_final_json(stdout: str):
    lines = stdout.splitlines()
    for index, line in enumerate(lines):
        if line.lstrip().startswith("["):
            return json.loads("\n".join(lines[index:]))
    raise ValueError(f"missing final JSON output in stdout: {stdout!r}")


def expect_fields(asset, expected):
    for key, value in expected.items():
        if asset.get(key) != value:
            print(f"expected {key}={value!r}, got {asset.get(key)!r} in {asset!r}", file=sys.stderr)
            return False
    return True


def main() -> int:
    with tempfile.TemporaryDirectory() as work_dir:
        env = os.environ.copy()
        env["SQLITE_DATABASE_PATH"] = os.path.join(work_dir, "assets.db")
        result = subprocess.run(
            [sys.argv[1], "--pcap", sys.argv[2], "--output", "json"],
            check=False,
            capture_output=True,
            text=True,
            env=env,
            cwd=work_dir,
        )
    if result.returncode != 0:
        print(result.stdout + result.stderr, file=sys.stderr)
        return result.returncode

    assets = by_mac(parse_final_json(result.stdout))
    expected = {
        "b8:27:eb:11:22:33": {
            "ip_addresses": ["192.168.1.60"],
            "vendor": "Raspberry Pi Foundation",
            "discovery_sources": ["arp"],
        },
        "02:42:ac:11:00:03": {
            "ip_addresses": ["192.168.1.20"],
            "hostname": "windows-laptop",
            "display_name": "windows-laptop",
            "os_hint": "windows",
            "discovery_sources": ["dhcp"],
        },
        "00:1a:11:22:33:44": {
            "ip_addresses": ["192.168.1.80"],
            "vendor": "Samsung",
            "os_hint": "linux",
            "device_type": "media-renderer",
            "model_hint": "Smart TV",
            "discovery_sources": ["ssdp"],
        },
        "3c:15:c2:aa:bb:cc": {
            "ip_addresses": ["192.168.1.90"],
            "display_name": "Nam-iPhone",
            "vendor": "Apple",
            "device_type": "apple-media",
            "model_hint": "iPhone",
            "discovery_sources": ["mdns"],
        },
    }
    for mac, fields in expected.items():
        asset = assets.get(mac)
        if asset is None:
            print(f"missing asset {mac}: {assets!r}", file=sys.stderr)
            return 1
        if not expect_fields(asset, {"mac_address": mac, **fields}):
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
