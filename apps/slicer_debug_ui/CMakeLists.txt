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
    services/PackageLoader.cpp
    services/PackageLoader.h
    services/ProcessRunner.cpp
    services/ProcessRunner.h
    services/ReportLoader.cpp
    services/ReportLoader.h
    services/ToolPaths.cpp
    services/ToolPaths.h
    services/LayerPreviewDataProvider.cpp
    services/LayerPreviewDataProvider.h
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
    widgets/ConfigDiffPanel.cpp
    widgets/ConfigDiffPanel.h
    widgets/MaterialProcessPanel.cpp
    widgets/MaterialProcessPanel.h
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
    widgets/QuickConfigPanel.cpp
    widgets/QuickConfigPanel.h
    widgets/ReportPanel.cpp
    widgets/ReportPanel.h
    widgets/SupportEditor.cpp
    widgets/SupportEditor.h
)

set_target_properties(slicer_debug_ui PROPERTIES
    AUTOMOC ON
)

target_link_libraries(slicer_debug_ui PRIVATE Qt5::Widgets)

if(MSVC)
    target_compile_options(slicer_debug_ui PRIVATE /utf-8)
endif()
