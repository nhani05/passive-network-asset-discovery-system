add_test(NAME asset-discovery-help COMMAND asset-discovery --help)
add_test(NAME asset-discovery-missing-input COMMAND asset-discovery)
set_tests_properties(asset-discovery-missing-input PROPERTIES WILL_FAIL TRUE)

add_executable(table-renderer-tests
    TableRendererTests.cpp
)
target_link_libraries(table-renderer-tests PRIVATE asset_discovery_output)
add_test(NAME table-renderer-tests COMMAND table-renderer-tests)

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
        NAME asset-discovery-pcap-sample
        COMMAND asset-discovery --pcap "${PROJECT_SOURCE_DIR}/samples/arp.pcap"
    )
    set_tests_properties(asset-discovery-pcap-sample PROPERTIES
        PASS_REGULAR_EXPRESSION "02:42:ac:11:00:02"
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
endif()
