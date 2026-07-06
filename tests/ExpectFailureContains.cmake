if(NOT DEFINED ASSET_DISCOVERY_EXE)
    message(FATAL_ERROR "ASSET_DISCOVERY_EXE is required")
endif()

if(NOT DEFINED PCAP_PATH)
    message(FATAL_ERROR "PCAP_PATH is required")
endif()

set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/expect-failure-sqlite")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "SQLITE_DATABASE_PATH=${WORK_DIR}/assets.db"
        "${ASSET_DISCOVERY_EXE}" --pcap "${PCAP_PATH}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(command_result EQUAL 0)
    message(FATAL_ERROR "Expected command to fail, but it exited with code 0")
endif()

set(combined_output "${command_output}${command_error}")
string(FIND "${combined_output}" "${PCAP_PATH}" path_position)
if(path_position EQUAL -1)
    message(FATAL_ERROR "Expected error output to contain '${PCAP_PATH}', got: ${combined_output}")
endif()
