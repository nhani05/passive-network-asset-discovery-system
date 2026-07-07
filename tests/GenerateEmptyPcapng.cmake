if(NOT DEFINED OUTPUT_PATH)
    message(FATAL_ERROR "OUTPUT_PATH is required")
endif()

execute_process(
    COMMAND /bin/sh -c "printf '\\012\\015\\015\\012\\034\\000\\000\\000\\115\\074\\053\\032\\001\\000\\000\\000\\377\\377\\377\\377\\377\\377\\377\\377\\034\\000\\000\\000\\001\\000\\000\\000\\024\\000\\000\\000\\001\\000\\000\\000\\377\\377\\000\\000\\024\\000\\000\\000' > \"$1\"" sh "${OUTPUT_PATH}"
    RESULT_VARIABLE write_result
)

if(NOT write_result EQUAL 0)
    message(FATAL_ERROR "Failed to write empty PCAPNG fixture to ${OUTPUT_PATH}")
endif()
