add_test(NAME asset-discovery-help COMMAND asset-discovery --help)
add_test(NAME asset-discovery-missing-input COMMAND asset-discovery)
set_tests_properties(asset-discovery-missing-input PROPERTIES WILL_FAIL TRUE)

add_executable(table-renderer-tests
    TableRendererTests.cpp
)
target_link_libraries(table-renderer-tests PRIVATE asset_discovery_output)
add_test(NAME table-renderer-tests COMMAND table-renderer-tests)

add_executable(json-renderer-tests
    JsonRendererTests.cpp
)
target_link_libraries(json-renderer-tests PRIVATE asset_discovery_output)
add_test(NAME json-renderer-tests COMMAND json-renderer-tests)

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
    set(EMPTY_PCAP_PATH "${CMAKE_CURRENT_BINARY_DIR}/empty.pcap")
    add_custom_command(
        OUTPUT "${EMPTY_PCAP_PATH}"
        COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT_PATH=${EMPTY_PCAP_PATH}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateEmptyPcap.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/GenerateEmptyPcap.cmake"
        VERBATIM
    )
    add_custom_target(empty-pcap-fixture DEPENDS "${EMPTY_PCAP_PATH}")
    add_dependencies(asset-discovery empty-pcap-fixture)

    add_test(
        NAME asset-discovery-pcap-sample-mac
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/arp.pcap"
            "-DEXPECTED_OUTPUT=02:42:ac:11:00:02"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectOutputContains.cmake"
    )

    add_test(
        NAME asset-discovery-pcap-sample-ip
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/arp.pcap"
            "-DEXPECTED_OUTPUT=192.168.1.10"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectOutputContains.cmake"
    )

    add_test(
        NAME asset-discovery-pcap-empty-table
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${EMPTY_PCAP_PATH}"
            "-DEXPECTED_OUTPUT=No assets discovered."
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectOutputContains.cmake"
    )

    add_test(
        NAME asset-discovery-pcap-missing-file
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/missing.pcap"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectFailureContains.cmake"
    )

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_FOUND)
        add_test(
            NAME asset-discovery-pcap-json
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ValidateJsonOutput.py"
                "$<TARGET_FILE:asset-discovery>"
                "${PROJECT_SOURCE_DIR}/samples/arp.pcap"
        )
    endif()
endif()
