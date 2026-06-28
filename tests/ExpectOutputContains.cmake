if(NOT DEFINED ASSET_DISCOVERY_EXE)
    message(FATAL_ERROR "ASSET_DISCOVERY_EXE is required")
endif()

if(NOT DEFINED EXPECTED_OUTPUT)
    message(FATAL_ERROR "EXPECTED_OUTPUT is required")
endif()

if(NOT DEFINED PCAP_PATH)
    message(FATAL_ERROR "PCAP_PATH is required")
endif()

execute_process(
    COMMAND "${ASSET_DISCOVERY_EXE}" --pcap "${PCAP_PATH}" --output table
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Expected command to succeed, got ${command_result}: ${command_output}${command_error}")
endif()

set(combined_output "${command_output}${command_error}")
string(FIND "${combined_output}" "${EXPECTED_OUTPUT}" output_position)
if(output_position EQUAL -1)
    message(FATAL_ERROR "Expected output to contain '${EXPECTED_OUTPUT}', got: ${combined_output}")
endif()
