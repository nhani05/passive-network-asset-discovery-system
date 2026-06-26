---
name: passive-asset-discovery-operations
description: Operate, diagnose, and maintain deployed Passive Network Asset Discovery System instances. Use for runtime troubleshooting, observability, incident analysis, configuration checks, and maintenance changes.
---

# Operations

Diagnose from evidence: command line, build/backend status, input source, permissions, packet counts, errors, and produced output. Separate environment failures from parser, state, and rendering defects.

For live capture, verify interface existence, required capabilities, network namespace, and libpcap/Npcap availability before changing code. For PCAP processing, preserve the original fixture and record the link type and observed failure.

Prefer safe, reversible mitigations. Capture enough structured diagnostics to reproduce failures without logging sensitive packet payloads or unnecessary asset data. Turn confirmed regressions into deterministic tests.
