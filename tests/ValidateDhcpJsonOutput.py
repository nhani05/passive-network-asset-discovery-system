import json
import os
import subprocess
import sys
import tempfile


def sqlite_env(work_dir: str) -> dict:
    env = os.environ.copy()
    env["SQLITE_DATABASE_PATH"] = os.path.join(work_dir, "assets.db")
    return env


def parse_final_json(stdout: str):
    lines = stdout.splitlines()
    for index, line in enumerate(lines):
        if line.lstrip().startswith("["):
            return json.loads("\n".join(lines[index:]))
    raise ValueError(f"missing final JSON output in stdout: {stdout!r}")


def main() -> int:
    with tempfile.TemporaryDirectory() as work_dir:
        result = subprocess.run(
            [sys.argv[1], "--pcap", sys.argv[2], "--output", "json"],
            check=False,
            capture_output=True,
            text=True,
            env=sqlite_env(work_dir),
            cwd=work_dir,
        )
    if result.returncode != 0:
        print(result.stdout + result.stderr, file=sys.stderr)
        return result.returncode
    assets = parse_final_json(result.stdout)
    if len(assets) != 1:
        print(f"expected one DHCP asset, got {assets!r}", file=sys.stderr)
        return 1
    asset = assets[0]
    expected = {
        "mac_address": "02:42:ac:11:00:03",
        "ip_addresses": ["192.168.1.20"],
        "hostname": "laptop-user",
        "display_name": "laptop-user",
        "discovery_sources": ["dhcp"],
    }
    for key, value in expected.items():
        if asset.get(key) != value:
            print(f"expected {key}={value!r}, got {asset.get(key)!r}", file=sys.stderr)
            return 1
    for key in ("vendor", "os_hint", "device_type", "model_hint"):
        if key not in asset:
            print(f"expected summary field {key} to be present", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
