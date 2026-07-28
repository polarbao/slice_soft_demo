execute_process(
    COMMAND
        "${SLICER_CLI}"
        --scene-config
        "${MISSING_SCENE_CONFIG}"
    RESULT_VARIABLE route_result
    OUTPUT_VARIABLE route_output
    ERROR_VARIABLE route_error
)

if(NOT route_result EQUAL 2)
    message(FATAL_ERROR
        "scene route returned ${route_result}, expected stable exit code 2")
endif()

if(NOT route_error MATCHES "SCENE_EFFECTIVE_CONFIG_INVALID")
    message(FATAL_ERROR
        "scene route did not emit SCENE_EFFECTIVE_CONFIG_INVALID: ${route_error}")
endif()

execute_process(
    COMMAND
        "${SLICER_CLI}"
        --config
        "samples/configs/slice_config.json"
        --scene-config
        "${MISSING_SCENE_CONFIG}"
    RESULT_VARIABLE combined_result
    OUTPUT_VARIABLE combined_output
    ERROR_VARIABLE combined_error
)

if(NOT combined_result EQUAL 2)
    message(FATAL_ERROR
        "combined route returned ${combined_result}, expected stable exit code 2")
endif()

if(NOT combined_error MATCHES "SCENE_EFFECTIVE_CONFIG_INVALID")
    message(FATAL_ERROR
        "combined route did not fail closed: ${combined_error}")
endif()
