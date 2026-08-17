cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS
        SOURCE_DIR OUTPUT_DIR CONFIG APP_ID APP_NAME APP_VERSION
        SLICER_ID SLICER_NAME SLICER_VERSION RELEASE_POLICY RELEASE_STATUS
        TARGET_TRIPLET TIFF_BACKEND OPENVDB_ENABLED)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing SliceSoft build-version input: ${required}")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported SliceSoft build configuration: ${CONFIG}")
endif()

function(slicesoft_token output value)
    string(TOLOWER "${value}" token)
    string(REGEX REPLACE "[^0-9a-z-]+" "-" token "${token}")
    string(REGEX REPLACE "^-+|-+$" "" token "${token}")
    if(token STREQUAL "")
        set(token "unknown")
    endif()
    set(${output} "${token}" PARENT_SCOPE)
endfunction()

set(revision "unknown")
set(source_state "unknown")
find_program(git_executable NAMES git)
if(git_executable)
    execute_process(
        COMMAND "${git_executable}" -C "${SOURCE_DIR}" rev-parse --short=12 HEAD
        RESULT_VARIABLE revision_result
        OUTPUT_VARIABLE revision_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(LENGTH "${revision_output}" revision_length)
    if(revision_result EQUAL 0
       AND revision_length EQUAL 12
       AND revision_output MATCHES "^[0-9a-fA-F]+$")
        string(TOLOWER "${revision_output}" revision)
        execute_process(
            COMMAND "${git_executable}" -C "${SOURCE_DIR}" status --porcelain --untracked-files=normal
            RESULT_VARIABLE status_result
            OUTPUT_VARIABLE status_output
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(status_result EQUAL 0)
            if(status_output STREQUAL "")
                set(source_state "clean")
            else()
                set(source_state "dirty")
            endif()
        endif()
    endif()
endif()

if(CONFIG STREQUAL "Debug")
    set(runtime "MSVC-x64-MDd")
else()
    set(runtime "MSVC-x64-MD")
endif()
if(OPENVDB_ENABLED)
    set(openvdb_state "on")
    set(openvdb_json true)
else()
    set(openvdb_state "off")
    set(openvdb_json false)
endif()

slicesoft_token(config_token "${CONFIG}")
slicesoft_token(runtime_token "${runtime}")
slicesoft_token(triplet_token "${TARGET_TRIPLET}")
slicesoft_token(tiff_token "${TIFF_BACKEND}")
set(build_metadata
    "${revision}.${source_state}.${config_token}.${runtime_token}.${triplet_token}.tiff-${tiff_token}.openvdb-${openvdb_state}")
set(app_full_version "${APP_VERSION}+${build_metadata}")
set(slicer_full_version "${SLICER_VERSION}+${build_metadata}")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(header_content "#pragma once\n\n")
string(APPEND header_content "#define SLICESOFT_APP_ID \"${APP_ID}\"\n")
string(APPEND header_content "#define SLICESOFT_APP_NAME \"${APP_NAME}\"\n")
string(APPEND header_content "#define SLICESOFT_APP_IMPLEMENTATION_VERSION \"${APP_VERSION}\"\n")
string(APPEND header_content "#define SLICESOFT_APP_FULL_BUILD_VERSION \"${app_full_version}\"\n")
string(APPEND header_content "#define SLICESOFT_SLICER_ID \"${SLICER_ID}\"\n")
string(APPEND header_content "#define SLICESOFT_SLICER_NAME \"${SLICER_NAME}\"\n")
string(APPEND header_content "#define SLICESOFT_SLICER_IMPLEMENTATION_VERSION \"${SLICER_VERSION}\"\n")
string(APPEND header_content "#define SLICESOFT_SLICER_FULL_BUILD_VERSION \"${slicer_full_version}\"\n")
string(APPEND header_content "#define SLICESOFT_SOURCE_REVISION \"${revision}\"\n")
string(APPEND header_content "#define SLICESOFT_SOURCE_STATE \"${source_state}\"\n")
string(APPEND header_content "#define SLICESOFT_BUILD_CONFIG \"${CONFIG}\"\n")
string(APPEND header_content "#define SLICESOFT_BUILD_RUNTIME \"${runtime}\"\n")
string(APPEND header_content "#define SLICESOFT_BUILD_TRIPLET \"${TARGET_TRIPLET}\"\n")
string(APPEND header_content "#define SLICESOFT_TIFF_BACKEND_VERSION_VARIANT \"${TIFF_BACKEND}\"\n")
string(APPEND header_content "#define SLICESOFT_OPENVDB_VERSION_VARIANT \"${openvdb_state}\"\n")

set(header_path "${OUTPUT_DIR}/SliceSoftBuildVersion.h")
set(header_temp "${header_path}.tmp")
file(WRITE "${header_temp}" "${header_content}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${header_temp}" "${header_path}")
file(REMOVE "${header_temp}")

set(manifest_content "{\n")
string(APPEND manifest_content "  \"schema\": \"slicesoft.build.1\",\n")
string(APPEND manifest_content "  \"releasePolicy\": \"${RELEASE_POLICY}\",\n")
string(APPEND manifest_content "  \"releaseStatus\": \"${RELEASE_STATUS}\",\n")
string(APPEND manifest_content "  \"source\": {\"revision\": \"${revision}\", \"state\": \"${source_state}\"},\n")
string(APPEND manifest_content "  \"build\": {\"config\": \"${CONFIG}\", \"runtime\": \"${runtime}\", \"triplet\": \"${TARGET_TRIPLET}\", \"tiffBackend\": \"${TIFF_BACKEND}\", \"openVdb\": ${openvdb_json}},\n")
string(APPEND manifest_content "  \"components\": {\n")
string(APPEND manifest_content "    \"application\": {\"id\": \"${APP_ID}\", \"name\": \"${APP_NAME}\", \"version\": \"${APP_VERSION}\", \"fullBuildVersion\": \"${app_full_version}\"},\n")
string(APPEND manifest_content "    \"slicer\": {\"id\": \"${SLICER_ID}\", \"name\": \"${SLICER_NAME}\", \"version\": \"${SLICER_VERSION}\", \"fullBuildVersion\": \"${slicer_full_version}\"}\n")
string(APPEND manifest_content "  },\n")
string(APPEND manifest_content "  \"compatibility\": {\"contracts\": [\"slicer-module.spi.v1\", \"file_contract_v1\", \"p0.rgbwsv.2\"]}\n")
string(APPEND manifest_content "}\n")

set(manifest_path "${OUTPUT_DIR}/slicesoft_build_manifest.json")
set(manifest_temp "${manifest_path}.tmp")
file(WRITE "${manifest_temp}" "${manifest_content}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${manifest_temp}" "${manifest_path}")
file(REMOVE "${manifest_temp}")
