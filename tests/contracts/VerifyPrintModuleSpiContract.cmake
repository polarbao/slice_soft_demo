if(NOT DEFINED CONTRACT_HEADER)
    message(FATAL_ERROR "CONTRACT_HEADER is required")
endif()

file(READ "${CONTRACT_HEADER}" contractText)
string(
    REGEX MATCHALL
    "PM_API[ \t]+[^;\r\n]+PM_CALL[ \t]+pm_[a-z_]+[ \t]*\\("
    declarations
    "${contractText}"
)
list(LENGTH declarations declarationCount)
if(NOT declarationCount EQUAL 11)
    message(FATAL_ERROR "Expected 11 pm_* declarations, found ${declarationCount}")
endif()

set(expectedFunctions
    pm_spi_version
    pm_module_info
    pm_create
    pm_destroy
    pm_submit
    pm_poll
    pm_cancel
    pm_result
    pm_release
    pm_self_test
    pm_last_error
)

foreach(functionName IN LISTS expectedFunctions)
    string(
        REGEX MATCHALL
        "PM_CALL[ \t]+${functionName}[ \t]*\\("
        functionMatches
        "${contractText}"
    )
    list(LENGTH functionMatches functionCount)
    if(NOT functionCount EQUAL 1)
        message(FATAL_ERROR "Expected exactly one declaration for ${functionName}, found ${functionCount}")
    endif()
endforeach()

message(STATUS "print_module_spi.h declares exactly 11 expected pm_* functions")
