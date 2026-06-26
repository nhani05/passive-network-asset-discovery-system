---
name: passive-asset-discovery-architecture
description: Design components, boundaries, data flow, and technical decisions for the Passive Network Asset Discovery System. Use for system design, module design, data modeling, capture/parsing boundaries, and architectural tradeoffs.
---

# Architecture

Inspect the current C++17/CMake structure and root README. Design the smallest set of components that meets the accepted requirements.

Keep capture adapters, packet decoding, asset-state management, rendering, and CLI orchestration separate. Feed offline PCAP and live capture into the same validated packet-processing path. Model a stable asset identity, observations, metadata, and first/last-seen rules explicitly. Define ownership, error propagation, unsupported link/protocol behavior, and thread-safety before implementation.

Record alternatives only where the decision is consequential. Ensure the design can be tested without live capture privileges.
