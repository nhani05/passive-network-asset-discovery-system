add_test(NAME asset-discovery-help COMMAND asset-discovery --help)
add_test(NAME asset-discovery-missing-input COMMAND asset-discovery)
set_tests_properties(asset-discovery-missing-input PROPERTIES WILL_FAIL TRUE)

add_executable(ethernet-frame-tests
    EthernetFrameTests.cpp
)
target_link_libraries(ethernet-frame-tests PRIVATE asset_discovery_parser)
add_test(NAME ethernet-frame-tests COMMAND ethernet-frame-tests)

add_executable(arp-packet-tests
    ArpPacketTests.cpp
)
target_link_libraries(arp-packet-tests PRIVATE asset_discovery_parser)
add_test(NAME arp-packet-tests COMMAND arp-packet-tests)

add_executable(asset-store-tests
    AssetStoreTests.cpp
)
target_link_libraries(asset-store-tests PRIVATE asset_discovery_asset)
add_test(NAME asset-store-tests COMMAND asset-store-tests)

if(PCAP_FOUND)
    add_test(
        NAME asset-discovery-pcap-sample
        COMMAND asset-discovery --pcap "${PROJECT_SOURCE_DIR}/samples/arp.pcap"
    )
    set_tests_properties(asset-discovery-pcap-sample PROPERTIES
        PASS_REGULAR_EXPRESSION "asset mac=02:42:ac:11:00:02 ips=192.168.1.10"
    )

    add_test(
        NAME asset-discovery-pcap-missing-file
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/missing.pcap"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectFailureContains.cmake"
    )
endif()
