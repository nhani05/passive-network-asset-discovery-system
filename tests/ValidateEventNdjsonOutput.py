#!/usr/bin/env python3

import json
import os
import subprocess
import sys
import tempfile


def stub_database_env(work_dir, events_path):
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
            "ASSET_DISCOVERY_EVENTS_JSON": events_path,
        }
    )
    return env


def parse_final_json(stdout):
    lines = stdout.splitlines()
    for index, line in enumerate(lines):
        if line.lstrip().startswith("["):
            return json.loads("\n".join(lines[index:]))
    raise ValueError(f"missing final JSON output in stdout: {stdout!r}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: ValidateEventNdjsonOutput.py <asset-discovery> <pcap>")

    executable = sys.argv[1]
    pcap_path = sys.argv[2]

    with tempfile.TemporaryDirectory() as work_dir:
        events_path = os.path.join(work_dir, "events.ndjson")
        completed = subprocess.run(
            [
                executable,
                "--pcap",
                pcap_path,
                "--output",
                "json",
            ],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=stub_database_env(work_dir, events_path),
            cwd=work_dir,
        )
        if completed.returncode != 0:
            raise AssertionError(completed.stdout + completed.stderr)

        if "new_asset" not in completed.stdout:
            raise AssertionError(f"expected default stdout event lines, got {completed.stdout!r}")

        assets = parse_final_json(completed.stdout)
        if len(assets) != 2:
            raise AssertionError(f"expected final JSON asset output, got {completed.stdout!r}")

        with open(events_path, encoding="utf-8") as input_file:
            events = [json.loads(line) for line in input_file if line.strip()]

    event_types = [event["event_type"] for event in events]
    if "new_asset" not in event_types:
        raise AssertionError(f"missing new_asset event: {event_types}")
    if "mac_changed_for_ip" not in event_types:
        raise AssertionError(f"missing mac_changed_for_ip event: {event_types}")

    change = next(event for event in events if event["event_type"] == "mac_changed_for_ip")
    if change.get("old_mac") != "aa:bb:cc:dd:ee:ff":
        raise AssertionError(f"old_mac was not recorded: {change}")
    if change.get("new_mac") != "11:22:33:44:55:66":
        raise AssertionError(f"new_mac was not recorded: {change}")


if __name__ == "__main__":
    main()
