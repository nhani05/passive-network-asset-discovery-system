---
name: passive-asset-discovery-release
description: Prepare and verify reproducible releases of the Passive Network Asset Discovery System. Use for release candidates, versioning, release checklists, changelogs, artifact verification, and demo readiness.
---

# Release

Start from a clean, identified revision. Verify requirements, tests, build instructions, Docker artifacts, and demo commands against that revision.

Check both intended capture modes to the extent environment permits: PCAP must run against a deterministic fixture; live capture must either work with documented prerequisites or fail with a clear limitation. Confirm output contracts and release notes describe only shipped behavior.

Record version, commit hash, build environment, commands, produced artifacts, known limitations, and rollback path. Block release on an unverified required artifact.
