---
name: passive-asset-discovery-container-delivery
description: Package and execute the Passive Network Asset Discovery System in Docker. Use for Dockerfiles, Compose configuration, runtime permissions, image validation, and deployment instructions.
---

# Container delivery

Build a minimal reproducible image. Separate build and runtime stages where practical, pin base-image intent, run as a non-root user where capture permissions permit it, and avoid embedding secrets.

Define how PCAP files enter the container and document the explicit host-network/capability requirements for live capture. Do not assume privileged mode is required or safe; request only the capabilities demonstrated by the capture backend. Keep offline PCAP demo execution available without live-network access.

Validate image build and a representative command. Record image inputs, ports if any, volumes, capabilities, and expected output in deployment documentation.
