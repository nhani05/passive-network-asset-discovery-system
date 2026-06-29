if(NOT DEFINED ASSET_DISCOVERY_EXE)
    message(FATAL_ERROR "ASSET_DISCOVERY_EXE is required")
endif()

if(NOT DEFINED PCAP_PATH)
    message(FATAL_ERROR "PCAP_PATH is required")
endif()

if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR is required")
endif()

set(STUB_DIR "${CMAKE_CURRENT_BINARY_DIR}/psql-stub")
file(MAKE_DIRECTORY "${STUB_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(STUB_PSQL "${STUB_DIR}/psql")
file(WRITE "${STUB_PSQL}" "#!/bin/sh\nset -eu\nlog=\"${WORK_DIR}/postgres-write.log\"\nattempt_file=\"${WORK_DIR}/postgres-write-attempts\"\nif [ -f \"\$attempt_file\" ]; then\n  attempts=\$(cat \"\$attempt_file\")\nelse\n  attempts=0\nfi\nattempts=\$((attempts + 1))\nprintf '%s' \"\$attempts\" > \"\$attempt_file\"\nprintf '%s\\n' \"\$0\" \"\$@\" >> \"\$log\"\nquiet=0\nfor arg in \"\$@\"; do\n  if [ \"\$arg\" = \"-q\" ]; then\n    quiet=1\n  fi\ndone\nif [ \"\$attempts\" -eq 1 ]; then\n  echo 'psql: error: could not translate host name \"db\" to address: Temporary failure in name resolution' >&2\n  exit 2\nfi\nif [ \"\$quiet\" -eq 0 ]; then\n  echo 'CREATE TABLE'\nfi\nwhile [ \"\$#\" -gt 0 ]; do\n  if [ \"\$1\" = \"-f\" ]; then\n    shift\n    cat \"\$1\" >> \"\$log\"\n    exit 0\n  fi\n  shift\ndone\nexit 0\n")
file(CHMOD "${STUB_PSQL}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(ENV_PATH "${STUB_DIR}:$ENV{PATH}")
file(WRITE "${WORK_DIR}/.env" "DB_HOST=localhost\nDB_PORT=5432\nDB_NAME=asset_discovery\nDB_USER=postgres\nDB_PASSWORD=123456\n")
file(REMOVE "${WORK_DIR}/postgres-write.log")
file(REMOVE "${WORK_DIR}/postgres-write-attempts")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=${ENV_PATH}"
        "DATABASE_URL="
        "PGHOST="
        "PGPORT="
        "PGDATABASE="
        "PGUSER="
        "PGPASSWORD="
        "PGSERVICE="
        "${ASSET_DISCOVERY_EXE}"
        --pcap "${PCAP_PATH}"
        --output json
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Expected command to succeed, got ${command_result}: ${command_output}${command_error}")
endif()

file(READ "${WORK_DIR}/postgres-write.log" logged_sql)
file(READ "${WORK_DIR}/postgres-write-attempts" write_attempts)
if(NOT write_attempts STREQUAL "2")
    message(FATAL_ERROR "Expected PostgreSQL write to retry once, got ${write_attempts} attempt(s)")
endif()

string(FIND "${logged_sql}" "CREATE TABLE IF NOT EXISTS assets" schema_position)
if(schema_position EQUAL -1)
    message(FATAL_ERROR "Expected PostgreSQL schema SQL in log, got: ${logged_sql}")
endif()

string(FIND "${logged_sql}" "\n-q\n" quiet_position)
if(quiet_position EQUAL -1)
    message(FATAL_ERROR "Expected PostgreSQL command to use psql -q, got: ${logged_sql}")
endif()

string(FIND "${logged_sql}" "SET client_min_messages TO warning" message_level_position)
if(message_level_position EQUAL -1)
    message(FATAL_ERROR "Expected SQL to suppress expected schema notices, got: ${logged_sql}")
endif()

string(FIND "${logged_sql}" "postgresql://" url_position)
if(url_position GREATER -1)
    message(FATAL_ERROR "Expected command line to avoid embedded connection strings, got: ${logged_sql}")
endif()

string(FIND "${logged_sql}" "INSERT INTO assets" insert_position)
if(insert_position EQUAL -1)
    message(FATAL_ERROR "Expected INSERT SQL in log, got: ${logged_sql}")
endif()

string(FIND "${command_output}" "\"mac_address\": \"02:42:ac:11:00:02\"" output_position)
if(output_position EQUAL -1)
    message(FATAL_ERROR "Expected JSON output to contain MAC address, got: ${command_output}${command_error}")
endif()

string(FIND "${command_output}" "CREATE TABLE" psql_output_position)
if(psql_output_position GREATER -1)
    message(FATAL_ERROR "Expected psql success output to stay out of CLI stdout, got: ${command_output}")
endif()
