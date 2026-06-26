---
name: passive-asset-discovery-implementation
description: Implement focused C++17/CMake changes for the Passive Network Asset Discovery System. Use for production code, CLI behavior, packet processing, asset state, output rendering, and build integration.
---

# Implementation

Inspect the relevant code before changing it. Preserve the C++17 baseline, `asset_discovery` namespace, and existing CLI contracts unless the accepted requirement changes them.

Implement one accepted behavior at a time. Validate every packet boundary before reading fields; reject malformed, truncated, and unsupported data safely. Keep platform-specific libpcap/Npcap code behind adapters and keep parsers and asset logic deterministic. Do not send warnings to JSON standard output.

Add focused tests with the change, build it, and report exact validation results. Avoid unrelated refactors and generated output.
