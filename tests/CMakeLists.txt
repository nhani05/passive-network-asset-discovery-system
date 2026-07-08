add_test(NAME asset-discovery-help COMMAND asset-discovery --help)
add_test(NAME asset-discovery-missing-input COMMAND asset-discovery)
set_tests_properties(asset-discovery-missing-input PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME asset-core-boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DTARGET_NAME=asset-core"
        "-DLINK_LIBRARIES=$<TARGET_PROPERTY:asset-core,INTERFACE_LINK_LIBRARIES>"
        "-DFORBIDDEN_PATTERNS=Qt;Qml;Quick;WebSocket;Network;http;websocket"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckTargetBoundary.cmake"
)

add_test(NAME asset-capture-help COMMAND asset-capture --help)
add_test(NAME asset-capture-backend-status COMMAND asset-capture --backend-status)
set_tests_properties(asset-capture-backend-status PROPERTIES PASS_REGULAR_EXPRESSION "raw_socket_permission=")
add_test(NAME asset-capture-protocol-self-test COMMAND asset-capture --self-test-protocol)

add_executable(table-renderer-tests
    discovery/output/TableRendererTests.cpp
)
target_link_libraries(table-renderer-tests PRIVATE asset_discovery_output)
add_test(NAME table-renderer-tests COMMAND table-renderer-tests)

add_executable(json-renderer-tests
    discovery/output/JsonRendererTests.cpp
)
target_link_libraries(json-renderer-tests PRIVATE asset_discovery_output)
add_test(NAME json-renderer-tests COMMAND json-renderer-tests)

add_executable(csv-renderer-tests
    discovery/output/CsvRendererTests.cpp
)
target_link_libraries(csv-renderer-tests PRIVATE asset_discovery_output)
add_test(NAME csv-renderer-tests COMMAND csv-renderer-tests)

add_executable(event-sink-tests
    event/EventSinkTests.cpp
)
target_link_libraries(event-sink-tests PRIVATE asset_discovery_output)
add_test(NAME event-sink-tests COMMAND event-sink-tests)

add_executable(ethernet-frame-tests
    packet/primitives/EthernetFrameTests.cpp
)
target_link_libraries(ethernet-frame-tests PRIVATE asset_discovery_parser)
add_test(NAME ethernet-frame-tests COMMAND ethernet-frame-tests)

add_executable(arp-packet-tests
    packet/primitives/ArpPacketTests.cpp
)
target_link_libraries(arp-packet-tests PRIVATE asset_discovery_parser)
add_test(NAME arp-packet-tests COMMAND arp-packet-tests)

add_executable(packet-parser-tests
    packet/facade/PacketParserTests.cpp
)
target_link_libraries(packet-parser-tests PRIVATE asset_discovery_parser)
add_test(NAME packet-parser-tests COMMAND packet-parser-tests)

add_executable(packet-context-tests
    packet/parser-core/PacketContextTests.cpp
)
target_link_libraries(packet-context-tests PRIVATE asset_discovery_parser)
add_test(NAME packet-context-tests COMMAND packet-context-tests)

add_executable(parser-engine-tests
    packet/parser-core/ParserEngineTests.cpp
)
target_link_libraries(parser-engine-tests PRIVATE asset_discovery_parser)
add_test(NAME parser-engine-tests COMMAND parser-engine-tests)

add_executable(dns-plugin-tests
    packet/parser-plugins/DnsPluginTests.cpp
)
target_link_libraries(dns-plugin-tests PRIVATE asset_discovery_parser)
add_test(NAME dns-plugin-tests COMMAND dns-plugin-tests)

add_executable(asset-store-tests
    discovery/domain/AssetStoreTests.cpp
)
target_link_libraries(asset-store-tests PRIVATE asset_discovery_asset)
add_test(NAME asset-store-tests COMMAND asset-store-tests)

add_executable(asset-event-detector-tests
    event/AssetEventDetectorTests.cpp
)
target_link_libraries(asset-event-detector-tests PRIVATE asset_discovery_asset)
add_test(NAME asset-event-detector-tests COMMAND asset-event-detector-tests)

add_executable(asset-monitor-tests
    discovery/monitor/AssetMonitorTests.cpp
)
target_link_libraries(asset-monitor-tests PRIVATE asset_discovery_monitor)
add_test(NAME asset-monitor-tests COMMAND asset-monitor-tests)

add_executable(sqlite-writer-tests
    storage/SQLiteWriterTests.cpp
)
target_link_libraries(sqlite-writer-tests PRIVATE asset_discovery_storage)
add_test(NAME sqlite-writer-tests COMMAND sqlite-writer-tests)

add_executable(arguments-tests
    cli/ArgumentsTests.cpp
)
target_link_libraries(arguments-tests PRIVATE asset_discovery_cli)
add_test(NAME arguments-tests COMMAND arguments-tests)

add_executable(app-config-tests
    config/AppConfigTests.cpp
)
target_link_libraries(app-config-tests PRIVATE asset_discovery_config)
add_test(NAME app-config-tests COMMAND app-config-tests)

add_executable(capture-backend-tests
    capture/CaptureBackendTests.cpp
)
target_link_libraries(capture-backend-tests PRIVATE asset_discovery_capture)
add_test(NAME capture-backend-tests COMMAND capture-backend-tests)

add_executable(capture-child-supervisor-tests
    capture/CaptureChildSupervisorTests.cpp
)
target_link_libraries(capture-child-supervisor-tests PRIVATE asset_discovery_capture)
add_test(NAME capture-child-supervisor-tests COMMAND capture-child-supervisor-tests)

add_executable(capture-child-protocol-tests
    capture/CaptureChildProtocolTests.cpp
)
target_link_libraries(capture-child-protocol-tests PRIVATE asset_discovery_capture)
add_test(NAME capture-child-protocol-tests COMMAND capture-child-protocol-tests)

add_executable(capture-packet-stream-tests
    capture/CapturePacketStreamTests.cpp
)
target_link_libraries(capture-packet-stream-tests PRIVATE asset_discovery_capture)
add_test(NAME capture-packet-stream-tests COMMAND capture-packet-stream-tests)

add_executable(bounded-queue-tests
    system/BoundedQueueTests.cpp
)
target_link_libraries(bounded-queue-tests PRIVATE asset_discovery_live)
add_test(NAME bounded-queue-tests COMMAND bounded-queue-tests)

add_executable(live-capture-pipeline-tests
    app/LiveCapturePipelineTests.cpp
)
target_link_libraries(live-capture-pipeline-tests PRIVATE asset_discovery_live asset_discovery_output)
add_test(NAME live-capture-pipeline-tests COMMAND live-capture-pipeline-tests)

add_executable(core-session-tests
    core/CoreSessionTests.cpp
)
target_link_libraries(core-session-tests PRIVATE asset-core)
add_test(NAME core-session-tests COMMAND core-session-tests)

if(PCAP_FOUND)
    find_package(Python3 COMPONENTS Interpreter QUIET)

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
        NAME asset-discovery-pcap-missing-file
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/missing.pcap"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectFailureContains.cmake"
    )

    add_test(
        NAME asset-discovery-database-required
        COMMAND "${CMAKE_COMMAND}"
            "-DASSET_DISCOVERY_EXE=$<TARGET_FILE:asset-discovery>"
            "-DPCAP_PATH=${PROJECT_SOURCE_DIR}/samples/arp.pcap"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/database-required"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ValidateDatabaseRequired.cmake"
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
            NAME asset-discovery-pcap-multi-asset-json
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ValidateMultiAssetJsonOutput.py"
                "$<TARGET_FILE:asset-discovery>"
                "${PROJECT_SOURCE_DIR}/samples/multi-asset.pcap"
        )

        add_test(
            NAME asset-discovery-preserves-existing-database
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/ValidateCliPreservesDatabase.py"
                "$<TARGET_FILE:asset-discovery>"
                "${PROJECT_SOURCE_DIR}/samples/arp.pcap"
        )

    endif()
endif()

add_executable(gui-model-tests
    gui/GuiModelTests.cpp
    ../src/gui/AssetModel.cpp
    ../src/gui/InterfaceModel.cpp
    ../src/gui/LogModel.cpp
    ../src/gui/EmailAlertNotifier.cpp
    ../src/gui/DesktopRunConfig.cpp
    ../src/gui/CaptureController.cpp
    ../include/pnad/gui/AssetModel.hpp
    ../include/pnad/gui/InterfaceModel.hpp
    ../include/pnad/gui/LogModel.hpp
    ../include/pnad/gui/EmailAlertNotifier.hpp
    ../include/pnad/gui/DesktopRunConfig.hpp
    ../include/pnad/gui/CaptureController.hpp
)
set_target_properties(gui-model-tests PROPERTIES AUTOMOC ON)
target_include_directories(gui-model-tests PRIVATE ../include)
target_compile_definitions(gui-model-tests PRIVATE PNAD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(gui-model-tests PRIVATE
    asset-core
    ${QT_LIBRARIES}
)
add_test(NAME gui-model-tests COMMAND gui-model-tests)

add_test(
    NAME gui-smoke-test
    COMMAND "${CMAKE_COMMAND}"
        "-DGUI_EXE=$<TARGET_FILE:asset-discovery-gui>"
        "-DGUI_SMOKE_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/gui-smoke-data"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/RunGuiSmokeTest.cmake"
)

if(TARGET asset-tests)
    add_dependencies(asset-tests
        asset-capture
        asset-discovery
        asset-discovery-gui
        table-renderer-tests
        json-renderer-tests
        csv-renderer-tests
        event-sink-tests
        ethernet-frame-tests
        arp-packet-tests
        packet-parser-tests
        packet-context-tests
        parser-engine-tests
        dns-plugin-tests
        asset-store-tests
        asset-event-detector-tests
        asset-monitor-tests
        sqlite-writer-tests
        arguments-tests
        app-config-tests
        capture-backend-tests
        capture-child-supervisor-tests
        capture-child-protocol-tests
        capture-packet-stream-tests
        bounded-queue-tests
        live-capture-pipeline-tests
        core-session-tests
    )
    add_dependencies(asset-tests gui-model-tests)
endif()
