#!/usr/bin/env bash
set -euo pipefail

# Resolve project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_ROOT}"

echo "=== 1. Building CLI-only variant ==="
rm -rf build-cli-smoke
cmake -S . -B build-cli-smoke -DASSET_DISCOVERY_REQUIRE_PCAP=OFF -DASSET_DISCOVERY_BUILD_GUI=OFF
cmake --build build-cli-smoke --parallel
ctest --test-dir build-cli-smoke --output-on-failure
echo "✓ CLI-only variant compiled and tested successfully"

echo "=== 2. Building GUI-enabled variant ==="
rm -rf build-gui-smoke
cmake -S . -B build-gui-smoke -DASSET_DISCOVERY_REQUIRE_PCAP=OFF -DASSET_DISCOVERY_BUILD_GUI=ON
cmake --build build-gui-smoke --parallel
ctest --test-dir build-gui-smoke --output-on-failure
echo "✓ GUI-enabled variant compiled and tested successfully"

# Clean up smoke build folders
rm -rf build-cli-smoke build-gui-smoke

echo "=== CI/Dev Smoke Build Completed Successfully ==="
