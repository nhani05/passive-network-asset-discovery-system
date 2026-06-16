## Context

The project is a Passive Network Asset Discovery System. It must observe existing network traffic from either a PCAP file or a network interface, detect assets by IP/MAC, track `first_seen` and `last_seen`, extract basic ARP/DHCP metadata such as hostname when available, and be deployable with Docker.

The repository currently contains requirements and project planning documents, but no implementation. This design defines the first implementation shape so the project can grow from a stable CLI-based MVP instead of starting with a dashboard, distributed storage, or active scanning behavior.

Primary constraints:

- The system must remain passive. It must not ping, scan ports, send ARP probes, or generate DHCP traffic to discover assets.
- PCAP processing should be the first reliable demo path because it is deterministic and easy to test.
- Live capture should be supported as a runtime mode, but Docker permissions and host OS behavior make it higher risk.
- ARP support is required for baseline discovery.
- DHCP support is required for metadata enrichment where DHCP packets are present.
- Docker packaging is part of the deliverable, not an optional afterthought.

Target high-level flow:

```text
PCAP file / live interface
          |
          v
Packet Input Layer
          |
          v
Protocol Parser Layer
   |              |
   v              v
 ARP            DHCP
   \              /
    v            v
 Asset Observation
          |
          v
Asset Discovery Engine
          |
          v
Output / Storage
```

## Goals / Non-Goals

**Goals:**

- Provide a modular architecture for packet input, protocol parsing, asset merge logic, output, optional storage, and Docker deployment.
- Implement a CLI-first system that can analyze a sample PCAP and produce a deterministic asset inventory.
- Use MAC address as the primary asset identity and maintain one or more observed IP addresses per MAC.
- Use packet timestamps for `first_seen` and `last_seen`.
- Parse ARP packets to detect sender IP/MAC and, when valid, target IP/MAC observations.
- Parse DHCP packets to enrich assets with hostname and requested/assigned IP metadata when present.
- Support table output for human demo and JSON output for machine-readable evidence.
- Keep the architecture easy to document, test, and defend in a project review.

**Non-Goals:**

- Active network scanning.
- Web dashboard, authentication, user management, or multi-user workflows.
- Threat detection, anomaly scoring, or machine learning.
- Distributed storage, message queues, Elasticsearch, Kafka, or long-running analytics pipelines.
- Full protocol coverage beyond ARP and DHCP for the initial project scope.
- Perfect device identity in the presence of MAC randomization, spoofing, NAT, or multiple interfaces.

## Decisions

### 1. Use a CLI-first architecture

The first user interface should be a command-line tool with modes such as:

```text
asset-discovery --pcap samples/arp_dhcp_sample.pcap --output table
asset-discovery --pcap samples/arp_dhcp_sample.pcap --output json
asset-discovery --interface eth0 --duration 60 --output table
```

Rationale:

- CLI output is enough to demonstrate the required behavior.
- CLI tools are straightforward to run inside Docker.
- Automated tests can invoke the CLI with sample PCAPs.
- A dashboard would add significant scope without improving the core discovery engine.

Alternative considered: web dashboard. Rejected for the MVP because it increases frontend/API/storage work before the packet analysis core is proven.

### 2. Treat PCAP processing as the primary MVP path

The first complete flow should read a file PCAP and emit an asset inventory.

Rationale:

- PCAP input is deterministic and repeatable.
- A sample PCAP can be included with the project or documented for demo generation.
- The same parser and asset engine can later be reused by live capture.
- Docker live capture has OS- and permission-specific risk.

Alternative considered: live capture first. Rejected for the MVP because Docker interface capture may require `--net=host`, `NET_ADMIN`, and `NET_RAW`, and behaves differently across environments.

### 3. Separate parser output from asset merge behavior with `AssetObservation`

Protocol parsers should not directly mutate the asset table. They should return a normalized observation:

```text
AssetObservation
- mac_address
- ip_address optional
- hostname optional
- source: ARP | DHCP
- timestamp
```

The asset discovery engine consumes observations and updates asset state.

Rationale:

- ARP and DHCP can share the same merge logic.
- Parser tests can focus on packet decoding.
- Asset engine tests can focus on deduplication, timestamps, IP history, and metadata precedence.
- Future protocols can produce the same observation type.

Alternative considered: parser directly updates assets. Rejected because it couples packet parsing to inventory policy and makes tests harder to isolate.

### 4. Use MAC address as the primary asset key

The asset table should use normalized MAC address as the primary key. Each asset can have zero or more observed IP addresses.

Rationale:

- IP addresses can change through DHCP.
- A MAC address is usually more stable inside a LAN.
- ARP and DHCP both expose client MAC information.
- Multiple observed IPs for one MAC are useful history rather than duplicate assets.

Known limitation: MAC addresses can be randomized, spoofed, or represent only one interface of a multi-interface device. This should be documented as a limitation.

Alternative considered: use IP address as the primary key. Rejected because DHCP churn would create duplicate asset records for the same device.

### 5. Keep storage optional until the core flow works

The MVP should maintain assets in memory and render table/JSON output at the end of a PCAP run. SQLite can be added after the parser and engine behavior is stable.

Rationale:

- The minimum deliverable is asset discovery and display, not persistence.
- In-memory state reduces early implementation risk.
- SQLite remains a good extension because it is lightweight, file-based, and Docker-friendly.

Alternative considered: start with SQLite immediately. Deferred because storage schema and persistence logic are easier to validate after observation and merge behavior are clear.

### 6. Package with Docker after the CLI can run locally

Docker should build the CLI and provide documented run commands for PCAP and live modes.

Expected PCAP demo shape:

```text
docker build -t passive-asset-discovery .
docker run --rm -v ./samples:/samples passive-asset-discovery \
  --pcap /samples/arp_dhcp_sample.pcap --output table
```

Expected live capture shape:

```text
docker run --rm --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery --interface eth0 --duration 60 --output table
```

Rationale:

- Docker is an explicit project requirement.
- PCAP mode can be demonstrated consistently in a container.
- Live capture can be documented with permission caveats.

## Risks / Trade-offs

- Live capture fails inside Docker -> Mitigate by making PCAP demo the primary acceptance path and documenting required Linux capabilities for live capture.
- DHCP parser reads malformed options incorrectly -> Mitigate with bounds checks, option-length validation, and sample packets with/without hostname.
- Asset identity is imperfect when MAC randomization or spoofing occurs -> Mitigate by documenting this limitation and preserving IP/source history for review.
- Sample PCAP lacks useful ARP/DHCP traffic -> Mitigate by adding or documenting a known-good sample capture for repeatable demo.
- Too much scope before MVP -> Mitigate by sequencing ARP + in-memory CLI output before DHCP, SQLite, live capture, and optional exports.
- Console output becomes hard to parse -> Mitigate by supporting JSON output alongside table output.

## Migration Plan

No migration is required because there is no existing application implementation.

Implementation should proceed in increments:

1. Scaffold project, build system, CLI entrypoint, and Docker skeleton.
2. Implement PCAP input and packet timestamp handling.
3. Implement ARP parser and in-memory asset engine.
4. Add table/JSON output.
5. Add DHCP parser and metadata enrichment.
6. Add Docker demo flow.
7. Add SQLite, live capture, CSV export, and broader tests if time allows.

Rollback strategy during implementation is to keep each increment independently runnable from the CLI and avoid merging optional extensions until the PCAP path still passes.

## Open Questions

- Which packet library should be selected for implementation: raw `libpcap`, PcapPlusPlus, or another C++ packet parsing library?
- Should SQLite be part of the required implementation or documented as an extension if time is limited?
- Will the final demo environment be Linux, Windows with Docker Desktop, or another setup?
- Will the project include a checked-in sample PCAP, or only instructions for generating one?
