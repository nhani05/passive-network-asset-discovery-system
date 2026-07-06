#!/usr/bin/env bash
set -euo pipefail

# Resolve project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_ROOT}"

echo "=== Building PCAP-enabled variant ==="
rm -rf build
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
echo "✓ PCAP-enabled variant compiled and tested successfully"

echo "=== CI/Dev Smoke Build Completed Successfully ==="
