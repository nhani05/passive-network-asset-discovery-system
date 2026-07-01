if(NOT DEFINED ASSET_DISCOVERY_EXE)
    message(FATAL_ERROR "ASSET_DISCOVERY_EXE is required")
endif()

if(NOT DEFINED PCAP_PATH)
    message(FATAL_ERROR "PCAP_PATH is required")
endif()

if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR is required")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "DATABASE_URL="
        "PGHOST="
        "PGPORT="
        "PGDATABASE="
        "PGUSER="
        "PGPASSWORD="
        "PGSERVICE="
        "DB_HOST="
        "DB_PORT="
        "DB_NAME="
        "DB_USER="
        "DB_PASSWORD="
        "${ASSET_DISCOVERY_EXE}" --pcap "${PCAP_PATH}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(command_result EQUAL 0)
    message(FATAL_ERROR "Expected command to fail without database configuration")
endif()

set(combined_output "${command_output}${command_error}")
string(FIND "${combined_output}" "PostgreSQL configuration is required" error_position)
if(error_position EQUAL -1)
    message(FATAL_ERROR "Expected required database configuration error, got: ${combined_output}")
endif()
