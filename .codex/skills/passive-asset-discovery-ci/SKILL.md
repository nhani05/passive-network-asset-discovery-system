---
name: passive-asset-discovery-ci
description: Build reliable continuous-integration checks for the Passive Network Asset Discovery System. Use for CI configuration, build matrices, automated tests, quality gates, and failure diagnosis.
---

# Continuous integration

Automate a clean, reproducible configure-build-test sequence. Start with CMake configure, build, and CTest; preserve the supported no-libpcap scaffold path and test the required-libpcap configuration when the runner provides it.

Use deterministic PCAP fixtures and non-privileged tests. Pin or document runner prerequisites, make failures readable, and avoid hidden state. Add checks incrementally: build/test first, then formatting, static analysis, Docker build, and release checks when the corresponding artifacts exist.

Treat CI configuration as code: review its permissions, cache keys, platform assumptions, and failure output.
