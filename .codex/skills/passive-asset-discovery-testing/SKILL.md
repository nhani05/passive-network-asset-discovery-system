---
name: passive-asset-discovery-testing
description: Design and implement deterministic tests for the Passive Network Asset Discovery System. Use for unit, integration, CLI, PCAP fixture, regression, and acceptance testing.
---

# Testing

Test observable contracts, not private implementation details. Use deterministic PCAP fixtures for packet/parser behavior; start from `samples/arp.pcap` and add minimal fixtures only for new protocol cases.

Cover valid observations and failure cases: truncated frames, unsupported link/protocol types, invalid CLI combinations, unavailable capture backends, and conflicting asset observations. Keep unit tests independent of live interfaces and privileges. Exercise PCAP input through the same processing path as production.

Run `ctest --test-dir build --output-on-failure` after a successful build and state any coverage not exercised.
