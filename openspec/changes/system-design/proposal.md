## Why

The project needs a concrete system design for a Passive Network Asset Discovery System so implementation can stay focused on the required deliverables: passive traffic analysis, asset discovery, ARP/DHCP metadata, Docker deployment, and a clear demo path.

The current repository contains project requirements and planning notes, but no formal OpenSpec contract that defines the product behavior, architecture boundaries, and implementation tasks.

## What Changes

- Define the MVP behavior for discovering network assets from passive packet input.
- Establish PCAP-file analysis as the stable first demo path, with live interface capture as an extension.
- Specify asset identity and merge behavior using MAC address as the primary key.
- Require ARP parsing for baseline asset discovery.
- Require DHCP parsing for hostname and DHCP metadata enrichment where available.
- Define CLI output expectations for table and JSON-style asset reporting.
- Define Docker packaging expectations for repeatable build and demo execution.
- Document design constraints, module boundaries, risks, and implementation sequencing.

## Capabilities

### New Capabilities

- `passive-asset-discovery`: Detect and report network assets from passive ARP/DHCP traffic read from PCAP files or live network interfaces.

### Modified Capabilities

None.

## Impact

- Adds OpenSpec planning artifacts under `openspec/changes/system-design/`.
- Establishes expected future source areas such as capture, protocol parser, asset engine, output, storage, CLI, and Docker packaging.
- Introduces likely implementation dependencies on a packet capture library, C++ build tooling, Docker, and optional SQLite storage.
- Does not introduce breaking changes because no application implementation exists yet.
