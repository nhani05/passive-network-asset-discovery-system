if(NOT DEFINED ASSET_DISCOVERY_EXE)
    message(FATAL_ERROR "ASSET_DISCOVERY_EXE is required")
endif()

if(NOT DEFINED PCAP_PATH)
    message(FATAL_ERROR "PCAP_PATH is required")
endif()

execute_process(
    COMMAND "${ASSET_DISCOVERY_EXE}" --pcap "${PCAP_PATH}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(command_result EQUAL 0)
    message(FATAL_ERROR "Expected command to fail, but it exited with 0")
endif()

set(combined_output "${command_output}${command_error}")
string(FIND "${combined_output}" "${PCAP_PATH}" path_position)
if(path_position EQUAL -1)
    message(FATAL_ERROR "Expected failure output to contain '${PCAP_PATH}', got: ${combined_output}")
endif()
