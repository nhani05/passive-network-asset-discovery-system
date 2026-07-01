import json
import os
import subprocess
import sys
import tempfile


def stub_database_env(work_dir: str) -> dict:
    bin_dir = os.path.join(work_dir, "bin")
    os.makedirs(bin_dir, exist_ok=True)
    psql_path = os.path.join(bin_dir, "psql")
    with open(psql_path, "w", encoding="utf-8") as output:
        output.write(
            "#!/bin/sh\n"
            "set -eu\n"
            "while [ \"$#\" -gt 0 ]; do\n"
            "  if [ \"$1\" = \"-f\" ]; then\n"
            "    shift\n"
            "    cat \"$1\" > /dev/null\n"
            "    exit 0\n"
            "  fi\n"
            "  shift\n"
            "done\n"
            "exit 0\n"
        )
    os.chmod(psql_path, 0o755)

    env = os.environ.copy()
    env.update(
        {
            "PATH": f"{bin_dir}:{env.get('PATH', '')}",
            "DATABASE_URL": "",
            "PGHOST": "localhost",
            "PGPORT": "5432",
            "PGDATABASE": "asset_discovery",
            "PGUSER": "postgres",
            "PGPASSWORD": "postgres",
            "PGSERVICE": "",
            "ASSET_DISCOVERY_EVENTS_JSON": os.path.join(work_dir, "events.ndjson"),
        }
    )
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
            env=stub_database_env(work_dir),
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
        "discovery_sources": ["dhcp"],
    }
    for key, value in expected.items():
        if asset.get(key) != value:
            print(f"expected {key}={value!r}, got {asset.get(key)!r}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
