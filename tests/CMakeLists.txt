add_test(NAME asset-discovery-help COMMAND asset-discovery --help)
add_test(NAME asset-discovery-missing-input COMMAND asset-discovery)
set_tests_properties(asset-discovery-missing-input PROPERTIES WILL_FAIL TRUE)

if(PCAP_FOUND)
    add_test(
        NAME asset-discovery-pcap-sample
        COMMAND asset-discovery --pcap "${PROJECT_SOURCE_DIR}/samples/arp.pcap"
    )
    set_tests_properties(asset-discovery-pcap-sample PROPERTIES
        PASS_REGULAR_EXPRESSION "packets=1"
    )

    add_test(
        NAME asset-discovery-pcap-missing-file
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/missing.pcap"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectFailureContains.cmake"
    )
endif()
