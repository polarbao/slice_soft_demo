add_executable(slicer_debug_ui
    main.cpp
    MainWindow.cpp
    MainWindow.h
    models/SceneDocument.cpp
    models/SceneDocument.h
    models/SceneProjectionRequest.h
    models/SceneSelectionModel.cpp
    models/SceneSelectionModel.h
    services/ConfigDocument.cpp
    services/ConfigDocument.h
    services/ConfigDiffModel.cpp
    services/ConfigDiffModel.h
    services/ConfigValidator.cpp
    services/ConfigValidator.h
    services/EffectiveConfigGenerator.cpp
    services/EffectiveConfigGenerator.h
    services/HelpTextProvider.cpp
    services/HelpTextProvider.h
    services/OpenVdbUtilityReportInterpreter.cpp
    services/OpenVdbUtilityReportInterpreter.h
    services/PackageLoader.cpp
    services/PackageLoader.h
    services/MaterialPreviewImageAdapter.cpp
    services/MaterialPreviewImageAdapter.h
    services/ProcessRunner.cpp
    services/ProcessRunner.h
    services/ProductionPackageResultValidator.cpp
    services/ProductionPackageResultValidator.h
    services/ProductionModeCatalog.cpp
    services/ProductionModeCatalog.h
    services/ProductionProfileSourceResolver.cpp
    services/ProductionProfileSourceResolver.h
    services/ProductionSliceRunSession.cpp
    services/ProductionSliceRunSession.h
    services/ReportLoader.cpp
    services/ReportLoader.h
    services/ScenarioRegistry.cpp
    services/ScenarioRegistry.h
    services/SliceSettingsModel.cpp
    services/SliceSettingsModel.h
    services/SliceProgressProtocolParser.cpp
    services/SliceProgressProtocolParser.h
    services/SlicePreflightCoordinator.cpp
    services/SlicePreflightCoordinator.h
    services/ToolPaths.cpp
    services/ToolPaths.h
    services/TiffLayerLoadWorker.cpp
    services/TiffLayerLoadWorker.h
    services/TransformedModelPreflightLoader.cpp
    services/TransformedModelPreflightLoader.h
    services/WorkspaceLayoutState.cpp
    services/WorkspaceLayoutState.h
    services/LayerPreviewDataProvider.cpp
    services/LayerPreviewDataProvider.h
    services/MaterialClosureReportInterpreter.cpp
    services/MaterialClosureReportInterpreter.h
    services/ModelPreflightController.cpp
    services/ModelPreflightController.h
    services/ModelPreflightPresenter.cpp
    services/ModelPreflightPresenter.h
    services/ModelTopViewLoader.cpp
    services/ModelTopViewLoader.h
    services/SceneModelRepository.cpp
    services/SceneModelRepository.h
    controllers/SceneTransformController.cpp
    controllers/SceneTransformController.h
    controllers/SceneSliceActionController.cpp
    controllers/SceneSliceActionController.h
    services/PreviewReportIndex.cpp
    services/PreviewReportIndex.h
    services/PreviewPhysicalScale.cpp
    services/PreviewPhysicalScale.h
    services/UiSmokeTestRunner.cpp
    services/UiSmokeTestRunner.h
    widgets/LogPanel.cpp
    widgets/LogPanel.h
    widgets/ChannelChartPanel.cpp
    widgets/ChannelChartPanel.h
    widgets/ConfigEditorPanel.cpp
    widgets/ConfigEditorPanel.h
    widgets/ContextInspector.cpp
    widgets/ContextInspector.h
    widgets/DiagnosticsDock.cpp
    widgets/DiagnosticsDock.h
    widgets/ConfigDiffPanel.cpp
    widgets/ConfigDiffPanel.h
    widgets/MaterialProcessPanel.cpp
    widgets/MaterialProcessPanel.h
    widgets/ModelPreflightPanel.cpp
    widgets/ModelPreflightPanel.h
    widgets/ModelListPanel.cpp
    widgets/ModelListPanel.h
    widgets/SceneLayoutPanel.cpp
    widgets/SceneLayoutPanel.h
    widgets/SceneActionBar.cpp
    widgets/SceneActionBar.h
    widgets/ModelTransformPanel.cpp
    widgets/ModelTransformPanel.h
    widgets/ModelTopViewWidget.cpp
    widgets/ModelTopViewWidget.h
    widgets/MaterialClosurePanel.cpp
    widgets/MaterialClosurePanel.h
    widgets/MaterialPolicyEditor.cpp
    widgets/MaterialPolicyEditor.h
    widgets/MaterialProcessProfileEditor.cpp
    widgets/MaterialProcessProfileEditor.h
    widgets/MaterialRoleMappingEditor.cpp
    widgets/MaterialRoleMappingEditor.h
    widgets/LayerPreviewPanel.cpp
    widgets/LayerPreviewPanel.h
    widgets/PreviewPanel.cpp
    widgets/PreviewPanel.h
    widgets/PreviewOverlayPanel.cpp
    widgets/PreviewOverlayPanel.h
    widgets/PreviewWorkspace.cpp
    widgets/PreviewWorkspace.h
    widgets/ProjectToolsDock.cpp
    widgets/ProjectToolsDock.h
    widgets/ProductionModePanel.cpp
    widgets/ProductionModePanel.h
    widgets/QuickConfigPanel.cpp
    widgets/QuickConfigPanel.h
    widgets/SettingHelpPanel.cpp
    widgets/SettingHelpPanel.h
    widgets/SliceTimingPanel.cpp
    widgets/SliceTimingPanel.h
    widgets/ReportPanel.cpp
    widgets/ReportPanel.h
    widgets/SupportEditor.cpp
    widgets/SupportEditor.h
)

set_target_properties(slicer_debug_ui PROPERTIES
    AUTOMOC ON
)

target_link_libraries(slicer_debug_ui PRIVATE Qt5::Widgets slicer_core)

add_executable(production_effective_config_unit_tests
    services/ConfigDiffModel.cpp
    services/ConfigDiffModel.h
    services/ConfigValidator.cpp
    services/ConfigValidator.h
    services/EffectiveConfigGenerator.cpp
    services/EffectiveConfigGenerator.h
    services/ProductionModeCatalog.cpp
    services/ProductionModeCatalog.h
    services/SliceSettingsModel.cpp
    services/SliceSettingsModel.h
    ../../tests/unit/production_effective_config/Main.cpp
)

target_compile_definitions(production_effective_config_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_include_directories(production_effective_config_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(production_effective_config_unit_tests PRIVATE
    Qt5::Core
    slicer_core
)

add_test(
    NAME production_effective_config_unit_tests
    COMMAND production_effective_config_unit_tests
)

add_executable(production_slice_route_process_tests
    services/ProcessRunner.cpp
    services/ProcessRunner.h
    services/ProductionModeCatalog.cpp
    services/ProductionModeCatalog.h
    services/ProductionProfileSourceResolver.cpp
    services/ProductionProfileSourceResolver.h
    services/ProductionSliceRunSession.cpp
    services/ProductionSliceRunSession.h
    ../../tests/unit/production_slice_route/Main.cpp
)

set_target_properties(production_slice_route_process_tests PROPERTIES
    AUTOMOC ON
)

target_compile_definitions(production_slice_route_process_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_include_directories(production_slice_route_process_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(production_slice_route_process_tests PRIVATE
    Qt5::Core
    slicer_core
)

add_test(
    NAME production_slice_route_process_tests
    COMMAND production_slice_route_process_tests
)

add_executable(production_package_result_unit_tests
    services/PackageLoader.cpp
    services/PackageLoader.h
    services/ProductionModeCatalog.cpp
    services/ProductionModeCatalog.h
    services/ProductionPackageResultValidator.cpp
    services/ProductionPackageResultValidator.h
    services/ProductionSliceRunSession.cpp
    services/ProductionSliceRunSession.h
    ../../tests/unit/production_package_result/Main.cpp
)

target_include_directories(production_package_result_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(production_package_result_unit_tests PRIVATE
    Qt5::Core
    slicer_core
)

add_test(
    NAME production_package_result_unit_tests
    COMMAND production_package_result_unit_tests
)

add_executable(scene_transform_controller_unit_tests
    models/SceneDocument.cpp
    models/SceneDocument.h
    models/SceneSelectionModel.cpp
    models/SceneSelectionModel.h
    services/SceneModelRepository.cpp
    services/SceneModelRepository.h
    controllers/SceneTransformController.cpp
    controllers/SceneTransformController.h
    ../../tests/unit/scene_transform_controller/Main.cpp
)

set_target_properties(scene_transform_controller_unit_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(scene_transform_controller_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
)

target_link_libraries(scene_transform_controller_unit_tests PRIVATE
    Qt5::Core
    slicer_core
)

add_test(
    NAME scene_transform_controller_unit_tests
    COMMAND scene_transform_controller_unit_tests
)

add_executable(scene_document_unit_tests
    models/SceneDocument.cpp
    models/SceneDocument.h
    models/SceneSelectionModel.cpp
    models/SceneSelectionModel.h
    ../../tests/unit/scene_document/Main.cpp
)

set_target_properties(scene_document_unit_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(scene_document_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/models
)

target_link_libraries(scene_document_unit_tests PRIVATE
    Qt5::Core
    slicer_core
)

add_test(
    NAME scene_document_unit_tests
    COMMAND scene_document_unit_tests
)

if(MSVC)
    target_compile_options(slicer_debug_ui PRIVATE /utf-8 /MP)
    target_compile_options(
        production_effective_config_unit_tests
        PRIVATE
        /utf-8
        /MP
    )
    target_compile_options(
        production_slice_route_process_tests
        PRIVATE
        /utf-8
        /MP
    )
    target_compile_options(
        production_package_result_unit_tests
        PRIVATE
        /utf-8
        /MP
    )
    target_compile_options(
        scene_transform_controller_unit_tests
        PRIVATE
        /utf-8
        /MP
    )
    target_compile_options(
        scene_document_unit_tests
        PRIVATE
        /utf-8
        /MP
    )

    if(MSVC_VERSION GREATER_EQUAL 1950)
        set(qt515MsvcCompatibilityHeader
            "${CMAKE_CURRENT_SOURCE_DIR}/compat/Qt515MsvcCompatibility.h"
        )
        target_sources(slicer_debug_ui PRIVATE ${qt515MsvcCompatibilityHeader})
        target_sources(
            production_effective_config_unit_tests
            PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_sources(
            production_slice_route_process_tests
            PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_sources(
            production_package_result_unit_tests
            PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_sources(
            scene_transform_controller_unit_tests
            PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_sources(
            scene_document_unit_tests
            PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(slicer_debug_ui PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
        target_compile_options(
            production_effective_config_unit_tests
            PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
        target_compile_options(
            production_slice_route_process_tests
            PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
        target_compile_options(
            production_package_result_unit_tests
            PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
        target_compile_options(
            scene_transform_controller_unit_tests
            PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
        target_compile_options(
            scene_document_unit_tests
            PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

target_sources(slicer_debug_ui PRIVATE
    controllers/SceneBatchImportController.cpp
    controllers/SceneBatchImportController.h
)

add_executable(scene_batch_import_controller_unit_tests
    models/SceneDocument.cpp
    models/SceneDocument.h
    controllers/SceneBatchImportController.cpp
    controllers/SceneBatchImportController.h
    ../../tests/unit/scene_batch_import_controller/Main.cpp
)

set_target_properties(
    scene_batch_import_controller_unit_tests
    PROPERTIES
    AUTOMOC ON
)

target_include_directories(
    scene_batch_import_controller_unit_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
)

target_link_libraries(
    scene_batch_import_controller_unit_tests
    PRIVATE
    Qt5::Core
    slicer_core
)

if(MSVC)
    target_compile_options(
        scene_batch_import_controller_unit_tests
        PRIVATE
        /utf-8
        /MP
    )
    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(
            scene_batch_import_controller_unit_tests
            PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(
            scene_batch_import_controller_unit_tests
            PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME scene_batch_import_controller_unit_tests
    COMMAND scene_batch_import_controller_unit_tests
)

add_executable(scene_slice_action_controller_unit_tests
    controllers/SceneSliceActionController.cpp
    controllers/SceneSliceActionController.h
    ../../tests/unit/scene_slice_action_controller/Main.cpp
)

set_target_properties(
    scene_slice_action_controller_unit_tests
    PROPERTIES
    AUTOMOC ON
)

target_include_directories(
    scene_slice_action_controller_unit_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
)

target_link_libraries(
    scene_slice_action_controller_unit_tests
    PRIVATE
    Qt5::Core
    slicer_core
)

if(MSVC)
    target_compile_options(
        scene_slice_action_controller_unit_tests
        PRIVATE
        /utf-8
        /MP
    )
    if(MSVC_VERSION GREATER_EQUAL 1950)
        target_sources(
            scene_slice_action_controller_unit_tests
            PRIVATE
            ${qt515MsvcCompatibilityHeader}
        )
        target_compile_options(
            scene_slice_action_controller_unit_tests
            PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()

add_test(
    NAME scene_slice_action_controller_unit_tests
    COMMAND scene_slice_action_controller_unit_tests
)
