if(NOT DEFINED ASSET_DISCOVERY_EXE)
    message(FATAL_ERROR "Cần ASSET_DISCOVERY_EXE")
endif()

if(NOT DEFINED PCAP_PATH)
    message(FATAL_ERROR "Cần PCAP_PATH")
endif()

execute_process(
    COMMAND "${ASSET_DISCOVERY_EXE}" --pcap "${PCAP_PATH}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(command_result EQUAL 0)
    message(FATAL_ERROR "Lệnh được kỳ vọng thất bại nhưng lại thoát với mã 0")
endif()

set(combined_output "${command_output}${command_error}")
string(FIND "${combined_output}" "${PCAP_PATH}" path_position)
if(path_position EQUAL -1)
    message(FATAL_ERROR "Output lỗi được kỳ vọng chứa '${PCAP_PATH}', nhưng nhận được: ${combined_output}")
endif()
