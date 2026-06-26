---
name: passive-asset-discovery-sdlc
description: Plan, implement, test, and document changes for the Passive Network Asset Discovery System. Use for requirements analysis, C++/CMake implementation, PCAP or live-interface capture, ARP/DHCP parsing, asset lifecycle tracking, CLI or output design, Docker packaging, test creation, and delivery-readiness reviews in this repository.
---

# Passive Asset Discovery SDLC

Use this skill to make a production-minded, incremental change to this repository. Read [references/project-context.md](references/project-context.md) before planning or implementing.

## Work flow

1. Inspect the relevant code and preserve unrelated worktree changes.
2. State the smallest vertical slice that satisfies the request and the observable acceptance criteria.
3. Keep platform-independent domain logic separate from packet capture and CLI concerns. Preserve the existing `asset_discovery` namespace and C++17 baseline unless a change explicitly requires otherwise.
4. Make malformed, truncated, unsupported, and unavailable-input conditions explicit. Do not invent packet fields or report a discovery result from an unvalidated parse.
5. Add or update tests with each behavior change. Prefer deterministic PCAP fixtures for parser and end-to-end coverage; use the supplied ARP fixture as the initial smoke input.
6. Build and run focused tests. Report commands run, results, and any environment limitation such as unavailable libpcap.
7. Update the design/build/deployment documentation when a public behavior, architecture, or deployment prerequisite changes.

## Git commits

- Make small commits that each represent one coherent, buildable change. Do not combine implementation, unrelated cleanup, generated files, or another person's work.
- Use Conventional Commits: `type(scope): imperative summary`. Use `feat`, `fix`, `test`, `docs`, `refactor`, `build`, `ci`, or `chore`; use the affected component as the scope when it improves clarity.
- Write a concise imperative summary that states the outcome, for example `feat(arp): record sender observations` or `test(cli): cover invalid durations`.
- Inspect the staged diff before every commit. Stage paths selectively, run the focused validation for that slice, and report the commit hash and checks performed.
- Keep prerequisite refactors and behavior changes in separate commits when either can be reviewed or reverted independently.

## Feature rules

- Model an asset around its stable identity (normally MAC) and associate observed IP addresses and metadata with timestamps. Define a deterministic merge/conflict policy before implementation.
- Treat `first_seen` as immutable on a recognized asset and advance `last_seen` only from valid observations.
- Parse link, network, and transport/application boundaries defensively before reading fields. Support only documented link types and protocols; count or surface unsupported traffic without crashing.
- Keep offline PCAP reading and live capture behind an adapter. Do not make core parsing or asset-state tests require elevated capture privileges.
- Preserve CLI invariants: exactly one of `--pcap` and `--interface`; `--interface` requires a positive `--duration`; `--output` accepts `table` or `json`.
- Keep output stable and machine-readable in JSON mode. Do not mix warnings into JSON standard output; use standard error or an explicit structured error policy.
- Do not claim Docker support until a reproducible Dockerfile or Compose configuration builds and executes the application.

## Completion gate

Before calling a change complete, verify the relevant items:

- The requested behavior works for valid input and fails safely for expected invalid input.
- Focused tests pass, including CLI validation where options changed.
- The project configures with and without libpcap according to `ASSET_DISCOVERY_REQUIRE_PCAP`.
- Documentation, Docker artifacts, and sample commands match the implemented behavior.

When a request spans requirements or architecture rather than code, produce an implementation-ready slice: components, data flow, input/error contracts, test plan, and delivery artifacts.
