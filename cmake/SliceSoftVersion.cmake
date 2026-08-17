include_guard(GLOBAL)

function(_slicesoft_version_json_get output json)
    string(JSON value ERROR_VARIABLE error GET "${json}" ${ARGN})
    if(NOT error STREQUAL "NOTFOUND")
        string(JOIN "." field ${ARGN})
        message(FATAL_ERROR
            "version-manifest.json is missing or has an invalid '${field}' field: ${error}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(_slicesoft_validate_core_version label value)
    if(NOT value MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
        message(FATAL_ERROR "${label} must be a SemVer core version: '${value}'")
    endif()
endfunction()

function(_slicesoft_validate_prerelease label value)
    if(NOT value STREQUAL ""
       AND NOT value MATCHES "^[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*$")
        message(FATAL_ERROR "${label} is not a valid SemVer prerelease: '${value}'")
    endif()
    if(NOT value STREQUAL "")
        string(REPLACE "." ";" identifiers "${value}")
        foreach(identifier IN LISTS identifiers)
            if(identifier MATCHES "^[0-9]+$" AND identifier MATCHES "^0[0-9]+$")
                message(FATAL_ERROR
                    "${label} contains a numeric SemVer identifier with a leading zero: "
                    "'${identifier}'")
            endif()
        endforeach()
    endif()
endfunction()

function(_slicesoft_compose_implementation_version output core prerelease)
    if(prerelease STREQUAL "")
        set(value "${core}")
    else()
        set(value "${core}-${prerelease}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(slicesoft_load_version_manifest manifest_path)
    if(NOT EXISTS "${manifest_path}")
        message(FATAL_ERROR "SliceSoft version manifest was not found: ${manifest_path}")
    endif()
    file(READ "${manifest_path}" manifest)

    _slicesoft_version_json_get(schema_version "${manifest}" schemaVersion)
    _slicesoft_version_json_get(version_scheme "${manifest}" versionScheme)
    _slicesoft_version_json_get(release_policy "${manifest}" releasePolicy)
    _slicesoft_version_json_get(release_status "${manifest}" release status)
    _slicesoft_version_json_get(release_version "${manifest}" release version)
    _slicesoft_version_json_get(release_prerelease "${manifest}" release preRelease)
    _slicesoft_version_json_get(app_id "${manifest}" components application id)
    _slicesoft_version_json_get(app_name "${manifest}" components application name)
    _slicesoft_version_json_get(app_version "${manifest}" components application version)
    _slicesoft_version_json_get(app_prerelease "${manifest}" components application preRelease)
    _slicesoft_version_json_get(app_status "${manifest}" components application status)
    _slicesoft_version_json_get(slicer_id "${manifest}" components slicer id)
    _slicesoft_version_json_get(slicer_name "${manifest}" components slicer name)
    _slicesoft_version_json_get(slicer_version "${manifest}" components slicer version)
    _slicesoft_version_json_get(slicer_prerelease "${manifest}" components slicer preRelease)
    _slicesoft_version_json_get(slicer_contract "${manifest}" components slicer contractVersion)
    _slicesoft_version_json_get(slicer_status "${manifest}" components slicer status)
    string(JSON compatibility_count ERROR_VARIABLE compatibility_error
        LENGTH "${manifest}" compatibility contracts)
    if(NOT compatibility_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR
            "version-manifest.json has an invalid compatibility.contracts field: "
            "${compatibility_error}")
    endif()
    if(NOT compatibility_count EQUAL 3)
        message(FATAL_ERROR
            "version-manifest.json must declare exactly three frozen compatibility contracts")
    endif()
    _slicesoft_version_json_get(compatibility_spi "${manifest}" compatibility contracts 0)
    _slicesoft_version_json_get(compatibility_worker "${manifest}" compatibility contracts 1)
    _slicesoft_version_json_get(compatibility_package "${manifest}" compatibility contracts 2)

    if(NOT schema_version EQUAL 1)
        message(FATAL_ERROR "Unsupported SliceSoft version manifest schema: ${schema_version}")
    endif()
    if(NOT version_scheme STREQUAL "semver-2.0.0")
        message(FATAL_ERROR "Unsupported SliceSoft version scheme: ${version_scheme}")
    endif()
    if(NOT release_policy STREQUAL "lockstep")
        message(FATAL_ERROR "Unsupported SliceSoft release policy: ${release_policy}")
    endif()
    if(NOT release_status MATCHES "^(development|prerelease|stable|deprecated)$")
        message(FATAL_ERROR "Unsupported SliceSoft release status: ${release_status}")
    endif()
    if(NOT app_id STREQUAL "slicesoft-app" OR NOT slicer_id STREQUAL "slicer")
        message(FATAL_ERROR
            "SliceSoft version component IDs are frozen as slicesoft-app and slicer")
    endif()
    if(NOT app_name STREQUAL "SliceSoft"
       OR NOT slicer_name STREQUAL "SliceSoft Geometry Slicer")
        message(FATAL_ERROR "SliceSoft version component names have drifted")
    endif()
    if(NOT app_status MATCHES "^(available|unavailable|deprecated)$"
       OR NOT slicer_status MATCHES "^(available|unavailable|deprecated)$")
        message(FATAL_ERROR "SliceSoft version component status is invalid")
    endif()
    if(NOT slicer_contract STREQUAL "slicer-module.spi.v1")
        message(FATAL_ERROR "SliceSoft slicer contractVersion must remain slicer-module.spi.v1")
    endif()
    if(NOT compatibility_spi STREQUAL "slicer-module.spi.v1"
       OR NOT compatibility_worker STREQUAL "file_contract_v1"
       OR NOT compatibility_package STREQUAL "p0.rgbwsv.2")
        message(FATAL_ERROR "SliceSoft frozen compatibility contracts have drifted")
    endif()

    _slicesoft_validate_core_version("release.version" "${release_version}")
    _slicesoft_validate_core_version("components.application.version" "${app_version}")
    _slicesoft_validate_core_version("components.slicer.version" "${slicer_version}")
    _slicesoft_validate_prerelease("release.preRelease" "${release_prerelease}")
    _slicesoft_validate_prerelease("components.application.preRelease" "${app_prerelease}")
    _slicesoft_validate_prerelease("components.slicer.preRelease" "${slicer_prerelease}")
    if(release_status STREQUAL "stable" AND NOT release_prerelease STREQUAL "")
        message(FATAL_ERROR "A stable SliceSoft release cannot have a prerelease identifier")
    endif()
    if(release_status STREQUAL "prerelease" AND release_prerelease STREQUAL "")
        message(FATAL_ERROR "A SliceSoft prerelease must have a prerelease identifier")
    endif()

    if(NOT app_version STREQUAL release_version
       OR NOT slicer_version STREQUAL release_version
       OR NOT app_prerelease STREQUAL release_prerelease
       OR NOT slicer_prerelease STREQUAL release_prerelease)
        message(FATAL_ERROR
            "lockstep release requires application and slicer versions to match release")
    endif()

    _slicesoft_compose_implementation_version(
        app_implementation "${app_version}" "${app_prerelease}")
    _slicesoft_compose_implementation_version(
        slicer_implementation "${slicer_version}" "${slicer_prerelease}")

    set(vcpkg_manifest "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")
    if(EXISTS "${vcpkg_manifest}")
        file(READ "${vcpkg_manifest}" vcpkg_json)
        _slicesoft_version_json_get(
            vcpkg_version "${vcpkg_json}" version-string)
        if(NOT vcpkg_version STREQUAL app_implementation)
            message(FATAL_ERROR
                "vcpkg.json version-string must match the source version manifest: "
                "${vcpkg_version} != ${app_implementation}")
        endif()
    endif()

    set(SLICESOFT_VERSION_MANIFEST "${manifest_path}" PARENT_SCOPE)
    set(SLICESOFT_RELEASE_POLICY "${release_policy}" PARENT_SCOPE)
    set(SLICESOFT_RELEASE_STATUS "${release_status}" PARENT_SCOPE)
    set(SLICESOFT_APP_ID "${app_id}" PARENT_SCOPE)
    set(SLICESOFT_APP_NAME "${app_name}" PARENT_SCOPE)
    set(SLICESOFT_APP_VERSION_CORE "${app_version}" PARENT_SCOPE)
    set(SLICESOFT_APP_PRERELEASE "${app_prerelease}" PARENT_SCOPE)
    set(SLICESOFT_APP_IMPLEMENTATION_VERSION "${app_implementation}" PARENT_SCOPE)
    set(SLICESOFT_SLICER_ID "${slicer_id}" PARENT_SCOPE)
    set(SLICESOFT_SLICER_NAME "${slicer_name}" PARENT_SCOPE)
    set(SLICESOFT_SLICER_VERSION_CORE "${slicer_version}" PARENT_SCOPE)
    set(SLICESOFT_SLICER_PRERELEASE "${slicer_prerelease}" PARENT_SCOPE)
    set(SLICESOFT_SLICER_IMPLEMENTATION_VERSION "${slicer_implementation}" PARENT_SCOPE)
endfunction()

function(slicesoft_initialize_version_build)
    set(generated_root "${CMAKE_BINARY_DIR}/generated/slicesoft_version")
    if(DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET STREQUAL "")
        set(target_triplet "${VCPKG_TARGET_TRIPLET}")
    else()
        set(target_triplet "unknown-triplet")
    endif()
    if(USE_OPENVDB)
        set(openvdb_enabled true)
    else()
        set(openvdb_enabled false)
    endif()

    add_custom_target(slicesoft_version_snapshot ALL
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DOUTPUT_DIR=${generated_root}/$<CONFIG>"
            "-DCONFIG=$<CONFIG>"
            "-DAPP_ID=${SLICESOFT_APP_ID}"
            "-DAPP_NAME=${SLICESOFT_APP_NAME}"
            "-DAPP_VERSION=${SLICESOFT_APP_IMPLEMENTATION_VERSION}"
            "-DSLICER_ID=${SLICESOFT_SLICER_ID}"
            "-DSLICER_NAME=${SLICESOFT_SLICER_NAME}"
            "-DSLICER_VERSION=${SLICESOFT_SLICER_IMPLEMENTATION_VERSION}"
            "-DRELEASE_POLICY=${SLICESOFT_RELEASE_POLICY}"
            "-DRELEASE_STATUS=${SLICESOFT_RELEASE_STATUS}"
            "-DTARGET_TRIPLET=${target_triplet}"
            "-DTIFF_BACKEND=${SLICESOFT_TIFF_BACKEND}"
            "-DOPENVDB_ENABLED=${openvdb_enabled}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GenerateSliceSoftBuildManifest.cmake"
        BYPRODUCTS
            "${generated_root}/$<CONFIG>/SliceSoftBuildVersion.h"
            "${generated_root}/$<CONFIG>/slicesoft_build_manifest.json"
        COMMENT "Refreshing SliceSoft source and build version snapshot"
        VERBATIM
    )
    set_property(TARGET slicesoft_version_snapshot PROPERTY FOLDER "Build")
    set(SLICESOFT_VERSION_GENERATED_ROOT "${generated_root}" CACHE INTERNAL "")
endfunction()

function(slicesoft_use_version target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot apply SliceSoft version to missing target: ${target}")
    endif()
    add_dependencies("${target}" slicesoft_version_snapshot)
    target_include_directories("${target}" PRIVATE
        "${SLICESOFT_VERSION_GENERATED_ROOT}/$<CONFIG>")
endfunction()

function(slicesoft_add_windows_version_resource target component file_description original_filename)
    slicesoft_use_version("${target}")
    if(NOT WIN32)
        return()
    endif()

    if(component STREQUAL "application")
        set(resource_version "${SLICESOFT_APP_IMPLEMENTATION_VERSION}")
        set(product_name "${SLICESOFT_APP_NAME}")
        set(full_build_version_macro "SLICESOFT_APP_FULL_BUILD_VERSION")
    elseif(component STREQUAL "slicer")
        set(resource_version "${SLICESOFT_SLICER_IMPLEMENTATION_VERSION}")
        set(product_name "${SLICESOFT_SLICER_NAME}")
        set(full_build_version_macro "SLICESOFT_SLICER_FULL_BUILD_VERSION")
    else()
        message(FATAL_ERROR "Unknown SliceSoft version component: ${component}")
    endif()

    string(REPLACE "." ";" version_parts "${SLICESOFT_APP_VERSION_CORE}")
    list(GET version_parts 0 version_major)
    list(GET version_parts 1 version_minor)
    list(GET version_parts 2 version_patch)
    set(SLICESOFT_RC_VERSION_MAJOR "${version_major}")
    set(SLICESOFT_RC_VERSION_MINOR "${version_minor}")
    set(SLICESOFT_RC_VERSION_PATCH "${version_patch}")
    set(SLICESOFT_RC_IMPLEMENTATION_VERSION "${resource_version}")
    set(SLICESOFT_RC_PRODUCT_NAME "${product_name}")
    set(SLICESOFT_RC_FULL_BUILD_VERSION_MACRO "${full_build_version_macro}")
    set(SLICESOFT_RC_FILE_DESCRIPTION "${file_description}")
    set(SLICESOFT_RC_INTERNAL_NAME "${target}")
    set(SLICESOFT_RC_ORIGINAL_FILENAME "${original_filename}")
    if(SLICESOFT_APP_PRERELEASE STREQUAL "")
        set(SLICESOFT_RC_PRERELEASE_FLAG "0x0L")
    else()
        set(SLICESOFT_RC_PRERELEASE_FLAG "VS_FF_PRERELEASE")
    endif()
    if(original_filename MATCHES "\\.dll$")
        set(SLICESOFT_RC_FILE_TYPE "VFT_DLL")
    else()
        set(SLICESOFT_RC_FILE_TYPE "VFT_APP")
    endif()
    set(resource_file
        "${CMAKE_BINARY_DIR}/generated/slicesoft_version/resources/${target}.rc")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/SliceSoftVersion.rc.in"
        "${resource_file}"
        @ONLY)
    target_sources("${target}" PRIVATE "${resource_file}")
endfunction()

function(slicesoft_copy_version_manifests target)
    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SLICESOFT_VERSION_MANIFEST}"
            "$<TARGET_FILE_DIR:${target}>/version-manifest.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SLICESOFT_VERSION_GENERATED_ROOT}/$<CONFIG>/slicesoft_build_manifest.json"
            "$<TARGET_FILE_DIR:${target}>/slicesoft_build_manifest.json"
        VERBATIM)
endfunction()
