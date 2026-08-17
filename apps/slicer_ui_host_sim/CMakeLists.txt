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
    HostImportDirectoryPolicy.cpp
    HostImportDirectoryPolicy.h
    HostModelListPanel.cpp
    HostModelListPanel.h
    HostChannelChartWidget.cpp
    HostChannelChartWidget.h
    HostPackageReviewController.cpp
    HostPackageReviewController.h
    HostPackageReviewPanel.cpp
    HostPackageReviewPanel.h
    HostRipJobController.cpp
    HostRipJobController.h
    HostRipSafety.cpp
    HostRipSafety.h
    HostRipSettingsPanel.cpp
    HostRipSettingsPanel.h
    HostRipSettingsStore.cpp
    HostRipSettingsStore.h
    HostProfileCatalog.cpp
    HostProfileCatalog.h
    HostProfilePanel.cpp
    HostProfilePanel.h
    HostProcessPresetCatalog.cpp
    HostProcessPresetCatalog.h
    HostTransformLayoutPanel.cpp
    HostTransformLayoutPanel.h
    HostWorkspaceState.cpp
    HostWorkspaceState.h
    HostWorkspaceTextureState.cpp
    HostWorkspaceTextureState.h
    HostVersionInfo.cpp
    HostVersionInfo.h
    HostMainWindow.cpp
    HostMainWindow.h
    HostMainWindowImport.cpp
    HostMainWindowJob.cpp
    HostMainWindowProfile.cpp
    HostMainWindowRip.cpp
    HostMainWindowResult.cpp
    HostMainWindowScene.cpp
    HostMainWindowSettings.cpp
    HostMainWindowState.cpp
    HostMainWindowPreflight.cpp
    HostMainWindowView.cpp
    HostMaterialSettingsPanel.cpp
    HostMaterialSettingsPanel.h
    HostTextureSettingsPanel.cpp
    HostTextureSettingsPanel.h
    HostTextureWhitePreflightService.cpp
    HostTextureWhitePreflightService.h
    HostSliceSettings.cpp
    HostTextureProfileBridge.cpp
    HostTextureProfileBridge.h
    HostSliceSettings.h
    HostSliceSettingsPanel.cpp
    HostSliceSettingsPanel.h
    HostSupportSettingsPanel.cpp
    HostSupportSettingsPanel.h
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
    TopViewCanvasWidget.cpp
    TopViewCanvasWidget.h
    ThreeDCanvasWidget.cpp
    ThreeDCanvasWidget.h
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
    render/MeshAttributeDecoder.cpp
    render/MeshAttributeDecoder.h
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
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
)

slicesoft_add_windows_version_resource(
    slicer_ui_host_sim
    application
    "SliceSoft Packaged Slicer"
    "slicer_ui_host_sim.exe"
)
slicesoft_copy_version_manifests(slicer_ui_host_sim)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
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
    slicesoft_rip_integration
    bcrypt
)

add_dependencies(slicer_ui_host_sim slicer_module slicer_worker)

# ModuleClient loads the capability DLL at runtime, so the linker cannot infer
# the app-local module, worker, manifest, or their dynamic dependencies.
set(slicesoftHostRuntimeFiles
    $<TARGET_FILE:slicer_module>
    $<TARGET_FILE:slicer_worker>
    $<TARGET_FILE_DIR:slicer_module>/module.json
)

get_target_property(
    slicesoftMeshoptimizerTargetType
    meshoptimizer::meshoptimizer
    TYPE
)
if(slicesoftMeshoptimizerTargetType MATCHES
    "^(SHARED_LIBRARY|MODULE_LIBRARY|UNKNOWN_LIBRARY)$"
)
    list(APPEND slicesoftHostRuntimeFiles
        $<TARGET_FILE:meshoptimizer::meshoptimizer>
    )
endif()

if(_slicesoft_tiff_runtime_file)
    list(APPEND slicesoftHostRuntimeFiles
        ${_slicesoft_tiff_runtime_file}
    )
endif()

set(slicesoftPackagedUserGuide
    "${CMAKE_SOURCE_DIR}/docs/user_guides/SLICE_PRODUCT_PACKAGED_新版切片软件使用手册.md"
)
set(slicesoftPackagedUserGuideAssets
    "${CMAKE_SOURCE_DIR}/docs/user_guides/assets/packaged_slicer"
)
if(NOT EXISTS "${slicesoftPackagedUserGuide}")
    message(FATAL_ERROR
        "Packaged slicer user guide is missing: ${slicesoftPackagedUserGuide}"
    )
endif()
if(NOT IS_DIRECTORY "${slicesoftPackagedUserGuideAssets}")
    message(FATAL_ERROR
        "Packaged slicer user guide assets are missing: ${slicesoftPackagedUserGuideAssets}"
    )
endif()

add_custom_command(TARGET slicer_ui_host_sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${slicesoftHostRuntimeFiles}
        $<TARGET_FILE_DIR:slicer_ui_host_sim>
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:slicer_ui_host_sim>/docs/user_guides/assets/packaged_slicer"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${slicesoftPackagedUserGuide}"
        "$<TARGET_FILE_DIR:slicer_ui_host_sim>/docs/user_guides/SLICE_PRODUCT_PACKAGED_新版切片软件使用手册.md"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${slicesoftPackagedUserGuideAssets}"
        "$<TARGET_FILE_DIR:slicer_ui_host_sim>/docs/user_guides/assets/packaged_slicer"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/rip_project/rip_cli.exe")
    find_program(slicesoftPowerShellExecutable NAMES powershell pwsh REQUIRED)
    add_custom_command(TARGET slicer_ui_host_sim POST_BUILD
        COMMAND ${slicesoftPowerShellExecutable}
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/PackageRipModule.ps1"
            -SourceRoot "${CMAKE_SOURCE_DIR}/rip_project"
            -Destination "$<TARGET_FILE_DIR:slicer_ui_host_sim>/modules/rip"
            -ReplaceOwnedDestination
        VERBATIM
    )
else()
    message(WARNING
        "External RIP SDK is absent; the host will keep RIP disabled at runtime."
    )
endif()

if(MSVC)
    target_compile_options(slicer_ui_host_sim PRIVATE
        ${SLICESOFT_MSVC_PARALLEL_COMPILE_OPTION}
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
        NAME ripflow_host_wiring_contract
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/ripflow/ValidateRipflowWiring.py
            ${CMAKE_SOURCE_DIR}
    )
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
    NAME slicer_stage14e02_qt_host_default_module_test
    COMMAND slicer_ui_host_sim --self-test
)

add_executable(ripflow_settings_unit_tests
    HostRipSettingsStore.cpp
    HostRipSettingsStore.h
    ${CMAKE_SOURCE_DIR}/tests/ripflow/HostRipSettingsTests.cpp
)
set_target_properties(ripflow_settings_unit_tests PROPERTIES AUTOMOC ON)
target_include_directories(ripflow_settings_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ripflow_settings_unit_tests PRIVATE Qt5::Core)
if(MSVC)
    target_compile_options(ripflow_settings_unit_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )
    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(ripflow_settings_unit_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(ripflow_settings_unit_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()
add_test(NAME ripflow_settings_unit_tests COMMAND ripflow_settings_unit_tests)

add_executable(ripflow_safety_unit_tests
    HostRipSafety.cpp
    HostRipSafety.h
    ${CMAKE_SOURCE_DIR}/tests/ripflow/HostRipSafetyTests.cpp
)
target_include_directories(ripflow_safety_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ripflow_safety_unit_tests PRIVATE Qt5::Core)
if(MSVC)
    target_compile_options(ripflow_safety_unit_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )
    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(ripflow_safety_unit_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(ripflow_safety_unit_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()
add_test(NAME ripflow_safety_unit_tests COMMAND ripflow_safety_unit_tests)

add_executable(ripflow_fake_cli EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/ripflow/FakeRipCli.cpp
)
if(MSVC)
    target_compile_options(ripflow_fake_cli PRIVATE /W4 /WX /utf-8)
endif()

if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/rip_project/rip_cli.exe")
    add_test(
        NAME ripflow_module_self_test
        COMMAND slicer_ui_host_sim --rip-module-self-test
    )
    set_tests_properties(
        ripflow_module_self_test
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
add_test(
    NAME ripflow_ui_self_test
    COMMAND slicer_ui_host_sim
        --rip-ui-self-test
        --module $<TARGET_FILE:slicer_module>
)
set_tests_properties(
    ripflow_ui_self_test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
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
    HostImportDirectoryPolicy.cpp
    HostImportDirectoryPolicy.h
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
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
    NAME hostflow_he02_batch_import
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

add_executable(hostflow_he01_stl_import_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    HostSliceJobController.cpp
    HostSliceJobController.h
    HostSliceSettings.cpp
    HostTextureProfileBridge.cpp
    HostTextureProfileBridge.h
    HostSliceSettings.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostStlImportTests.cpp
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    PROPERTIES LANGUAGE CXX
)

set_target_properties(hostflow_he01_stl_import_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_he01_stl_import_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim
)

target_link_libraries(hostflow_he01_stl_import_tests PRIVATE
    Qt5::Core
    bcrypt
)

add_dependencies(hostflow_he01_stl_import_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_he01_stl_import_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_he01_stl_import_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_he01_stl_import_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_he01_stl_import
    COMMAND hostflow_he01_stl_import_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

set_tests_properties(
    hostflow_he01_stl_import
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 180
)

add_executable(hostflow_hb02_model_list_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelListPanel.cpp
    HostModelListPanel.h
    ModuleClient.cpp
    ModuleClient.h
    ThreeDCanvasWidget.cpp
    ThreeDCanvasWidget.h
    TopViewCanvasWidget.cpp
    TopViewCanvasWidget.h
    ViewWorkspaceWidget.cpp
    ViewWorkspaceWidget.h
    camera/CameraController.cpp
    camera/CameraController.h
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
    HostProcessPresetCatalog.cpp
    HostProcessPresetCatalog.h
    HostSliceSettings.cpp
    HostTextureProfileBridge.cpp
    HostTextureProfileBridge.h
    HostSliceSettings.h
    HostMaterialSettingsPanel.cpp
    HostMaterialSettingsPanel.h
    HostTextureSettingsPanel.cpp
    HostTextureSettingsPanel.h
    HostSliceSettingsPanel.cpp
    HostSliceSettingsPanel.h
    HostSliceJobPanel.cpp
    HostSliceJobPanel.h
    HostPackageReviewPanel.cpp
    HostPackageReviewPanel.h
    HostPackageReviewController.h
    HostChannelChartWidget.cpp
    HostChannelChartWidget.h
    HostSupportSettingsPanel.cpp
    HostSupportSettingsPanel.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostSliceSettingsTests.cpp
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
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
    slicer_base
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
    NAME hostflow_he03_support_settings
    COMMAND hostflow_hb05_slice_settings_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME hostflow_he04_material_profile
    COMMAND hostflow_hb05_slice_settings_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME hostflow_he05_texture_profile
    COMMAND hostflow_hb05_slice_settings_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_executable(hostflow_he06_texture_white_preflight_tests
    HostTextureWhitePreflightService.cpp
    HostTextureWhitePreflightService.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostTextureWhitePreflightTests.cpp
)

set_target_properties(
    hostflow_he06_texture_white_preflight_tests
    PROPERTIES AUTOMOC ON
)

target_include_directories(
    hostflow_he06_texture_white_preflight_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    hostflow_he06_texture_white_preflight_tests
    PRIVATE Qt5::Core Qt5::Gui
)

if(MSVC)
    target_compile_options(
        hostflow_he06_texture_white_preflight_tests
        PRIVATE /MP /W4 /WX /utf-8
    )
    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(
            hostflow_he06_texture_white_preflight_tests
            PRIVATE ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(
            hostflow_he06_texture_white_preflight_tests
            PRIVATE "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_he06_texture_white_preflight
    COMMAND hostflow_he06_texture_white_preflight_tests
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
    HostTextureProfileBridge.cpp
    HostTextureProfileBridge.h
    HostSliceSettings.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostSliceJobTests.cpp
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
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

add_executable(hostflow_hb07_package_review_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    HostPackageReviewController.cpp
    HostPackageReviewController.h
    HostSliceJobController.cpp
    HostSliceJobController.h
    HostSliceSettings.cpp
    HostTextureProfileBridge.cpp
    HostTextureProfileBridge.h
    HostSliceSettings.h
    ModuleClient.cpp
    ModuleClient.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostPackageReviewTests.cpp
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    PROPERTIES LANGUAGE CXX
)

set_target_properties(hostflow_hb07_package_review_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_hb07_package_review_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim
)

target_link_libraries(hostflow_hb07_package_review_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    bcrypt
)

add_dependencies(hostflow_hb07_package_review_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hb07_package_review_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb07_package_review_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb07_package_review_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb07_package_review
    COMMAND hostflow_hb07_package_review_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME hostflow_hb07_result_ui_smoke
    COMMAND slicer_ui_host_sim
        --hostflow-result-ui-self-test
        --module $<TARGET_FILE:slicer_module>
)

add_executable(hostflow_hd05_package_directory_tests
    HostChannelChartWidget.cpp
    HostChannelChartWidget.h
    HostPackageReviewPanel.cpp
    HostPackageReviewPanel.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostPackageDirectoryPanelTests.cpp
)

set_target_properties(hostflow_hd05_package_directory_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(hostflow_hd05_package_directory_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hd05_package_directory_tests PRIVATE
    Qt5::Widgets
)

if(MSVC)
    target_compile_options(hostflow_hd05_package_directory_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hd05_package_directory_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hd05_package_directory_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hd05_package_directory
    COMMAND hostflow_hd05_package_directory_tests
)

add_executable(hostflow_hb08_workspace_state_tests
    HostSliceSettings.cpp
    HostTextureProfileBridge.cpp
    HostTextureProfileBridge.h
    HostSliceSettings.h
    HostWorkspaceState.cpp
    HostWorkspaceState.h
    HostWorkspaceTextureState.cpp
    HostWorkspaceTextureState.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.h
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostWorkspaceStateTests.cpp
)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostMaterialProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostTextureProfile.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/HostRequestBuilder.c
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim/JsonText.c
    PROPERTIES LANGUAGE CXX
)

target_include_directories(hostflow_hb08_workspace_state_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/slicer_host_sim
)

target_link_libraries(hostflow_hb08_workspace_state_tests PRIVATE
    Qt5::Widgets
    bcrypt
)

if(MSVC)
    target_compile_options(hostflow_hb08_workspace_state_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hb08_workspace_state_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hb08_workspace_state_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hb08_workspace_state
    COMMAND hostflow_hb08_workspace_state_tests
)

add_test(
    NAME hostflow_he03_support_persistence
    COMMAND hostflow_hb08_workspace_state_tests
)

add_test(
    NAME hostflow_he04_material_persistence
    COMMAND hostflow_hb08_workspace_state_tests
)

add_test(
    NAME hostflow_he05_texture_persistence
    COMMAND hostflow_hb08_workspace_state_tests
)

add_test(
    NAME hostflow_hb08_workspace_ui_smoke
    COMMAND slicer_ui_host_sim
        --hostflow-workspace-ui-self-test
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
    render/MeshAttributeDecoder.cpp
    render/MeshAttributeDecoder.h
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
    TopViewCanvasWidget.cpp
    TopViewCanvasWidget.h
    ThreeDCanvasWidget.cpp
    ThreeDCanvasWidget.h
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
    render/MeshAttributeDecoder.cpp
    render/MeshAttributeDecoder.h
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

add_executable(hostflow_hd01_top_canvas_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    ModuleClient.cpp
    ModuleClient.h
    TopViewCanvasWidget.cpp
    TopViewCanvasWidget.h
    ThreeDCanvasWidget.cpp
    ThreeDCanvasWidget.h
    ViewWorkspaceWidget.cpp
    ViewWorkspaceWidget.h
    camera/CameraController.cpp
    camera/CameraController.h
    camera/ViewModeSwitch.cpp
    camera/ViewModeSwitch.h
    render/TopViewRenderPolicy.cpp
    render/TopViewRenderPolicyData.cpp
    render/TopViewRenderPolicy.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostTopViewCanvasTests.cpp
)

target_include_directories(hostflow_hd01_top_canvas_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hd01_top_canvas_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    bcrypt
)

add_dependencies(hostflow_hd01_top_canvas_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hd01_top_canvas_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hd01_top_canvas_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hd01_top_canvas_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hd01_top_canvas
    COMMAND hostflow_hd01_top_canvas_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
        --evidence-root ${CMAKE_BINARY_DIR}/hostflow_hd01_evidence/$<CONFIG>
)

add_executable(hostflow_hd02_three_d_canvas_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    ModuleClient.cpp
    ModuleClient.h
    ThreeDCanvasWidget.cpp
    ThreeDCanvasWidget.h
    TopViewCanvasWidget.cpp
    TopViewCanvasWidget.h
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
    render/MeshAttributeDecoder.cpp
    render/MeshAttributeDecoder.h
    render/SceneRenderPolicy.cpp
    render/SceneRenderPolicyData.cpp
    render/SceneRenderPolicyMesh.cpp
    render/SceneRenderPolicy.h
    render/TopViewRenderPolicy.cpp
    render/TopViewRenderPolicyData.cpp
    render/TopViewRenderPolicy.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostThreeDCanvasTests.cpp
)

target_include_directories(hostflow_hd02_three_d_canvas_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hd02_three_d_canvas_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    bcrypt
)

add_dependencies(hostflow_hd02_three_d_canvas_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hd02_three_d_canvas_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hd02_three_d_canvas_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hd02_three_d_canvas_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hd02_three_d_canvas
    COMMAND hostflow_hd02_three_d_canvas_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
        --evidence-root ${CMAKE_BINARY_DIR}/hostflow_hd02_evidence/$<CONFIG>
)

add_test(
    NAME hostflow_hd02_real_asset_matrix
    COMMAND hostflow_hd02_three_d_canvas_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
        --evidence-root ${CMAKE_BINARY_DIR}/hostflow_hd02_evidence/$<CONFIG>
        --real-asset-matrix
)

add_executable(hostflow_hd03_drag_interaction_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    ModuleClient.cpp
    ModuleClient.h
    MoveOptimizationPolicy.cpp
    MoveOptimizationPolicy.h
    SceneInteractionController.cpp
    SceneInteractionController.h
    TopViewCanvasWidget.cpp
    TopViewCanvasWidget.h
    TransformCommitPolicy.cpp
    TransformCommitPolicy.h
    render/TopViewRenderPolicy.cpp
    render/TopViewRenderPolicyData.cpp
    render/TopViewRenderPolicy.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostDragInteractionTests.cpp
)

target_include_directories(hostflow_hd03_drag_interaction_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hd03_drag_interaction_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    bcrypt
)

add_dependencies(hostflow_hd03_drag_interaction_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hd03_drag_interaction_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hd03_drag_interaction_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hd03_drag_interaction_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hd03_drag_interaction
    COMMAND hostflow_hd03_drag_interaction_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
        --evidence-root ${CMAKE_BINARY_DIR}/hostflow_hd03_evidence/$<CONFIG>
)

add_executable(hostflow_hd04_scene_refresh_tests
    HostModelImportWorkflow.cpp
    HostModelImportWorkflow.h
    HostModelImportWorkflowScene.cpp
    ModuleClient.cpp
    ModuleClient.h
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
    render/MeshAttributeDecoder.cpp
    render/MeshAttributeDecoder.h
    render/SceneRenderPolicy.cpp
    render/SceneRenderPolicyData.cpp
    render/SceneRenderPolicyMesh.cpp
    render/SceneRenderPolicy.h
    render/TopViewRenderPolicy.cpp
    render/TopViewRenderPolicyData.cpp
    render/TopViewRenderPolicy.h
    ${CMAKE_SOURCE_DIR}/tests/hostflow/HostSceneRefreshTests.cpp
)

target_include_directories(hostflow_hd04_scene_refresh_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(hostflow_hd04_scene_refresh_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    bcrypt
)

add_dependencies(hostflow_hd04_scene_refresh_tests slicer_module)

if(MSVC)
    target_compile_options(hostflow_hd04_scene_refresh_tests PRIVATE
        /MP
        /W4
        /WX
        /utf-8
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(hostflow_hd04_scene_refresh_tests PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(hostflow_hd04_scene_refresh_tests PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME hostflow_hd04_scene_refresh
    COMMAND hostflow_hd04_scene_refresh_tests
        --module $<TARGET_FILE:slicer_module>
        --repo-root ${CMAKE_SOURCE_DIR}
)

set_tests_properties(
    slicer_stage14e04d_dual_view_contract_test
    hostflow_hd01_top_canvas
    hostflow_hd02_three_d_canvas
    hostflow_hd02_real_asset_matrix
    hostflow_hd03_drag_interaction
    hostflow_hd04_scene_refresh
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_tests_properties(
    hostflow_hd01_top_canvas
    hostflow_hd02_three_d_canvas
    hostflow_hd03_drag_interaction
    hostflow_hd04_scene_refresh
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 180
)

set_tests_properties(
    hostflow_hd02_real_asset_matrix
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 900
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
    hostflow_he03_support_settings
    hostflow_he04_material_profile
    hostflow_he05_texture_profile
    hostflow_hb05_settings_ui_smoke
    hostflow_hb06_job_ui_smoke
    hostflow_hb07_result_ui_smoke
    hostflow_hd05_package_directory
    hostflow_hb08_workspace_state
    hostflow_he03_support_persistence
    hostflow_he04_material_persistence
    hostflow_he05_texture_persistence
    hostflow_hb08_workspace_ui_smoke
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_tests_properties(
    hostflow_ha03_qt_end_to_end
    hostflow_hb06_slice_job
    hostflow_hb07_package_review
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 180
)
