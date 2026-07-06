#!/usr/bin/env bash
# PNAD Desktop launcher with rendering fallback.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_PATH="${DIR}/bin/asset-discovery-gui"
if [ ! -f "${BIN_PATH}" ]; then
    BIN_PATH="${DIR}/../build/asset-discovery-gui"
fi

if [ ! -f "${BIN_PATH}" ]; then
    echo "Error: PNAD desktop binary was not found."
    exit 1
fi

# Detect display server
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "Warning: No graphical display server detected. Running in offscreen mode."
    export QT_QPA_PLATFORM=offscreen
fi

# Probe OpenGL by running offscreen for 0.5 seconds
echo "Checking desktop rendering environment..."
if ! QT_QPA_PLATFORM=offscreen timeout 0.5s "${BIN_PATH}" >/dev/null 2>&1; then
    echo "Rendering check failed. Falling back to Qt Quick software backend..."
    export QT_QUICK_BACKEND=software
else
    echo "Rendering environment verified."
fi

exec "${BIN_PATH}"
