## ADDED Requirements

### Requirement: Passive traffic sources
The system SHALL analyze existing network traffic from a PCAP file and SHALL support live network interface capture as a runtime mode.

#### Scenario: Analyze PCAP file
- **WHEN** the user runs the system with a valid PCAP file path
- **THEN** the system analyzes packets from that file without sending traffic to the network

#### Scenario: Capture live interface
- **WHEN** the user runs the system with a network interface and capture duration
- **THEN** the system captures packets from that interface for the requested duration without actively probing the network

#### Scenario: Reject missing input source
- **WHEN** the user runs the system without a PCAP file or network interface
- **THEN** the system reports a clear usage error and does not start discovery

### Requirement: Passive-only discovery behavior
The system MUST remain passive and MUST NOT perform ping sweeps, port scans, ARP probing, DHCP probing, or other active discovery traffic.

#### Scenario: Process passive packets only
- **WHEN** the system analyzes PCAP or live traffic
- **THEN** it only derives assets from observed packets and does not generate discovery packets

### Requirement: ARP asset detection
The system SHALL parse ARP packets and create asset observations from valid sender IP/MAC fields.

#### Scenario: Detect ARP sender asset
- **WHEN** an ARP packet contains a valid sender MAC address and sender IP address
- **THEN** the system records an observation containing that MAC address, IP address, ARP source, and packet timestamp

#### Scenario: Ignore malformed ARP packet
- **WHEN** an ARP packet is too short or has invalid address fields
- **THEN** the system ignores the malformed packet without terminating analysis

### Requirement: DHCP metadata enrichment
The system SHALL parse DHCP packets and enrich assets with available client MAC, hostname, requested IP, assigned IP, and DHCP message type metadata.

#### Scenario: Extract DHCP hostname
- **WHEN** a DHCP packet contains client MAC information and hostname option data
- **THEN** the system records an observation containing the client MAC, hostname, DHCP source, and packet timestamp

#### Scenario: Extract DHCP IP metadata
- **WHEN** a DHCP packet contains requested IP or assigned IP data
- **THEN** the system associates the observed IP address with the DHCP client asset

#### Scenario: DHCP packet without hostname
- **WHEN** a DHCP packet does not contain hostname option data
- **THEN** the system still processes any available MAC and IP metadata without failing

### Requirement: Asset identity and merge logic
The system SHALL use normalized MAC address as the primary asset identity and SHALL merge multiple observations for the same MAC into one asset record.

#### Scenario: Create new asset
- **WHEN** the asset engine receives the first valid observation for a MAC address
- **THEN** it creates one asset with `first_seen` and `last_seen` set to the observation timestamp

#### Scenario: Update existing asset
- **WHEN** the asset engine receives another valid observation for an existing MAC address
- **THEN** it updates `last_seen`, preserves the original `first_seen`, and merges new IP, hostname, and source metadata

#### Scenario: Preserve IP history
- **WHEN** the same MAC address is observed with multiple IP addresses
- **THEN** the asset record contains all distinct observed IP addresses instead of creating duplicate assets

### Requirement: Asset inventory fields
The system SHALL report discovered assets with MAC address, observed IP addresses, `first_seen`, `last_seen`, discovery sources, and hostname when available.

#### Scenario: Report required asset fields
- **WHEN** discovery completes for a PCAP or live capture run
- **THEN** each reported asset includes MAC address, IP address data, `first_seen`, `last_seen`, and discovery source data

#### Scenario: Report optional hostname
- **WHEN** a hostname has been observed for an asset from DHCP traffic
- **THEN** the reported asset includes that hostname

### Requirement: CLI output formats
The system SHALL provide a human-readable table output and a machine-readable JSON output for discovered assets.

#### Scenario: Render table output
- **WHEN** the user selects table output
- **THEN** the system prints discovered assets in a readable table containing the required asset fields

#### Scenario: Render JSON output
- **WHEN** the user selects JSON output
- **THEN** the system prints discovered assets as valid JSON containing the required asset fields

### Requirement: Docker execution
The system SHALL be buildable and runnable through Docker for the PCAP demo path.

#### Scenario: Build Docker image
- **WHEN** the user builds the project Docker image
- **THEN** the build completes with the asset discovery CLI available inside the image

#### Scenario: Run PCAP demo in Docker
- **WHEN** the user runs the Docker image with a mounted sample PCAP file
- **THEN** the container analyzes the PCAP and prints the selected asset output format

### Requirement: Error handling
The system SHALL handle invalid input files, unsupported output formats, and malformed packets with clear errors or safe packet-level skipping.

#### Scenario: Missing PCAP file
- **WHEN** the user provides a PCAP path that does not exist
- **THEN** the system reports that the file cannot be opened and exits with a non-zero status

#### Scenario: Unsupported output format
- **WHEN** the user requests an unsupported output format
- **THEN** the system reports the valid output options and exits with a non-zero status

#### Scenario: Malformed packet during analysis
- **WHEN** the system encounters a malformed packet after analysis has started
- **THEN** the system skips that packet and continues processing subsequent packets
