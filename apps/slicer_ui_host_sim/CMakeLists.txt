add_executable(slicer_ui_host_sim
    camera/CameraController.cpp
    camera/CameraController.h
    CapabilityCoverageRequests.cpp
    CapabilityCoverageRequests.h
    CapabilityCoverageReport.cpp
    CapabilityCoverageRunner.cpp
    CapabilityCoverageRunner.h
    CapabilityCoverageWorkflow.cpp
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    HostModelListPanel.cpp
    HostModelListPanel.h
    HostProfileCatalog.cpp
    HostProfileCatalog.h
    HostProfilePanel.cpp
    HostProfilePanel.h
    HostTransformLayoutPanel.cpp
    HostTransformLayoutPanel.h
    HostMainWindow.cpp
    HostMainWindow.h
    HostMainWindowJob.cpp
    HostMainWindowProfile.cpp
    HostMainWindowScene.cpp
    HostMainWindowSettings.cpp
    HostSliceSettings.cpp
    HostSliceSettings.h
    HostSliceSettingsPanel.cpp
    HostSliceSettingsPanel.h
    HostSliceJobController.cpp
    HostSliceJobController.h
    HostSliceJobPanel.cpp
    HostSliceJobPanel.h
    Main.cpp
    ModuleClient.cpp
    ModuleClient.h
    MoveOptimizationPolicy.cpp
    MoveOptimizationPolicy.h
    SceneInteractionController.cpp
    SceneInteractionController.h
    TransformCommitPolicy.cpp
    TransformCommitPolicy.h
    ViewWorkspaceWidget.cpp
    ViewWorkspaceWidget.h
    render/AppearanceCache.cpp
    render/AppearanceCache.h
    render/CpuRasterBackend.cpp
    render/CpuRasterBackend.h
    render/CpuRasterDecor.cpp
    render/CpuRasterDecor.h
    render/CpuRasterResources.h
    render/CpuRasterizer.cpp
    render/IRenderBackend.h
    render/SceneRenderPolicy.cpp
    render/SceneRenderPolicyData.cpp
    render/SceneRenderPolicyMesh.cpp
    render/SceneRenderPolicy.h
    render/TopViewRenderPolicy.cpp
    render/TopViewRenderPolicyData.cpp
    render/TopViewRenderPolicy.h
    settings/ViewPresentationSettings.cpp
    settings/ViewPresentationSettings.h
    camera/ViewModeSwitch.cpp
    camera/ViewModeSwitch.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    PROPERTIES LANGUAGE CXX
)

set_target_properties(slicer_ui_host_sim PROPERTIES
    AUTOMOC ON
)

target_include_directories(slicer_ui_host_sim PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim
)

target_link_libraries(slicer_ui_host_sim PRIVATE
    Qt5::Widgets
    bcrypt
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
            "$<$<COMPILE_LANGUAGE:CXX>:/FI${qt515MsvcCompatibilityHeader}>"
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

add_test(
    NAME slicer_stage14e04b_capability_coverage_test
    COMMAND slicer_ui_host_sim
        --capability-self-test
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
        --evidence-root
            ${CMAKE_BINARY_DIR}/stage14e04b_evidence/$<CONFIG>
)

add_test(
    NAME hostflow_ha03_qt_end_to_end
    COMMAND slicer_ui_host_sim
        --capability-self-test
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
        --evidence-root
            ${CMAKE_BINARY_DIR}/hostflow_ha03_qt_evidence/$<CONFIG>
)

add_executable(hostflow_hb01_model_import_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostModelImportWorkflowTests.cpp
)

target_include_directories(hostflow_hb01_model_import_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hb01_model_import_tests PRIVATE
    Qt5::Core
    bcrypt
)

add_dependencies(hostflow_hb01_model_import_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hb01_model_import_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb01_model_import_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb01_model_import_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb01_model_import
    COMMAND hostflow_hb01_model_import_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME hostflow_hb01_import_ui_smoke
    COMMAND slicer_ui_host_sim
        --hostflow-import-ui-self-test
        --module $<TARGET_FILE:slicer_module>
)

add_executable(hostflow_hb02_model_list_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelListPanel.cpp
    HostModelListPanel.h
    ModuleClient.cpp
    ModuleClient.h
    ViewWorkspaceWidget.cpp
    ViewWorkspaceWidget.h
    camera/ViewModeSwitch.cpp
    camera/ViewModeSwitch.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostModelListPanelTests.cpp
)

set_target_properties(hostflow_hb02_model_list_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_hb02_model_list_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hb02_model_list_tests PRIVATE
    Qt5::Widgets
    bcrypt
)

add_dependencies(hostflow_hb02_model_list_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hb02_model_list_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb02_model_list_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb02_model_list_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb02_model_list_selection
    COMMAND hostflow_hb02_model_list_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_executable(hostflow_hb03_transform_layout_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    HostTransformLayoutPanel.cpp
    HostTransformLayoutPanel.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostTransformLayoutPanelTests.cpp
)

set_target_properties(hostflow_hb03_transform_layout_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_hb03_transform_layout_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hb03_transform_layout_tests PRIVATE
    Qt5::Widgets
    bcrypt
)

add_dependencies(hostflow_hb03_transform_layout_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hb03_transform_layout_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb03_transform_layout_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb03_transform_layout_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb03_transform_layout
    COMMAND hostflow_hb03_transform_layout_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_executable(hostflow_hb04_profile_catalog_tests
    HostProfileCatalog.cpp
    HostProfileCatalog.h
    HostProfilePanel.cpp
    HostProfilePanel.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostProfilePanelTests.cpp
)

set_target_properties(hostflow_hb04_profile_catalog_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_hb04_profile_catalog_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hb04_profile_catalog_tests PRIVATE
    Qt5::Widgets
    bcrypt
)

add_dependencies(hostflow_hb04_profile_catalog_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hb04_profile_catalog_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb04_profile_catalog_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb04_profile_catalog_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb04_profile_catalog
    COMMAND hostflow_hb04_profile_catalog_tests
        --module $<TARGET_FILE:slicer_module>
)

add_test(
    NAME hostflow_hb04_profile_ui_smoke
    COMMAND slicer_ui_host_sim
        --hostflow-profile-ui-self-test
        --module $<TARGET_FILE:slicer_module>
)

add_executable(hostflow_hb05_slice_settings_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    HostSliceSettings.cpp
    HostSliceSettings.h
    HostSliceSettingsPanel.cpp
    HostSliceSettingsPanel.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostSliceSettingsTests.cpp
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    PROPERTIES LANGUAGE CXX
)

set_target_properties(hostflow_hb05_slice_settings_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_hb05_slice_settings_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim
)

target_link_libraries(hostflow_hb05_slice_settings_tests PRIVATE
    Qt5::Widgets
    bcrypt
)

add_dependencies(hostflow_hb05_slice_settings_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hb05_slice_settings_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb05_slice_settings_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb05_slice_settings_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb05_slice_settings
    COMMAND hostflow_hb05_slice_settings_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME hostflow_hb05_settings_ui_smoke
    COMMAND slicer_ui_host_sim
        --hostflow-settings-ui-self-test
        --module $<TARGET_FILE:slicer_module>
)

add_executable(hostflow_hb06_slice_job_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    HostSliceJobController.cpp
    HostSliceJobController.h
    HostSliceSettings.cpp
    HostSliceSettings.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostSliceJobTests.cpp
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    PROPERTIES LANGUAGE CXX
)

set_target_properties(hostflow_hb06_slice_job_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_hb06_slice_job_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim
)

target_link_libraries(hostflow_hb06_slice_job_tests PRIVATE
    Qt5::Core
    bcrypt
)

add_dependencies(hostflow_hb06_slice_job_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hb06_slice_job_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb06_slice_job_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb06_slice_job_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb06_slice_job
    COMMAND hostflow_hb06_slice_job_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME hostflow_hb06_job_ui_smoke
    COMMAND slicer_ui_host_sim
        --hostflow-job-ui-self-test
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

add_executable(stage14e04_top_view_tests
    ModuleClient.cpp
    ModuleClient.h
    MoveOptimizationPolicy.cpp
    MoveOptimizationPolicy.h
    SceneInteractionController.cpp
    SceneInteractionController.h
    TransformCommitPolicy.cpp
    TransformCommitPolicy.h
    render/TopViewRenderPolicy.cpp
    render/TopViewRenderPolicyData.cpp
    render/TopViewRenderPolicy.h
    ${CMAKE_SOURCE_DIR}/tests/stage14e_04/Stage14E04TopViewTests.cpp
)

target_include_directories(stage14e04_top_view_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(stage14e04_top_view_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(stage14e04_top_view_tests PRIVATE
    Qt5::Core
    Qt5::Gui
)

add_dependencies(stage14e04_top_view_tests slicer_module)

if(MSVC)
    target_compile_options(stage14e04_top_view_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(stage14e04_top_view_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(stage14e04_top_view_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME slicer_stage14e04_top_view_contract_test
    COMMAND stage14e04_top_view_tests
        --module $<TARGET_FILE:slicer_module>
)

add_executable(stage14e04c_three_d_tests
    ModuleClient.cpp
    ModuleClient.h
    SceneInteractionController.cpp
    SceneInteractionController.h
    TransformCommitPolicy.cpp
    TransformCommitPolicy.h
    camera/CameraController.cpp
    camera/CameraController.h
    render/AppearanceCache.cpp
    render/AppearanceCache.h
    render/CpuRasterBackend.cpp
    render/CpuRasterBackend.h
    render/CpuRasterDecor.cpp
    render/CpuRasterDecor.h
    render/CpuRasterResources.h
    render/CpuRasterizer.cpp
    render/IRenderBackend.h
    render/SceneRenderPolicy.cpp
    render/SceneRenderPolicyData.cpp
    render/SceneRenderPolicyMesh.cpp
    render/SceneRenderPolicy.h
    ${CMAKE_SOURCE_DIR}/tests/stage14e_04c/Stage14E04CThreeDTests.cpp
)

target_include_directories(stage14e04c_three_d_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(stage14e04c_three_d_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(stage14e04c_three_d_tests PRIVATE
    Qt5::Core
)

add_dependencies(stage14e04c_three_d_tests slicer_module)

if(MSVC)
    target_compile_options(stage14e04c_three_d_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(stage14e04c_three_d_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(stage14e04c_three_d_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME slicer_stage14e04c_three_d_contract_test
    COMMAND stage14e04c_three_d_tests
        --module $<TARGET_FILE:slicer_module>
)

add_executable(stage14e04d_view_switch_tests
    ModuleClient.cpp
    ModuleClient.h
    SceneInteractionController.cpp
    SceneInteractionController.h
    TransformCommitPolicy.cpp
    TransformCommitPolicy.h
    ViewWorkspaceWidget.cpp
    ViewWorkspaceWidget.h
    camera/CameraController.cpp
    camera/CameraController.h
    camera/ViewModeSwitch.cpp
    camera/ViewModeSwitch.h
    render/AppearanceCache.cpp
    render/AppearanceCache.h
    render/CpuRasterBackend.cpp
    render/CpuRasterBackend.h
    render/CpuRasterDecor.cpp
    render/CpuRasterDecor.h
    render/CpuRasterResources.h
    render/CpuRasterizer.cpp
    render/IRenderBackend.h
    render/SceneRenderPolicy.cpp
    render/SceneRenderPolicyData.cpp
    render/SceneRenderPolicyMesh.cpp
    render/SceneRenderPolicy.h
    render/TopViewRenderPolicy.cpp
    render/TopViewRenderPolicyData.cpp
    render/TopViewRenderPolicy.h
    settings/ViewPresentationSettings.cpp
    settings/ViewPresentationSettings.h
    ${CMAKE_SOURCE_DIR}/tests/stage14e_04d/Stage14E04DViewSwitchTests.cpp
)

target_include_directories(stage14e04d_view_switch_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(stage14e04d_view_switch_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(stage14e04d_view_switch_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

add_dependencies(stage14e04d_view_switch_tests slicer_module)

if(MSVC)
    target_compile_options(stage14e04d_view_switch_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(stage14e04d_view_switch_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(stage14e04d_view_switch_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME slicer_stage14e04d_dual_view_contract_test
    COMMAND stage14e04d_view_switch_tests
        --module $<TARGET_FILE:slicer_module>
)

set_tests_properties(
    slicer_stage14e04d_dual_view_contract_test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_tests_properties(
    slicer_stage14e02_qt_host_smoke_test
    slicer_stage14e04b_capability_coverage_test
    hostflow_ha03_qt_end_to_end
    hostflow_hb01_import_ui_smoke
    hostflow_hb02_model_list_selection
    hostflow_hb03_transform_layout
    hostflow_hb04_profile_catalog
    hostflow_hb04_profile_ui_smoke
    hostflow_hb05_slice_settings
    hostflow_hb05_settings_ui_smoke
    hostflow_hb06_job_ui_smoke
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_tests_properties(
    hostflow_ha03_qt_end_to_end
    hostflow_hb06_slice_job
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 180
)
