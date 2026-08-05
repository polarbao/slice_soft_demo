if(NOT DEFINED ERROR_CODE_FILE)
    message(FATAL_ERROR "ERROR_CODE_FILE is required")
endif()

file(READ "${ERROR_CODE_FILE}" errorCodeJson)
string(JSON contractVersion GET "${errorCodeJson}" contractVersion)
if(NOT contractVersion STREQUAL "1.0")
    message(FATAL_ERROR "Unexpected error-code contract version: ${contractVersion}")
endif()

string(JSON codeCount LENGTH "${errorCodeJson}" codes)
if(NOT codeCount EQUAL 19)
    message(FATAL_ERROR "Expected 19 registered error codes, found ${codeCount}")
endif()

set(seenCodes "")
math(EXPR lastCodeIndex "${codeCount} - 1")
foreach(codeIndex RANGE 0 ${lastCodeIndex})
    string(JSON code GET "${errorCodeJson}" codes ${codeIndex} code)
    if(code IN_LIST seenCodes)
        message(FATAL_ERROR "Duplicate error code: ${code}")
    endif()
    list(APPEND seenCodes "${code}")
endforeach()

foreach(requiredCode
    PM-SLICER-VIEWDATA-STALE
    PM-SLICER-VIEWDATA-BUDGET
    PM-SLICER-CONTRACT-0060
    PM-SLICER-RESOURCE-0041
)
    if(NOT requiredCode IN_LIST seenCodes)
        message(FATAL_ERROR "Required error code is missing: ${requiredCode}")
    endif()
endforeach()

message(STATUS "slicer_error_codes.json contains 19 unique registered codes")
