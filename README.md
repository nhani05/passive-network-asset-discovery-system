# Passive Network Asset Discovery Desktop

PNAD is a Linux desktop application for passive network asset discovery. The product workflow is the GUI: users start live capture or PCAP analysis from Capture Center, investigate discovered assets and events, view network grouping, export reports, save preferences, and review runtime readiness in System Health.

The reusable C++ engine still provides packet capture, parsing, asset monitoring, event detection, SQLite persistence, and export behavior behind the desktop application.

## Desktop Workflows

- **Capture Center**: start Live Capture from a ready network interface or PCAP Analysis from a readable PCAP/PCAPNG file.
- **Dashboard**: review recent sessions, asset totals, event totals, recent events, and capture health.
- **Assets**: search and inspect discovered devices by MAC, IP, hostname, vendor, role, and discovery source.
- **Events**: filter events by severity, type, MAC, IP, protocol, interface, and message, then pivot to related assets.
- **Network Map**: group assets by stable network identity and open asset details.
- **Reports / Export**: export assets, events, and session summaries.
- **Preferences**: save capture defaults, local database location, detection rules, export format, and advanced engine settings.
- **System Health**: inspect capture permission, backend availability, selected interface readiness, local database readiness, rendering status, runtime failures, and log location.

## Build Requirements

Native desktop build:

- CMake 3.16 or newer.
- C++17 compiler.
- SQLite3 development package.
- Qt Quick/QML runtime and development packages.
- Optional libpcap development package for capture backend support.

Ubuntu/Debian desktop dependencies:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libsqlite3-dev libpcap-dev qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev qml-module-qtquick-controls2 qml-module-qtquick-dialogs qml-module-qtquick-layouts qml-module-qtquick-window2
```

## Build The Desktop App

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF -DASSET_DISCOVERY_BUILD_GUI=ON
cmake --build build --parallel
```

Run the desktop app:

```sh
./build/asset-discovery-gui
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

The GUI-enabled test set includes core engine tests, GUI model/controller tests, a package inspection test, and an offscreen QML smoke test that verifies first-run Capture Center routing and all top-level navigation destinations.

## Package

Build the GUI-only desktop package:

```sh
./scripts/package-release.sh
```
