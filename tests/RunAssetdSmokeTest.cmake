if(NOT DEFINED ASSETD_EXE)
    message(FATAL_ERROR "ASSETD_EXE is required")
endif()

execute_process(
    COMMAND "${ASSETD_EXE}" --listen-address 127.0.0.1 --port 18080
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "assetd smoke test failed with ${command_result}: ${command_output}${command_error}")
endif()

set(combined_output "${command_output}${command_error}")
foreach(expected IN ITEMS "assetd backend service started." "status: healthy" "listen: 127.0.0.1:18080")
    string(FIND "${combined_output}" "${expected}" output_position)
    if(output_position EQUAL -1)
        message(FATAL_ERROR "Expected assetd output to contain '${expected}', got: ${combined_output}")
    endif()
endforeach()
