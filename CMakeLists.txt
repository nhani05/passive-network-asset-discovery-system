cmake_minimum_required(VERSION 3.16)

project(passive_asset_discovery VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASSET_DISCOVERY_REQUIRE_PCAP "Dừng cấu hình khi không có libpcap" OFF)

add_library(asset_discovery_parser
    src/parser/ArpPacket.cpp
    src/parser/AssetObservation.cpp
    src/parser/EthernetFrame.cpp
    src/parser/PacketParsers.cpp
)

target_include_directories(asset_discovery_parser PUBLIC include)

add_library(asset_discovery_asset
    src/asset/AssetStore.cpp
)

target_include_directories(asset_discovery_asset PUBLIC include)
target_link_libraries(asset_discovery_asset PUBLIC asset_discovery_parser)

add_library(asset_discovery_output
    src/output/JsonRenderer.cpp
    src/output/TableRenderer.cpp
)

target_include_directories(asset_discovery_output PUBLIC include)
target_link_libraries(asset_discovery_output PUBLIC asset_discovery_asset)

add_executable(asset-discovery
    src/main.cpp
    src/capture/PacketCapture.cpp
    src/cli/Arguments.cpp
)

target_include_directories(asset-discovery PRIVATE include)
target_link_libraries(asset-discovery PRIVATE asset_discovery_output asset_discovery_asset asset_discovery_parser)

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
        message(STATUS "Đã tìm thấy Npcap SDK: ${NPCAP_ROOT}")
    endif()
endif()

if(PCAP_FOUND)
    target_include_directories(asset-discovery PRIVATE ${PCAP_INCLUDE_DIRS})
    target_link_libraries(asset-discovery PRIVATE ${PCAP_LIBRARIES})
    target_compile_options(asset-discovery PRIVATE ${PCAP_CFLAGS_OTHER})
    target_compile_definitions(asset-discovery PRIVATE ASSET_DISCOVERY_HAS_PCAP=1)
else()
    if(ASSET_DISCOVERY_REQUIRE_PCAP)
        message(FATAL_ERROR "Không tìm thấy libpcap. Hãy cài file phát triển của libpcap hoặc đặt ASSET_DISCOVERY_REQUIRE_PCAP=OFF.")
    endif()
    target_compile_definitions(asset-discovery PRIVATE ASSET_DISCOVERY_HAS_PCAP=0)
    message(WARNING "Không tìm thấy libpcap. Đang build khung CLI không có backend capture packet.")
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
