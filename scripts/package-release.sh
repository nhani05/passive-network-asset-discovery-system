#!/usr/bin/env bash
# PNAD Linux Release Packaging Script
set -euo pipefail

# Resolve project root
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${DIR}/.."
cd "${PROJECT_ROOT}"

echo "=== 1. Building release binaries ==="
rm -rf build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF
cmake --build build --parallel

echo "=== 2. Creating release folder structure ==="
RELEASE_DIR="${PROJECT_ROOT}/release/pnad-desktop"
rm -rf "${RELEASE_DIR}"
mkdir -p "${RELEASE_DIR}/bin"

# Copy binaries
cp build/asset-discovery-gui "${RELEASE_DIR}/bin/"

# Copy launcher script
cp scripts/pnad-gui-launcher.sh "${RELEASE_DIR}/pnad-gui.sh"
chmod +x "${RELEASE_DIR}/pnad-gui.sh"

# Copy app icon if available, otherwise write a note
if [ -f "qml/pnad.png" ]; then
    cp qml/pnad.png "${RELEASE_DIR}/pnad.png"
fi

echo "=== 3. Creating pnad.desktop launcher metadata ==="
cat << 'EOF' > "${RELEASE_DIR}/pnad.desktop"
[Desktop Entry]
Type=Application
Name=Passive Network Asset Discovery
Comment=Passive network monitor and asset discovery GUI tool
Exec=bash -c "cd \"\$(dirname \"%k\")\" && ./pnad-gui.sh"
Icon=pnad
Terminal=false
Categories=System;Security;Network;
EOF
chmod +x "${RELEASE_DIR}/pnad.desktop"

echo "=== 4. Creating README for runtime dependencies ==="
cat << 'EOF' > "${RELEASE_DIR}/README.md"
# Passive Network Asset Discovery (PNAD) Desktop

Passive network monitoring and device inventory desktop application.

## Prerequisites / Dependencies

Install the Linux desktop runtime dependencies before launching PNAD:

```sh
# Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y libpcap-dev libsqlite3-0 libqt5widgets5 libqt5qml5 libqt5quick5 libqt5quickcontrols2-5 qml-module-qtquick-controls2 qml-module-qtquick-dialogs qml-module-qtquick-layouts qml-module-qtquick-window2
```

## Launching PNAD

Use the installed desktop entry or run `pnad-gui.sh` from this folder. The launcher opens the desktop application directly and does not require product arguments.

The launcher detects rendering startup issues and falls back to the Qt Quick software backend when needed.

## Product Workflows

- Select Live or PCAP mode from the first screen.
- Discover assets from the fixed ARP/DHCP capture scope.
- Inspect IP, MAC, hostname, first seen, last seen, and protocols.
- Export the current asset inventory as CSV or JSON.
- Review the in-session new-asset log.

## Troubleshooting Capture Permissions

If live capture reports missing permission, grant packet capture capabilities to the GUI binary:

```sh
sudo setcap cap_net_raw,cap_net_admin=eip bin/asset-discovery-gui
```
EOF
if [ -f "docs/desktop-user-guide.md" ]; then
    cp docs/desktop-user-guide.md "${RELEASE_DIR}/USER_GUIDE.md"
fi

echo "=== 5. Inspecting desktop package contents ==="
if [ -e "${RELEASE_DIR}/bin/asset-discovery" ]; then
    echo "Desktop package must not ship the CLI product binary." >&2
    exit 1
fi
if grep -R -E -- '--pcap|--interface|--sqlite|--output|--capture-backend|CLI binary|command-line product' "${RELEASE_DIR}"/*.md "${RELEASE_DIR}"/*.desktop "${RELEASE_DIR}"/*.sh >/dev/null; then
    echo "Desktop package contains CLI product workflow text." >&2
    exit 1
fi

echo "=== 6. Compressing release archive ==="
cd release
tar -czf pnad-desktop-linux.tar.gz pnad-desktop

echo "=== Packaging completed successfully! ==="
echo "Release archive created at: release/pnad-desktop-linux.tar.gz"
