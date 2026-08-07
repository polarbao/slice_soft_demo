add_executable(slicer_ui_host_sim
    HostMainWindow.cpp
    HostMainWindow.h
    Main.cpp
    ModuleClient.cpp
    ModuleClient.h
    SceneInteractionController.cpp
    SceneInteractionController.h
    TransformCommitPolicy.cpp
    TransformCommitPolicy.h
)

set_target_properties(slicer_ui_host_sim PROPERTIES
    AUTOMOC ON
)

target_include_directories(slicer_ui_host_sim PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(slicer_ui_host_sim PRIVATE
    Qt5::Widgets
)

add_dependencies(slicer_ui_host_sim slicer_module)

if(MSVC)
    target_compile_options(slicer_ui_host_sim PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        set(qt515MsvcCompatibilityHeader
            "${CMAKE_CURRENT_SOURCE_DIR}/Qt515MsvcCompatibility.h"
        )
        target_sources(slicer_ui_host_sim PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(slicer_ui_host_sim PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

if(Python3_Interpreter_FOUND)
    add_test(
        NAME slicer_stage14e02_qt_host_boundary_test
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/stage14e_02/ValidateQtHostBoundary.py
            --repo-root ${CMAKE_SOURCE_DIR}
            --binary $<TARGET_FILE:slicer_ui_host_sim>
    )

    add_test(
        NAME slicer_stage14e02_qt_host_missing_module_test
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/stage14e_02/ValidateMissingModule.py
            --host $<TARGET_FILE:slicer_ui_host_sim>
    )
endif()

add_test(
    NAME slicer_stage14e02_qt_host_smoke_test
    COMMAND slicer_ui_host_sim
        --self-test
        --module $<TARGET_FILE:slicer_module>
)

add_executable(stage14e03_interaction_tests
    ModuleClient.cpp
    ModuleClient.h
    SceneInteractionController.cpp
    SceneInteractionController.h
    TransformCommitPolicy.cpp
    TransformCommitPolicy.h
    ${CMAKE_SOURCE_DIR}/tests/stage14e_03/Stage14E03InteractionTests.cpp
)

target_include_directories(stage14e03_interaction_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(stage14e03_interaction_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(stage14e03_interaction_tests PRIVATE
    Qt5::Core
)

add_dependencies(stage14e03_interaction_tests slicer_module)

if(MSVC)
    target_compile_options(stage14e03_interaction_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(stage14e03_interaction_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(stage14e03_interaction_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME slicer_stage14e03_interaction_contract_test
    COMMAND stage14e03_interaction_tests
        --module $<TARGET_FILE:slicer_module>
)

set_tests_properties(
    slicer_stage14e02_qt_host_smoke_test
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
