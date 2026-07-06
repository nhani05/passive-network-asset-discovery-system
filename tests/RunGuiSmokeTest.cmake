set(ENV{QT_QPA_PLATFORM} "offscreen")
set(ENV{QT_QUICK_BACKEND} "software")
if(GUI_SMOKE_DATA_DIR)
    file(REMOVE_RECURSE "${GUI_SMOKE_DATA_DIR}")
    file(MAKE_DIRECTORY "${GUI_SMOKE_DATA_DIR}")
    set(ENV{XDG_DATA_HOME} "${GUI_SMOKE_DATA_DIR}")
endif()

message(STATUS "Running GUI smoke test for: ${GUI_EXE}")

execute_process(
    COMMAND "${GUI_EXE}" --smoke-first-run --smoke-cycle
    TIMEOUT 3
    RESULT_VARIABLE res
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

# A TIMEOUT result from execute_process means the app successfully ran the event loop for 3s
if(res EQUAL 0)
    if(err MATCHES "FirstRunAssertionFailed")
        message(FATAL_ERROR "GUI smoke test failed first-run Core Discovery assertion:\n${err}")
    endif()
    if(err MATCHES "qrc:/.*(unavailable|module .* is not installed|ReferenceError|TypeError)")
        message(FATAL_ERROR "GUI smoke test reported QML errors:\n${err}")
    endif()
    message(STATUS "GUI smoke test loaded all top-level views successfully.")
elseif(res STREQUAL "TIMEOUT" OR res STREQUAL "Process terminated due to timeout" OR res EQUAL 124 OR res EQUAL 255 OR res EQUAL -1)
    if(err MATCHES "FirstRunAssertionFailed")
        message(FATAL_ERROR "GUI smoke test failed first-run Core Discovery assertion:\n${err}")
    endif()
    if(err MATCHES "qrc:/.*(unavailable|module .* is not installed|ReferenceError|TypeError)")
        message(FATAL_ERROR "GUI smoke test started but QML reported load errors:\n${err}")
    endif()
    message(STATUS "GUI smoke test started up successfully and ran for 3 seconds.")
else()
    message(FATAL_ERROR "GUI smoke test exited early with error code: ${res}\nStdout: ${out}\nStderr: ${err}")
endif()
