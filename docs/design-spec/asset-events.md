# Asset Event Logging

This document defines the arpwatch-like event layer used by the passive asset discovery pipeline.

## Flow

```text
ObservationBatch
  -> AssetMonitor
       -> AssetEventDetector
       -> AssetStore
       -> EventDispatcher
            -> stdout
            -> NDJSON file
            -> syslog
            -> PostgreSQL asset_events
```

Live capture uses a bounded event queue and event writer thread between `AssetMonitor` and `EventDispatcher`. PCAP mode uses the same detector and dispatches events synchronously while replaying observations from the capture file.

## Event Types

| Type | Severity | Meaning |
| --- | --- | --- |
| `new_asset` | `info` | A MAC address is observed for the first time. |
| `mac_changed_for_ip` | `warning` | An IP address is now associated with a different MAC address. |
| `ip_changed_for_mac` | `info` | A MAC address is now associated with a different IP address. |
| `ip_mac_flip_flop` | `high` | An IP returns to a previous MAC within the flip-flop window. |
| `asset_reappeared` | `info` | A known IP/MAC pair appears again after the inactivity threshold. |
| `hostname_learned` | `info` | A hostname is learned for a MAC address. |
| `hostname_changed` | `warning` | A MAC address reports a different hostname. |
| `ethernet_arp_mac_mismatch` | `high` | Ethernet source MAC differs from the ARP sender MAC. |
| `non_local_source_ip` | `warning` | An observed IP is outside configured local networks and not ignored. |

## NDJSON Format

Each event is written as one JSON object per line:

```json
{"ts":"1.0","event_type":"new_asset","severity":"info","ip":"192.168.1.12","mac":"aa:bb:cc:dd:ee:ff","protocol":"arp","interface":"eth0","message":"New asset discovered","metadata":{}}
{"ts":"2.0","event_type":"mac_changed_for_ip","severity":"warning","ip":"192.168.1.12","mac":"11:22:33:44:55:66","old_mac":"aa:bb:cc:dd:ee:ff","new_mac":"11:22:33:44:55:66","protocol":"arp","interface":"eth0","message":"IP address is now associated with a different MAC address","metadata":{}}
```

Timestamps use the existing packet timestamp format, `seconds.microseconds`, to stay compatible with current asset output.

## PostgreSQL

`assets` stores the final snapshot. `asset_events` stores event history:

```sql
CREATE TABLE IF NOT EXISTS asset_events (
    id BIGSERIAL PRIMARY KEY,
    event_time TEXT NOT NULL,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    ip_address TEXT,
    mac_address TEXT,
    old_ip TEXT,
    new_ip TEXT,
    old_mac TEXT,
    new_mac TEXT,
    hostname TEXT,
    protocol TEXT,
    interface TEXT,
    message TEXT,
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

## CLI and Environment Configuration

Realtime event logging is enabled by default. Sinks (stdout, NDJSON file, syslog, and database) are configured by convention and environment settings rather than CLI flags:

- **stdout**: Human-readable event lines are printed to stdout by default.
- **NDJSON**: Events are appended as JSON to `logs/events.ndjson` by default. You can override the output file path by setting the `ASSET_DISCOVERY_EVENTS_JSON` environment variable.
- **syslog**: Automatically enabled on supported Unix platforms.
- **PostgreSQL**: Writes to `asset_events` are automatically enabled when database configuration is active in the environment or `.env` file (e.g., via `DATABASE_URL` or `PG*`/`DB_*` variables).

### Examples

```sh
# Process a PCAP file with default event outputs and print final summary
./build/asset-discovery --pcap samples/arp.pcap

# Capture live from eth0 indefinitely, storing NDJSON events to a custom file
export ASSET_DISCOVERY_EVENTS_JSON=custom_logs/events.ndjson
sudo ./build/asset-discovery --interface eth0
```

Use `configs/default.yaml`, `--config <file>`, or `--profile <name>` for normal event/network policy such as `network.local_nets`, `network.ignore_nets`, `events.rate_limit_sec`, `events.flip_flop_window_sec`, and `events.reappearance_threshold_sec`. The matching CLI flags remain available as advanced overrides for quick tests.
