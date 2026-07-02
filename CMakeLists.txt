cmake_minimum_required(VERSION 3.16)

project(passive_asset_discovery VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASSET_DISCOVERY_REQUIRE_PCAP "Stop configuration when libpcap is unavailable" OFF)

add_library(asset_discovery_domain
    src/event/AssetEvent.cpp
    src/event/AssetEventDetector.cpp
    src/discovery/AssetObservation.cpp
    src/discovery/AssetStore.cpp
    src/event/EventRateLimiter.cpp
)

target_include_directories(asset_discovery_domain PUBLIC include)
add_library(asset_discovery_asset ALIAS asset_discovery_domain)

add_library(asset_discovery_packet
    src/packet/ArpPacket.cpp
    src/packet/EthernetFrame.cpp
)

target_include_directories(asset_discovery_packet PUBLIC include)
target_link_libraries(asset_discovery_packet PUBLIC asset_discovery_domain)

add_library(asset_discovery_parser_core
    src/packet/PacketContext.cpp
    src/packet/ParserEngine.cpp
    src/packet/ParserInterface.cpp
    src/packet/ParserRegistry.cpp
)

target_include_directories(asset_discovery_parser_core PUBLIC include)
target_link_libraries(asset_discovery_parser_core PUBLIC asset_discovery_domain asset_discovery_packet)

add_library(asset_discovery_parser_plugins
    src/packet/ArpPlugin.cpp
    src/packet/BuiltinParserPlugins.cpp
    src/packet/DhcpPlugin.cpp
    src/packet/DnsPlugin.cpp
    src/packet/NetbiosPlugin.cpp
    src/packet/SsdpPlugin.cpp
    src/packet/TcpPlugin.cpp
)

target_include_directories(asset_discovery_parser_plugins PUBLIC include)
target_link_libraries(asset_discovery_parser_plugins PUBLIC asset_discovery_parser_core asset_discovery_packet)

add_library(asset_discovery_parser
    src/packet/PacketParserFacade.cpp
)

target_include_directories(asset_discovery_parser PUBLIC include)
target_link_libraries(asset_discovery_parser PUBLIC asset_discovery_parser_core asset_discovery_parser_plugins)

add_library(asset_discovery_monitor
    src/discovery/AssetMonitor.cpp
)

target_include_directories(asset_discovery_monitor PUBLIC include)
target_link_libraries(asset_discovery_monitor PUBLIC asset_discovery_domain)

add_library(asset_discovery_live
    src/app/LiveCapturePipeline.cpp
)

target_include_directories(asset_discovery_live PUBLIC include)
target_link_libraries(asset_discovery_live PUBLIC
    asset_discovery_capture
    asset_discovery_domain
    asset_discovery_monitor
    asset_discovery_parser
)

add_library(asset_discovery_output
    src/discovery/CsvRenderer.cpp
    src/event/EventSink.cpp
    src/discovery/JsonRenderer.cpp
    src/discovery/TableRenderer.cpp
)

target_include_directories(asset_discovery_output PUBLIC include)
target_link_libraries(asset_discovery_output PUBLIC asset_discovery_domain)

add_library(asset_discovery_storage
    src/storage/PostgresWriter.cpp
)

target_include_directories(asset_discovery_storage PUBLIC include)
target_link_libraries(asset_discovery_storage PUBLIC asset_discovery_domain asset_discovery_output)

add_library(asset_discovery_capture
    src/capture/AfPacketCaptureBackend.cpp
    src/capture/PacketCapture.cpp
)

target_include_directories(asset_discovery_capture PUBLIC include)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(asset_discovery_capture PRIVATE ASSET_DISCOVERY_HAS_AF_PACKET=1)
else()
    target_compile_definitions(asset_discovery_capture PRIVATE ASSET_DISCOVERY_HAS_AF_PACKET=0)
endif()

add_library(asset_discovery_cli
    src/cli/Arguments.cpp
)

target_include_directories(asset_discovery_cli PUBLIC include)
target_link_libraries(asset_discovery_cli PUBLIC asset_discovery_domain)
target_compile_definitions(asset_discovery_cli PUBLIC ASSET_DISCOVERY_VERSION="${PROJECT_VERSION}")

add_library(asset_discovery_config
    src/config/AppConfig.cpp
)

target_include_directories(asset_discovery_config PUBLIC include)
target_link_libraries(asset_discovery_config PUBLIC
    asset_discovery_capture
    asset_discovery_cli
    asset_discovery_domain
)

add_executable(asset-discovery
    src/main.cpp
)

target_include_directories(asset-discovery PRIVATE include)
target_link_libraries(asset-discovery PRIVATE
    asset_discovery_capture
    asset_discovery_cli
    asset_discovery_config
    asset_discovery_domain
    asset_discovery_live
    asset_discovery_monitor
    asset_discovery_output
    asset_discovery_parser
    asset_discovery_storage
)

add_executable(asset-discovery-live-benchmark
    benchmarks/LivePipelineBenchmark.cpp
)

target_include_directories(asset-discovery-live-benchmark PRIVATE include)
target_link_libraries(asset-discovery-live-benchmark PRIVATE asset_discovery_live asset_discovery_capture)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PCAP QUIET libpcap)
endif()

if(NOT PCAP_FOUND AND DEFINED NPCAP_ROOT)
    set(_NPCAP_INCLUDE_DIR "${NPCAP_ROOT}/Include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_NPCAP_LIBRARY_DIR "${NPCAP_ROOT}/Lib/x64")
    else()
        set(_NPCAP_LIBRARY_DIR "${NPCAP_ROOT}/Lib")
    endif()

    set(_NPCAP_WPCAP_LIBRARY "${_NPCAP_LIBRARY_DIR}/wpcap.lib")
    set(_NPCAP_PACKET_LIBRARY "${_NPCAP_LIBRARY_DIR}/Packet.lib")

    if(EXISTS "${_NPCAP_INCLUDE_DIR}/pcap.h"
       AND EXISTS "${_NPCAP_WPCAP_LIBRARY}"
       AND EXISTS "${_NPCAP_PACKET_LIBRARY}")
        set(PCAP_FOUND TRUE)
        set(PCAP_INCLUDE_DIRS "${_NPCAP_INCLUDE_DIR}")
        set(PCAP_LIBRARIES "${_NPCAP_WPCAP_LIBRARY}" "${_NPCAP_PACKET_LIBRARY}")
        set(PCAP_CFLAGS_OTHER "")
        message(STATUS "Found Npcap SDK: ${NPCAP_ROOT}")
    endif()
endif()

if(PCAP_FOUND)
    target_include_directories(asset_discovery_capture PRIVATE ${PCAP_INCLUDE_DIRS})
    target_link_libraries(asset_discovery_capture PRIVATE ${PCAP_LIBRARIES})
    target_compile_options(asset_discovery_capture PRIVATE ${PCAP_CFLAGS_OTHER})
    target_compile_definitions(asset_discovery_capture PRIVATE ASSET_DISCOVERY_HAS_PCAP=1)
else()
    if(ASSET_DISCOVERY_REQUIRE_PCAP)
        message(FATAL_ERROR "libpcap was not found. Install the libpcap development package or set ASSET_DISCOVERY_REQUIRE_PCAP=OFF.")
    endif()
    target_compile_definitions(asset_discovery_capture PRIVATE ASSET_DISCOVERY_HAS_PCAP=0)
    message(WARNING "libpcap was not found. Building the CLI shell without a packet capture backend.")
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
