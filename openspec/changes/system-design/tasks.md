## 1. Project Scaffold

- [x] 1.1 Create the implementation directory structure for capture, parser, asset, output, CLI, tests, samples, scripts, and Docker-related files.
- [x] 1.2 Add a C++ build configuration with a runnable `asset-discovery` CLI target.
- [x] 1.3 Select and integrate the packet capture/parsing dependency for PCAP and live interface input.
- [x] 1.4 Add a minimal CLI argument parser supporting `--pcap`, `--interface`, `--duration`, `--output`, and `--help`.
- [x] 1.5 Add clear validation for missing input source and unsupported output format.

## 2. Packet Input

- [ ] 2.1 Implement PCAP file reading from a CLI-provided path.
- [ ] 2.2 Normalize packet timestamp data so parser observations can use packet time for `first_seen` and `last_seen`.
- [ ] 2.3 Report a clear non-zero error when a PCAP file cannot be opened.
- [ ] 2.4 Add live interface capture with a duration limit.
- [ ] 2.5 Ensure capture modes do not generate active discovery traffic.

## 3. Protocol Parsing

- [ ] 3.1 Define the normalized `AssetObservation` data model with MAC, optional IP, optional hostname, source, and timestamp fields.
- [ ] 3.2 Implement Ethernet/ARP detection and parse valid ARP sender MAC/IP observations.
- [ ] 3.3 Safely skip malformed or truncated ARP packets.
- [ ] 3.4 Implement UDP/DHCP detection for ports 67 and 68.
- [ ] 3.5 Parse DHCP client MAC and message type metadata.
- [ ] 3.6 Parse DHCP hostname, requested IP, and assigned IP metadata when present.
- [ ] 3.7 Safely handle DHCP packets without hostname or optional IP metadata.

## 4. Asset Discovery Engine

- [ ] 4.1 Define the `Asset` model with MAC address, IP address collection, optional hostname, `first_seen`, `last_seen`, and discovery source collection.
- [ ] 4.2 Normalize MAC addresses before using them as asset keys.
- [ ] 4.3 Create a new asset when the first valid observation for a MAC address arrives.
- [ ] 4.4 Update existing assets by preserving `first_seen`, updating `last_seen`, and merging IP, hostname, and source metadata.
- [ ] 4.5 Preserve distinct IP history for a MAC instead of creating duplicate assets.

## 5. Output and Demo Evidence

- [ ] 5.1 Implement table output containing MAC, observed IP addresses, hostname when available, `first_seen`, `last_seen`, and discovery sources.
- [ ] 5.2 Implement JSON output containing the same required asset fields.
- [ ] 5.3 Add at least one repeatable sample PCAP or documented sample PCAP generation workflow containing ARP traffic.
- [ ] 5.4 Add a sample or documented workflow containing DHCP hostname or IP metadata for enrichment testing.
- [ ] 5.5 Add CLI examples for PCAP table output and PCAP JSON output.

## 6. Docker Packaging

- [ ] 6.1 Add a Dockerfile that builds the CLI and provides it as the container entrypoint or default command.
- [ ] 6.2 Verify Docker PCAP execution with a mounted sample directory.
- [ ] 6.3 Document live capture Docker permissions including host networking and required network capabilities.
- [ ] 6.4 Add docker-compose only if it simplifies the demo or live capture workflow.

## 7. Testing

- [ ] 7.1 Add parser tests for valid ARP packets.
- [ ] 7.2 Add parser tests for malformed or truncated ARP packets.
- [ ] 7.3 Add parser tests for DHCP hostname and IP metadata extraction.
- [ ] 7.4 Add asset engine tests for create, update, timestamp preservation, and IP history merge behavior.
- [ ] 7.5 Add an integration test or script that runs the CLI against a sample PCAP and checks for expected asset output.
- [ ] 7.6 Add a Docker run test or documented verification command for the PCAP demo path.

## 8. Documentation

- [ ] 8.1 Update the README with build, run, PCAP demo, output format, and Docker instructions.
- [ ] 8.2 Add or update a design document describing module boundaries, data flow, asset merge rules, and passive-only behavior.
- [ ] 8.3 Add a demo guide with exact commands and expected evidence for PCAP and optional live capture.
- [ ] 8.4 Document known limitations including MAC randomization, spoofing, multiple interfaces per device, incomplete DHCP data, and Docker live-capture environment differences.
- [ ] 8.5 Record final verification results for local build, PCAP run, Docker build, and Docker PCAP run.
