import json
import os
import sqlite3
import subprocess
import sys
import tempfile


def run_cli(executable: str, pcap_path: str, db_path: str) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    env["SQLITE_DATABASE_PATH"] = db_path
    return subprocess.run(
        [executable, "--pcap", pcap_path, "--output", "json"],
        check=False,
        capture_output=True,
        text=True,
        env=env,
    )


def seed_sentinel_data(db_path: str) -> None:
    with sqlite3.connect(db_path) as db:
        db.execute(
            """
            INSERT OR REPLACE INTO assets (
                mac_address,
                ip_addresses,
                display_name,
                first_seen,
                last_seen,
                discovery_sources
            ) VALUES (?, ?, ?, ?, ?, ?);
            """,
            (
                "aa:bb:cc:dd:ee:ff",
                '["10.10.10.10"]',
                "Sentinel Asset",
                "1.0",
                "1.0",
                '["manual"]',
            ),
        )
        db.execute(
            "INSERT OR REPLACE INTO app_settings (key, value) VALUES (?, ?);",
            ("sentinel.setting", "keep-me"),
        )
        db.execute(
            """
            INSERT INTO analysis_sessions (
                mode,
                source,
                start_time,
                status,
                storage_context
            ) VALUES (?, ?, ?, ?, ?);
            """,
            ("Manual", "seed", "1.0", "Completed", db_path),
        )


def query_one(db_path: str, sql: str, args=()):
    with sqlite3.connect(db_path) as db:
        row = db.execute(sql, args).fetchone()
    return None if row is None else row[0]


def parse_final_json(stdout: str):
    lines = stdout.splitlines()
    for index, line in enumerate(lines):
        if line.lstrip().startswith("["):
            return json.loads("\n".join(lines[index:]))
    raise ValueError(f"missing final JSON output in stdout: {stdout!r}")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: ValidateCliPreservesDatabase.py <asset-discovery-exe> <pcap-path>", file=sys.stderr)
        return 2

    executable = sys.argv[1]
    pcap_path = sys.argv[2]

    with tempfile.TemporaryDirectory() as work_dir:
        db_path = os.path.join(work_dir, "assets.db")

        initial = run_cli(executable, pcap_path, db_path)
        if initial.returncode != 0:
            print(initial.stdout + initial.stderr, file=sys.stderr)
            return initial.returncode

        seed_sentinel_data(db_path)

        result = run_cli(executable, pcap_path, db_path)
        if result.returncode != 0:
            print(result.stdout + result.stderr, file=sys.stderr)
            return result.returncode

        assets = parse_final_json(result.stdout)
        if not assets:
            print("expected CLI to return discovered assets", file=sys.stderr)
            return 1

        sentinel_name = query_one(
            db_path,
            "SELECT display_name FROM assets WHERE mac_address = ?;",
            ("aa:bb:cc:dd:ee:ff",),
        )
        if sentinel_name != "Sentinel Asset":
            print("expected existing asset row to be preserved", file=sys.stderr)
            return 1

        setting = query_one(
            db_path,
            "SELECT value FROM app_settings WHERE key = ?;",
            ("sentinel.setting",),
        )
        if setting != "keep-me":
            print("expected existing app_settings row to be preserved", file=sys.stderr)
            return 1

        session_count = query_one(
            db_path,
            "SELECT COUNT(*) FROM analysis_sessions WHERE mode = 'Manual' AND source = 'seed';",
        )
        if session_count != 1:
            print("expected existing analysis session row to be preserved", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
