# Repository Guidelines

## Project Structure & Module Organization

This is a C++17 CMake project for passive network asset discovery. Public headers live under `include/`, grouped by module: `cli/`, `capture/`, `parser/`, `asset/`, and `output/`. Implementations mirror that layout under `src/`, with `src/main.cpp` wiring CLI parsing, packet capture, asset storage, and rendering. Tests are in `tests/` and are registered through `tests/CMakeLists.txt`. Sample traffic belongs in `samples/`; user-facing documentation belongs in `docs/`.

## Build, Test, and Development Commands

- `cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF`: configure a local build, including environments without libpcap.
- `cmake --build build`: compile libraries, `asset-discovery`, and test binaries.
- `ctest --test-dir build --output-on-failure`: run tests and print failing output.
- `./build/asset-discovery --help`: inspect supported CLI options.
- `./build/asset-discovery --pcap samples/arp.pcap`: run the sample PCAP flow when libpcap/Npcap is available.

Use `ASSET_DISCOVERY_REQUIRE_PCAP=ON` only when the environment must fail fast without packet capture.

## Coding Style & Naming Conventions

Keep code in standard C++17 with CMake target-based dependencies. Use 4-space indentation, braces on their own line for functions, and braces on the same line for control blocks. Namespaces use `asset_discovery::<module>`. Types and classes use `PascalCase`; functions, variables, and CMake targets use names such as `parseArguments`, `AssetStore`, and `asset_discovery_parser`.

## Language Guidelines

Documentation must be written in Vietnamese. This applies to files such as `README.md`, content under `docs/`, sample/demo instructions, build/deploy guides, and sprint notes. Code comments, CLI help text, warnings, errors, status messages, tests, commit messages, and internal contributor guidance such as this `AGENTS.md` should be written in English. Technical system terms may stay untranslated in Vietnamese docs when that is clearer, for example `asset`, `capture`, `packet`, `parser`, `PCAP`, `CLI`, `backend`, and `renderer`.

## Testing Guidelines

Tests are lightweight C++ executables registered with CTest; Python is used only for JSON-output validation when available. Name files as `<Feature>Tests.cpp` and CTest entries in lowercase hyphen form such as `asset-store-tests`. Cover parser edge cases, renderer output, CLI failures, and PCAP sample behavior when changing those areas.

## Commit & Pull Request Guidelines

Recent history uses Conventional Commits, for example `feat(capture): render live interface assets` and `feat(output): add TableRenderer with unit tests`. Keep commits focused and include tests or documentation with behavior changes. Pull requests should describe the user-visible change, list verification commands run, link relevant issues, and include screenshots or terminal excerpts when output formatting changes.

## Security & Configuration Tips

Do not commit generated build directories, packet captures containing sensitive traffic, or machine-specific Npcap paths. Keep sample PCAPs minimal and sanitized.
