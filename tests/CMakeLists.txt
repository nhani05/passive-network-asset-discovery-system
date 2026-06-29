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

add_executable(csv-renderer-tests
    CsvRendererTests.cpp
)
target_link_libraries(csv-renderer-tests PRIVATE asset_discovery_output)
add_test(NAME csv-renderer-tests COMMAND csv-renderer-tests)

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

add_executable(packet-parser-tests
    PacketParserTests.cpp
)
target_link_libraries(packet-parser-tests PRIVATE asset_discovery_parser)
add_test(NAME packet-parser-tests COMMAND packet-parser-tests)

add_executable(packet-context-tests
    PacketContextTests.cpp
)
target_link_libraries(packet-context-tests PRIVATE asset_discovery_parser)
add_test(NAME packet-context-tests COMMAND packet-context-tests)

add_executable(parser-engine-tests
    ParserEngineTests.cpp
)
target_link_libraries(parser-engine-tests PRIVATE asset_discovery_parser)
add_test(NAME parser-engine-tests COMMAND parser-engine-tests)

add_executable(dns-plugin-tests
    DnsPluginTests.cpp
)
target_link_libraries(dns-plugin-tests PRIVATE asset_discovery_parser)
add_test(NAME dns-plugin-tests COMMAND dns-plugin-tests)

add_executable(asset-store-tests
    AssetStoreTests.cpp
)
target_link_libraries(asset-store-tests PRIVATE asset_discovery_asset)
add_test(NAME asset-store-tests COMMAND asset-store-tests)

add_executable(postgres-writer-tests
    PostgresWriterTests.cpp
)
target_link_libraries(postgres-writer-tests PRIVATE asset_discovery_storage)
add_test(NAME postgres-writer-tests COMMAND postgres-writer-tests)

add_executable(arguments-tests
    ArgumentsTests.cpp
)
target_link_libraries(arguments-tests PRIVATE asset_discovery_cli)
add_test(NAME arguments-tests COMMAND arguments-tests)

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

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_FOUND)
        set(DHCP_PCAP_PATH "${CMAKE_CURRENT_BINARY_DIR}/dhcp.pcap")
        add_custom_command(
            OUTPUT "${DHCP_PCAP_PATH}"
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/GenerateDhcpPcap.py"
                "${DHCP_PCAP_PATH}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/GenerateDhcpPcap.py"
            VERBATIM
        )
        add_custom_target(dhcp-pcap-fixture DEPENDS "${DHCP_PCAP_PATH}")
        add_dependencies(asset-discovery dhcp-pcap-fixture)
    endif()

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
        NAME asset-discovery-pcap-filter-arp
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/arp.pcap"
            "-DFILTER=arp"
            "-DEXPECTED_OUTPUT=02:42:ac:11:00:02"
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

    if(Python3_FOUND)
        add_test(
            NAME asset-discovery-pcap-json
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ValidateJsonOutput.py"
                "$<TARGET_FILE:asset-discovery>"
                "${PROJECT_SOURCE_DIR}/samples/arp.pcap"
        )

        add_test(
            NAME asset-discovery-pcap-dhcp-json
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ValidateDhcpJsonOutput.py"
                "$<TARGET_FILE:asset-discovery>"
                "${DHCP_PCAP_PATH}"
        )

        add_test(
            NAME asset-discovery-pcap-multi-asset-json
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ValidateMultiAssetJsonOutput.py"
                "$<TARGET_FILE:asset-discovery>"
                "${PROJECT_SOURCE_DIR}/samples/multi-asset.pcap"
        )
    endif()

    add_test(
        NAME asset-discovery-postgres-write
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/arp.pcap"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/dotenv-postgres-write"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ValidatePostgresWrite.cmake"
    )
endif()
