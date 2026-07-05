if(NOT DEFINED TARGET_NAME)
    message(FATAL_ERROR "TARGET_NAME is required")
endif()

if(NOT DEFINED LINK_LIBRARIES)
    set(LINK_LIBRARIES "")
endif()

if(NOT DEFINED FORBIDDEN_PATTERNS)
    set(FORBIDDEN_PATTERNS "")
endif()

foreach(link_item IN LISTS LINK_LIBRARIES)
    foreach(pattern IN LISTS FORBIDDEN_PATTERNS)
        if(link_item MATCHES "${pattern}")
            message(FATAL_ERROR
                "${TARGET_NAME} links forbidden dependency '${link_item}' matching '${pattern}'")
        endif()
    endforeach()
endforeach()

message(STATUS "${TARGET_NAME} dependency boundary check passed")
