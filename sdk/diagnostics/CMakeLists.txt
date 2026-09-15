cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SliceSoftDiagnosticsSdk LANGUAGES CXX)
    # Prefix-only dependencies may offer Debug/Release and unconfigured targets.
    foreach(config RELWITHDEBINFO MINSIZEREL)
        if(NOT DEFINED CMAKE_MAP_IMPORTED_CONFIG_${config})
            set(CMAKE_MAP_IMPORTED_CONFIG_${config} "Release;")
        endif()
    endforeach()
endif()

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This source SDK requires Windows x64 and MSVC")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contracts/slicer_logging.h")
    message(FATAL_ERROR "Use the exported SDK root, not sdk/diagnostics in the source repository")
endif()

option(SLICESOFT_DIAGNOSTICS_BUILD_CRASH "Build the optional crash client and helper" OFF)
if(TARGET slicesoft_diagnostics OR TARGET slicesoft_diagnostics_host)
    message(FATAL_ERROR "SliceSoft diagnostic targets already exist; reuse them instead of adding this SDK twice")
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json CONFIG REQUIRED)
endif()
if(NOT TARGET spdlog::spdlog)
    find_package(spdlog CONFIG REQUIRED)
endif()

function(slicesoft_sdk_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    set_target_properties(${target} PROPERTIES
        AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endfunction()

add_library(slicesoft_diagnostics STATIC src/diagnostics/LogEvent.cpp)
add_library(SliceSoft::Diagnostics ALIAS slicesoft_diagnostics)
target_include_directories(slicesoft_diagnostics PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(slicesoft_diagnostics PRIVATE nlohmann_json::nlohmann_json)
slicesoft_sdk_target(slicesoft_diagnostics)

add_library(slicesoft_diagnostics_host STATIC
    src/diagnostics/host/LogSession.cpp
    src/diagnostics/host/ModuleLogBinding.cpp
    src/diagnostics/host/SessionRetention.cpp
    src/diagnostics/spdlog/FileLogSink.cpp)
add_library(SliceSoft::DiagnosticsHost ALIAS slicesoft_diagnostics_host)
target_link_libraries(slicesoft_diagnostics_host
    PUBLIC slicesoft_diagnostics PRIVATE spdlog::spdlog)
slicesoft_sdk_target(slicesoft_diagnostics_host)

if(SLICESOFT_DIAGNOSTICS_BUILD_CRASH)
    add_library(slicesoft_diagnostics_windows STATIC src/diagnostics/windows/CrashReporter.cpp)
    add_library(SliceSoft::CrashReporter ALIAS slicesoft_diagnostics_windows)
    target_include_directories(slicesoft_diagnostics_windows PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    slicesoft_sdk_target(slicesoft_diagnostics_windows)

    add_executable(slicer_crash_reporter
        apps/slicer_crash_reporter/main.cpp src/diagnostics/windows/CrashCapture.cpp)
    target_include_directories(slicer_crash_reporter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(slicer_crash_reporter PRIVATE dbghelp)
    slicesoft_sdk_target(slicer_crash_reporter)
endif()
