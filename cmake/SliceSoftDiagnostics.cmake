find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/slicesoft_diagnostics_runtime_$<CONFIG>.txt"
    CONTENT "spdlog=$<TARGET_FILE:spdlog::spdlog>\nfmt=$<TARGET_FILE:fmt::fmt>\nspdlogLicense=${spdlog_DIR}/copyright\nfmtLicense=${fmt_DIR}/copyright\n")

add_library(slicesoft_diagnostics STATIC src/diagnostics/LogEvent.cpp)
target_include_directories(slicesoft_diagnostics PUBLIC "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}")
target_link_libraries(slicesoft_diagnostics PRIVATE nlohmann_json::nlohmann_json)

add_library(slicesoft_diagnostics_transport STATIC
    src/diagnostics/transport/EventPipe.cpp
    src/diagnostics/transport/WorkerEnvironment.cpp
    src/diagnostics/transport/WorkerTelemetry.cpp)
target_link_libraries(slicesoft_diagnostics_transport PUBLIC slicesoft_diagnostics PRIVATE advapi32 bcrypt)

add_library(slicesoft_diagnostics_host STATIC
    src/diagnostics/host/LogSession.cpp
    src/diagnostics/host/SessionRetention.cpp
    src/diagnostics/host/ModuleLogBinding.cpp
    src/diagnostics/spdlog/FileLogSink.cpp)
target_link_libraries(slicesoft_diagnostics_host PUBLIC slicesoft_diagnostics PRIVATE spdlog::spdlog)

foreach(target slicesoft_diagnostics slicesoft_diagnostics_host slicesoft_diagnostics_transport)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    endif()
endforeach()

add_library(slicesoft_diagnostics_windows STATIC src/diagnostics/windows/CrashReporter.cpp)
target_include_directories(slicesoft_diagnostics_windows PUBLIC "${PROJECT_SOURCE_DIR}/src")
add_executable(slicer_crash_reporter
    apps/slicer_crash_reporter/main.cpp src/diagnostics/windows/CrashCapture.cpp)
target_include_directories(slicer_crash_reporter PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(slicer_crash_reporter PRIVATE dbghelp)

add_library(slicesoft_diagnostics_process STATIC src/diagnostics/host/ProcessDiagnostics.cpp)
target_link_libraries(slicesoft_diagnostics_process PUBLIC
    slicesoft_diagnostics_host slicesoft_diagnostics_windows)
add_dependencies(slicesoft_diagnostics_process slicer_crash_reporter)

foreach(target slicesoft_diagnostics_windows slicer_crash_reporter slicesoft_diagnostics_process)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    endif()
endforeach()

function(slicesoft_diagnostic_symbols)
    if(MSVC)
        foreach(target slicer_cli rip_reader_test slicer_ui_host_sim slicer_module slicer_worker slicer_crash_reporter)
            if(TARGET ${target})
                target_compile_options(${target} PRIVATE /Zi)
                target_link_options(${target} PRIVATE /DEBUG)
            endif()
        endforeach()
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL slicesoft_diagnostic_symbols)

function(slicesoft_register_diagnostic_tests)
    add_executable(diagnostics_host_tests tests/diagnostics/LogSessionTests.cpp)
    target_link_libraries(diagnostics_host_tests PRIVATE slicesoft_diagnostics_host
        nlohmann_json::nlohmann_json spdlog::spdlog)
    add_test(NAME diagnostics_host_tests COMMAND diagnostics_host_tests "${CMAKE_BINARY_DIR}/diagnostic-evidence/logs")
    add_executable(session_retention_tests tests/diagnostics/SessionRetentionTests.cpp)
    target_link_libraries(session_retention_tests PRIVATE slicesoft_diagnostics_host)
    add_test(NAME session_retention_tests COMMAND session_retention_tests "${CMAKE_BINARY_DIR}/diagnostic-evidence/retention")
    add_executable(diagnostics_host_adapter_tests tests/diagnostics/HostAdapterIntegrationTests.cpp)
    target_link_libraries(diagnostics_host_adapter_tests PRIVATE slicesoft_diagnostics_host spdlog::spdlog nlohmann_json::nlohmann_json)
    add_dependencies(diagnostics_host_adapter_tests slicer_module)
    add_test(NAME diagnostics_host_adapter_tests COMMAND diagnostics_host_adapter_tests
        $<TARGET_FILE:slicer_module> "${CMAKE_BINARY_DIR}/diagnostic-evidence/host-adapter")
    add_executable(diagnostics_c_header_test tests/diagnostics/LoggingHeaderC.c)
    target_include_directories(diagnostics_c_header_test PRIVATE "${PROJECT_SOURCE_DIR}")
    add_test(NAME diagnostics_c_header_test COMMAND diagnostics_c_header_test)

    add_executable(module_log_dispatcher_tests tests/diagnostics/ModuleLogDispatcherTests.cpp
        src/slicer_module/logging/ModuleLogDispatcher.cpp)
    target_compile_definitions(module_log_dispatcher_tests PRIVATE PM_MODULE_STATIC)
    target_link_libraries(module_log_dispatcher_tests PRIVATE slicesoft_diagnostics)
    add_test(NAME module_log_dispatcher_tests COMMAND module_log_dispatcher_tests)
    add_executable(module_logging_integration_tests tests/diagnostics/ModuleLoggingIntegrationTests.cpp)
    target_link_libraries(module_logging_integration_tests PRIVATE slicesoft_diagnostics)
    add_dependencies(module_logging_integration_tests slicer_module slicer_worker)
    add_test(NAME module_logging_integration_tests COMMAND module_logging_integration_tests $<TARGET_FILE:slicer_module>)
    add_executable(module_event_pipe_tests tests/diagnostics/ModuleEventPipeTests.cpp)
    target_link_libraries(module_event_pipe_tests PRIVATE slicesoft_diagnostics_transport)
    add_test(NAME module_event_pipe_tests COMMAND module_event_pipe_tests)

    add_executable(diagnostics_crash_child tests/diagnostics/CrashChild.cpp)
    target_link_libraries(diagnostics_crash_child PRIVATE slicesoft_diagnostics_windows)
    add_executable(diagnostics_crash_tests tests/diagnostics/CrashReporterTests.cpp)
    target_link_libraries(diagnostics_crash_tests PRIVATE slicesoft_diagnostics_windows dbghelp)
    add_dependencies(diagnostics_crash_tests diagnostics_crash_child slicer_crash_reporter)
    add_test(NAME diagnostics_crash_tests COMMAND diagnostics_crash_tests
        $<TARGET_FILE:diagnostics_crash_child> $<TARGET_FILE:slicer_crash_reporter>
        "${CMAKE_BINARY_DIR}/diagnostic-evidence/dumps")
    add_test(NAME diagnostics_packaging_tests COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_SOURCE_DIR}/tests/diagnostics/TestDiagnosticsPackaging.ps1"
        -BuildDir "${CMAKE_BINARY_DIR}" -Config $<CONFIG>)
    set_tests_properties(diagnostics_packaging_tests PROPERTIES TIMEOUT 60)
    set_tests_properties(diagnostics_host_tests module_log_dispatcher_tests
        module_logging_integration_tests module_event_pipe_tests diagnostics_crash_tests
        session_retention_tests diagnostics_host_adapter_tests PROPERTIES TIMEOUT 60)
    foreach(target diagnostics_crash_child diagnostics_crash_tests)
        target_compile_options(${target} PRIVATE /Zi /W4 /WX /utf-8)
        target_link_options(${target} PRIVATE /DEBUG)
    endforeach()
endfunction()
