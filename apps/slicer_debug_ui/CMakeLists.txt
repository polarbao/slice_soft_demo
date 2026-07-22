add_executable(slicer_debug_ui
    main.cpp
    MainWindow.cpp
    MainWindow.h
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
    services/ProcessRunner.cpp
    services/ProcessRunner.h
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
    services/LayerPreviewDataProvider.cpp
    services/LayerPreviewDataProvider.h
    services/MaterialClosureReportInterpreter.cpp
    services/MaterialClosureReportInterpreter.h
    services/ModelPreflightController.cpp
    services/ModelPreflightController.h
    services/ModelPreflightPresenter.cpp
    services/ModelPreflightPresenter.h
    services/PreviewReportIndex.cpp
    services/PreviewReportIndex.h
    services/UiSmokeTestRunner.cpp
    services/UiSmokeTestRunner.h
    widgets/LogPanel.cpp
    widgets/LogPanel.h
    widgets/ChannelChartPanel.cpp
    widgets/ChannelChartPanel.h
    widgets/ConfigEditorPanel.cpp
    widgets/ConfigEditorPanel.h
    widgets/DiagnosticsDock.cpp
    widgets/DiagnosticsDock.h
    widgets/ConfigDiffPanel.cpp
    widgets/ConfigDiffPanel.h
    widgets/MaterialProcessPanel.cpp
    widgets/MaterialProcessPanel.h
    widgets/ModelPreflightPanel.cpp
    widgets/ModelPreflightPanel.h
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

if(MSVC)
    target_compile_options(slicer_debug_ui PRIVATE /utf-8 /MP)

    if(MSVC_VERSION GREATER_EQUAL 1950)
        set(qt515MsvcCompatibilityHeader
            "${CMAKE_CURRENT_SOURCE_DIR}/compat/Qt515MsvcCompatibility.h"
        )
        target_sources(slicer_debug_ui PRIVATE ${qt515MsvcCompatibilityHeader})
        target_compile_options(slicer_debug_ui PRIVATE
            "/FI${qt515MsvcCompatibilityHeader}"
        )
    endif()
endif()
