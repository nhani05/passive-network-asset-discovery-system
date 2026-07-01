if(NOT DEFINED ASSET_DISCOVERY_EXE)
    message(FATAL_ERROR "ASSET_DISCOVERY_EXE is required")
endif()

if(NOT DEFINED EXPECTED_OUTPUT)
    message(FATAL_ERROR "EXPECTED_OUTPUT is required")
endif()

if(NOT DEFINED PCAP_PATH)
    message(FATAL_ERROR "PCAP_PATH is required")
endif()

set(command_args "${ASSET_DISCOVERY_EXE}" --pcap "${PCAP_PATH}")
if(DEFINED FILTER)
    list(APPEND command_args --filter "${FILTER}")
endif()
list(APPEND command_args --output table)

set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/expect-output-db-stub")
set(STUB_DIR "${WORK_DIR}/bin")
file(MAKE_DIRECTORY "${STUB_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/logs")

set(STUB_PSQL "${STUB_DIR}/psql")
file(WRITE "${STUB_PSQL}" "#!/bin/sh\nset -eu\nwhile [ \"\$#\" -gt 0 ]; do\n  if [ \"\$1\" = \"-f\" ]; then\n    shift\n    cat \"\$1\" > /dev/null\n    exit 0\n  fi\n  shift\ndone\nexit 0\n")
file(CHMOD "${STUB_PSQL}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=${STUB_DIR}:$ENV{PATH}"
        "DATABASE_URL="
        "PGHOST=localhost"
        "PGPORT=5432"
        "PGDATABASE=asset_discovery"
        "PGUSER=postgres"
        "PGPASSWORD=postgres"
        "ASSET_DISCOVERY_EVENTS_JSON=${WORK_DIR}/logs/events.ndjson"
        ${command_args}
    WORKING_DIRECTORY "${WORK_DIR}"
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
