if(NOT DEFINED PACKAGE_DIR)
    message(FATAL_ERROR "PACKAGE_DIR is required")
endif()

if(EXISTS "${PACKAGE_DIR}/bin/asset-discovery")
    message(FATAL_ERROR "Desktop package must not include the CLI product binary")
endif()

if(NOT EXISTS "${PACKAGE_DIR}/bin/asset-discovery-gui")
    message(FATAL_ERROR "Desktop package must include the GUI product binary")
endif()

file(GLOB_RECURSE package_text
    "${PACKAGE_DIR}/*.md"
    "${PACKAGE_DIR}/*.desktop"
    "${PACKAGE_DIR}/*.sh")

foreach(path IN LISTS package_text)
    file(READ "${path}" content)
    if(content MATCHES "--pcap|--interface|--sqlite|--output|--capture-backend|CLI binary|command-line product")
        message(FATAL_ERROR "Desktop package advertises CLI product workflow in ${path}")
    endif()
endforeach()
