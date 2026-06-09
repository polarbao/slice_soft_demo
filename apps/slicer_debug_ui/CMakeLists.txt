add_executable(slicer_debug_ui
    main.cpp
    MainWindow.cpp
    MainWindow.h
    services/PackageLoader.cpp
    services/PackageLoader.h
    services/ProcessRunner.cpp
    services/ProcessRunner.h
    services/ReportLoader.cpp
    services/ReportLoader.h
    services/ToolPaths.cpp
    services/ToolPaths.h
    widgets/LogPanel.cpp
    widgets/LogPanel.h
    widgets/MaterialProcessPanel.cpp
    widgets/MaterialProcessPanel.h
    widgets/PreviewPanel.cpp
    widgets/PreviewPanel.h
    widgets/ReportPanel.cpp
    widgets/ReportPanel.h
)

set_target_properties(slicer_debug_ui PROPERTIES
    AUTOMOC ON
)

target_link_libraries(slicer_debug_ui PRIVATE Qt5::Widgets)

