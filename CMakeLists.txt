cmake_minimum_required(VERSION 3.24)

project(SliceSoftDemo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SLICER_DEBUG_UI "Build Qt slicer debug UI" ON)
option(ENABLE_GEOMETRY_KERNEL_DEMO "Build experimental geometry kernel demo" ON)
option(USE_OPENVDB "Enable optional OpenVDB adapter" OFF)

add_library(slicer_core
    src/third_party/miniz/miniz.c
    src/third_party/miniz/miniz_tdef.c
    src/third_party/miniz/miniz_tinfl.c
    src/third_party/miniz/miniz_zip.c
    src/slicer_core/config/ConfigMigration.cpp
    src/slicer_core/config/ConfigMigration.h
    src/slicer_core/config/ConfigSchema.cpp
    src/slicer_core/config/ConfigSchema.h
    src/slicer_core/config/NormalizedConfig.cpp
    src/slicer_core/config/NormalizedConfig.h
    src/slicer_core/config.cpp
    src/slicer_core/config.h
    src/slicer_core/diagnostics/Diagnostics.cpp
    src/slicer_core/diagnostics/Diagnostics.h
    src/slicer_core/diagnostics/MaterialClosureCandidateDetector.cpp
    src/slicer_core/diagnostics/MaterialClosureCandidateDetector.h
    src/slicer_core/diagnostics/MaterialClosureSemanticDetector.cpp
    src/slicer_core/diagnostics/MaterialClosureSemanticDetector.h
    src/slicer_core/diagnostics/MeshRepairReport.cpp
    src/slicer_core/diagnostics/MeshRepairReport.h
    src/slicer_core/diagnostics/TextureFillPartitionClosureAdapter.cpp
    src/slicer_core/diagnostics/TextureFillPartitionClosureAdapter.h
    src/slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.cpp
    src/slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h
    src/slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.cpp
    src/slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h
    src/slicer_core/diagnostics/ProductionAdmissionPolicy.cpp
    src/slicer_core/diagnostics/ProductionAdmissionPolicy.h
    src/slicer_core/diagnostics/ValidationIssue.cpp
    src/slicer_core/diagnostics/ValidationIssue.h
    src/slicer_core/geometry/DistanceField2D.cpp
    src/slicer_core/geometry/DistanceField2D.h
    src/slicer_core/geometry/GeometryKernelReport.cpp
    src/slicer_core/geometry/GeometryKernelReport.h
    src/slicer_core/geometry/GeometryKernelService.h
    src/slicer_core/geometry/GeometryKernelTypes.h
    src/slicer_core/geometry/OpenVdbAdapter.cpp
    src/slicer_core/geometry/OpenVdbAdapter.h
    src/slicer_core/geometry/OpenVdbGeometryKernelService.cpp
    src/slicer_core/geometry/OpenVdbGeometryKernelService.h
    src/slicer_core/geometry/OpenVdbLevelSetBuilder.cpp
    src/slicer_core/geometry/OpenVdbLevelSetBuilder.h
    src/slicer_core/geometry/OpenVdbSdfUtilityReport.cpp
    src/slicer_core/geometry/OpenVdbSdfUtilityReport.h
    src/slicer_core/geometry/OpenVdbSurfaceShell.cpp
    src/slicer_core/geometry/OpenVdbSurfaceShell.h
    src/slicer_core/geometry/MeshRobustnessDiagnostics.cpp
    src/slicer_core/geometry/MeshRobustnessDiagnostics.h
    src/slicer_core/geometry/MeshScaleTolerance.cpp
    src/slicer_core/geometry/MeshScaleTolerance.h
    src/slicer_core/geometry/MeshTopologyDiagnostics.cpp
    src/slicer_core/geometry/MeshTopologyDiagnostics.h
    src/slicer_core/geometry/repair/MeshRepairHash.cpp
    src/slicer_core/geometry/repair/MeshRepairHash.h
    src/slicer_core/geometry/repair/MeshRepairEligibilityPolicy.cpp
    src/slicer_core/geometry/repair/MeshRepairEligibilityPolicy.h
    src/slicer_core/geometry/repair/MeshRepairEvidenceValidator.cpp
    src/slicer_core/geometry/repair/MeshRepairEvidenceValidator.h
    src/slicer_core/geometry/repair/MeshCompleteSelfIntersectionAnalyzer.cpp
    src/slicer_core/geometry/repair/MeshCompleteSelfIntersectionAnalyzer.h
    src/slicer_core/geometry/repair/MeshNonManifoldPatternClassifier.cpp
    src/slicer_core/geometry/repair/MeshNonManifoldPatternClassifier.h
    src/slicer_core/geometry/repair/MeshRepairPreflight.cpp
    src/slicer_core/geometry/repair/MeshRepairPreflight.h
    src/slicer_core/geometry/repair/MeshRepairService.cpp
    src/slicer_core/geometry/repair/MeshRepairService.h
    src/slicer_core/geometry/repair/MeshRepairBoundaryOperations.cpp
    src/slicer_core/geometry/repair/MeshRepairBoundaryOperations.h
    src/slicer_core/geometry/repair/MeshRepairTopologyOperations.cpp
    src/slicer_core/geometry/repair/MeshRepairTopologyOperations.h
    src/slicer_core/geometry/repair/MeshRepairTypes.cpp
    src/slicer_core/geometry/repair/MeshRepairTypes.h
    src/slicer_core/geometry/NearestTriangleQuery.cpp
    src/slicer_core/geometry/NearestTriangleQuery.h
    src/slicer_core/geometry/PointInClosedMeshQuery.cpp
    src/slicer_core/geometry/PointInClosedMeshQuery.h
    src/slicer_core/geometry/SceneModelTriangleMeshAdapter.cpp
    src/slicer_core/geometry/SceneModelTriangleMeshAdapter.h
    src/slicer_core/geometry/ShellMask.cpp
    src/slicer_core/geometry/ShellMask.h
    src/slicer_core/geometry/TriangleMeshData.cpp
    src/slicer_core/geometry/TriangleMeshData.h
    src/slicer_core/geometry/TriangleIntersectionQuery.cpp
    src/slicer_core/geometry/TriangleIntersectionQuery.h
    src/slicer_core/importers/mtl/MtlImporter.cpp
    src/slicer_core/importers/mtl/MtlImporter.h
    src/slicer_core/importers/obj/ObjImporter.cpp
    src/slicer_core/importers/obj/ObjImporter.h
    src/slicer_core/importers/three_mf/ThreeMfImporter.cpp
    src/slicer_core/importers/three_mf/ThreeMfImporter.h
    src/slicer_core/json_value.cpp
    src/slicer_core/json_value.h
    src/slicer_core/material/MaterialChannelComposer.cpp
    src/slicer_core/material/MaterialChannelComposer.h
    src/slicer_core/material/MaterialClosureRepair.cpp
    src/slicer_core/material/MaterialClosureRepair.h
    src/slicer_core/materials/material_policy/MaterialPolicy.cpp
    src/slicer_core/materials/material_policy/MaterialPolicy.h
    src/slicer_core/materials/process_profile/MaterialProcessProfile.cpp
    src/slicer_core/materials/process_profile/MaterialProcessProfile.h
    src/slicer_core/materials/role_mapping/MaterialRoleMapping.cpp
    src/slicer_core/materials/role_mapping/MaterialRoleMapping.h
    src/slicer_core/materials/texture_application/TextureApplicationPolicy.cpp
    src/slicer_core/materials/texture_application/TextureApplicationPolicy.h
    src/slicer_core/materials/texture_application/GlobalTextureFillPartitionService.cpp
    src/slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h
    src/slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.cpp
    src/slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.h
    src/slicer_core/materials/texture_application/OpenVdbTextureFillConformanceBackend.cpp
    src/slicer_core/materials/texture_application/OpenVdbTextureFillConformanceBackend.h
    src/slicer_core/materials/texture_application/TextureFillPartitionAdmission.cpp
    src/slicer_core/materials/texture_application/TextureFillPartitionAdmission.h
    src/slicer_core/materials/texture_application/TextureFillPartitionTypes.cpp
    src/slicer_core/materials/texture_application/TextureFillPartitionTypes.h
    src/slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.cpp
    src/slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h
    src/slicer_core/materials/texture_application/SurfaceShellTexturePrototype.cpp
    src/slicer_core/materials/texture_application/SurfaceShellTexturePrototype.h
    src/slicer_core/materials/texture_application/SurfaceShellTextureReport.cpp
    src/slicer_core/materials/texture_application/SurfaceShellTextureReport.h
    src/slicer_core/materials/texture_application/SurfaceShellTextureService.cpp
    src/slicer_core/materials/texture_application/SurfaceShellTextureService.h
    src/slicer_core/materials/texture_application/SurfaceAttributeMap.cpp
    src/slicer_core/materials/texture_application/SurfaceAttributeMap.h
    src/slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.cpp
    src/slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h
    src/slicer_core/materials/texture_application/SurfaceShellBenchmarkReport.cpp
    src/slicer_core/materials/texture_application/SurfaceShellBenchmarkReport.h
    src/slicer_core/materials/texture_application/SurfaceShellRealModelReport.cpp
    src/slicer_core/materials/texture_application/SurfaceShellRealModelReport.h
    src/slicer_core/materials/texture_application/SurfaceTextureTransfer.cpp
    src/slicer_core/materials/texture_application/SurfaceTextureTransfer.h
    src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.cpp
    src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.h
    src/slicer_core/model.cpp
    src/slicer_core/model.h
    src/slicer_core/output/rgbwsv/RgbwsvPackage.cpp
    src/slicer_core/output/rgbwsv/RgbwsvPackage.h
    src/slicer_core/pipeline/PipelineContext.cpp
    src/slicer_core/pipeline/PipelineContext.h
    src/slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.cpp
    src/slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h
    src/slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.cpp
    src/slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.h
    src/slicer_core/pipeline/OpenVdbCandidatePipeline.cpp
    src/slicer_core/pipeline/OpenVdbCandidatePipeline.h
    src/slicer_core/pipeline/PipelineStepResult.cpp
    src/slicer_core/pipeline/PipelineStepResult.h
    src/slicer_core/pipeline/SlicePipeline.cpp
    src/slicer_core/pipeline/SlicePipeline.h
    src/slicer_core/raster/RasterBoundary.cpp
    src/slicer_core/raster/RasterBoundary.h
    src/slicer_core/raster/TextureFillPartitionRasterMapper.cpp
    src/slicer_core/raster/TextureFillPartitionRasterMapper.h
    src/slicer_core/reports/ReportBase.cpp
    src/slicer_core/reports/ReportBase.h
    src/slicer_core/reports/MaterialClosureReport.cpp
    src/slicer_core/reports/MaterialClosureReport.h
    src/slicer_core/reports/TextureFillPartitionReport.cpp
    src/slicer_core/reports/TextureFillPartitionReport.h
    src/slicer_core/reports/ReportSchema.cpp
    src/slicer_core/reports/ReportSchema.h
    src/slicer_core/reports/ReportSchemaValidator.cpp
    src/slicer_core/reports/ReportSchemaValidator.h
    src/slicer_core/reports/ReportWriter.cpp
    src/slicer_core/reports/ReportWriter.h
    src/slicer_core/rip_reader.cpp
    src/slicer_core/rip_reader.h
    src/slicer_core/scene/SceneModel.cpp
    src/slicer_core/scene/SceneModel.h
    src/slicer_core/SliceRunTelemetry.h
    src/slicer_core/slicer.cpp
    src/slicer_core/slicer.h
    src/slicer_core/support/SupportComponentAnalysis.cpp
    src/slicer_core/support/SupportComponentAnalysis.h
    src/slicer_core/support/SupportPolicy.cpp
    src/slicer_core/support/SupportPolicy.h
    src/slicer_core/support/SupportShapeOptimizer.cpp
    src/slicer_core/support/SupportShapeOptimizer.h
    src/slicer_core/support/SupportShapePipeline.cpp
    src/slicer_core/support/SupportShapePipeline.h
    src/slicer_core/support/SupportShapePolicy.cpp
    src/slicer_core/support/SupportShapePolicy.h
    src/slicer_core/support/SupportShapeReport.cpp
    src/slicer_core/support/SupportShapeReport.h
    src/slicer_core/system/ProcessMemoryStats.cpp
    src/slicer_core/system/ProcessMemoryStats.h
    src/slicer_core/texture_image.cpp
    src/slicer_core/texture_image.h
    src/slicer_core/tiff_io.cpp
    src/slicer_core/tiff_io.h
)

target_include_directories(slicer_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniz
)

if(MSVC)
    target_compile_options(slicer_core PRIVATE /bigobj)
endif()

if(WIN32)
    target_link_libraries(slicer_core PRIVATE windowscodecs ole32 psapi)
endif()

if(USE_OPENVDB)
    find_package(OpenVDB CONFIG QUIET)
    if(NOT OpenVDB_FOUND)
        message(FATAL_ERROR
            "USE_OPENVDB=ON but OpenVDB package config was not found.\n"
            "Use vcpkg manifest mode, for example:\n"
            "  powershell -ExecutionPolicy Bypass -File scripts/configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows\n"
            "The helper script reads VCPKG_ROOT by default. For a custom path, pass:\n"
            "  -VcpkgRoot \"<vcpkg-root>\"\n"
            "Or configure manually with:\n"
            "  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake\n"
            "  -DVCPKG_TARGET_TRIPLET=x64-windows\n"
            "Alternatively set -DOpenVDB_DIR=<directory-containing-OpenVDBConfig.cmake>."
        )
    endif()
    target_compile_definitions(slicer_core PRIVATE SLICER_CORE_USE_OPENVDB=1)
    if(TARGET OpenVDB::openvdb)
        target_link_libraries(slicer_core PRIVATE OpenVDB::openvdb)
    elseif(TARGET OpenVDB::OpenVDB)
        target_link_libraries(slicer_core PRIVATE OpenVDB::OpenVDB)
    else()
        message(FATAL_ERROR "OpenVDB package found but no supported imported target is available.")
    endif()
endif()

add_executable(slicer_cli
    apps/slicer_cli/main.cpp
)

target_link_libraries(slicer_cli PRIVATE slicer_core)

add_executable(texture_fill_partition_release_benchmark
    apps/texture_fill_partition_release_benchmark/main.cpp
)

target_link_libraries(texture_fill_partition_release_benchmark PRIVATE slicer_core)

add_executable(mesh_repair_preflight
    apps/mesh_repair_preflight/main.cpp
)

target_link_libraries(mesh_repair_preflight PRIVATE slicer_core)

add_executable(rip_reader_test
    apps/rip_reader_test/main.cpp
)

target_link_libraries(rip_reader_test PRIVATE slicer_core)

add_executable(support_shape_unit_tests
    tests/unit/support_shape/main.cpp
)

target_link_libraries(support_shape_unit_tests PRIVATE slicer_core)

add_executable(surface_shell_texture_unit_tests
    tests/unit/surface_shell_texture/main.cpp
)

target_link_libraries(surface_shell_texture_unit_tests PRIVATE slicer_core)

add_executable(surface_shell_real_model_unit_tests
    tests/unit/surface_shell_real_model/main.cpp
)

target_link_libraries(surface_shell_real_model_unit_tests PRIVATE slicer_core)

add_executable(surface_shell_robustness_unit_tests
    tests/unit/surface_shell_robustness/main.cpp
)

target_link_libraries(surface_shell_robustness_unit_tests PRIVATE slicer_core)

add_executable(production_admission_policy_unit_tests
    tests/unit/production_admission_policy/main.cpp
)

target_link_libraries(production_admission_policy_unit_tests PRIVATE slicer_core)

add_executable(experimental_config_unit_tests
    tests/unit/experimental_config/main.cpp
)

target_link_libraries(experimental_config_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_service_unit_tests
    tests/unit/texture_fill_partition_service/main.cpp
)

target_link_libraries(texture_fill_partition_service_unit_tests PRIVATE slicer_core)

add_executable(legacy_cpu_global_distance_unit_tests
    tests/unit/legacy_cpu_global_distance/main.cpp
)

target_link_libraries(legacy_cpu_global_distance_unit_tests PRIVATE slicer_core)

add_executable(openvdb_texture_fill_conformance_unit_tests
    tests/unit/openvdb_texture_fill_conformance/main.cpp
)

target_link_libraries(openvdb_texture_fill_conformance_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_width_sweep_unit_tests
    tests/unit/texture_fill_partition_width_sweep/main.cpp
)

target_link_libraries(texture_fill_partition_width_sweep_unit_tests PRIVATE slicer_core)
target_compile_definitions(texture_fill_partition_width_sweep_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(texture_fill_partition_report_unit_tests
    tests/unit/texture_fill_partition_report/main.cpp
)

target_link_libraries(texture_fill_partition_report_unit_tests PRIVATE slicer_core)
target_compile_definitions(texture_fill_partition_report_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(texture_fill_partition_texture_transfer_unit_tests
    tests/unit/texture_fill_partition_texture_transfer/main.cpp
)

target_link_libraries(texture_fill_partition_texture_transfer_unit_tests PRIVATE slicer_core)
target_compile_definitions(texture_fill_partition_texture_transfer_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(texture_fill_partition_diagnostic_composer_unit_tests
    tests/unit/texture_fill_partition_diagnostic_composer/main.cpp
)

target_link_libraries(texture_fill_partition_diagnostic_composer_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_closure_adapter_unit_tests
    tests/unit/texture_fill_partition_closure_adapter/main.cpp
)

target_link_libraries(texture_fill_partition_closure_adapter_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_raster_mapper_unit_tests
    tests/unit/texture_fill_partition_raster_mapper/main.cpp
)

target_link_libraries(texture_fill_partition_raster_mapper_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_full_closure_adapter_unit_tests
    tests/unit/texture_fill_partition_full_closure_adapter/main.cpp
)

target_link_libraries(texture_fill_partition_full_closure_adapter_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_release_benchmark_unit_tests
    tests/unit/texture_fill_partition_release_benchmark/main.cpp
)

target_link_libraries(texture_fill_partition_release_benchmark_unit_tests PRIVATE slicer_core)

add_executable(geometry_kernel_service_unit_tests
    tests/unit/geometry_kernel_service/main.cpp
)

target_link_libraries(geometry_kernel_service_unit_tests PRIVATE slicer_core)

add_executable(openvdb_sdf_utility_report_unit_tests
    tests/unit/openvdb_sdf_utility_report/main.cpp
)

target_link_libraries(openvdb_sdf_utility_report_unit_tests PRIVATE slicer_core)

add_executable(material_closure_report_unit_tests
    tests/unit/material_closure_report/main.cpp
)

target_link_libraries(material_closure_report_unit_tests PRIVATE slicer_core)

add_executable(material_closure_candidate_detector_unit_tests
    tests/unit/material_closure_candidate_detector/main.cpp
)

target_link_libraries(material_closure_candidate_detector_unit_tests PRIVATE slicer_core)

add_executable(material_closure_semantic_detector_unit_tests
    tests/unit/material_closure_semantic_detector/main.cpp
)

target_link_libraries(material_closure_semantic_detector_unit_tests PRIVATE slicer_core)

add_executable(surface_shell_texture_service_unit_tests
    tests/unit/surface_shell_texture_service/main.cpp
)

target_link_libraries(surface_shell_texture_service_unit_tests PRIVATE slicer_core)

add_executable(material_channel_composer_unit_tests
    tests/unit/material_channel_composer/main.cpp
)

target_link_libraries(material_channel_composer_unit_tests PRIVATE slicer_core)

add_executable(mesh_repair_contract_unit_tests
    tests/unit/mesh_repair_contract/main.cpp
)

target_link_libraries(mesh_repair_contract_unit_tests PRIVATE slicer_core)
target_compile_definitions(mesh_repair_contract_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(mesh_repair_eligibility_unit_tests
    tests/unit/mesh_repair_eligibility/main.cpp
)

target_link_libraries(mesh_repair_eligibility_unit_tests PRIVATE slicer_core)

add_executable(mesh_repair_fixture_golden_unit_tests
    tests/unit/mesh_repair_fixture_golden/main.cpp
)

target_link_libraries(mesh_repair_fixture_golden_unit_tests PRIVATE slicer_core)
target_compile_definitions(mesh_repair_fixture_golden_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(mesh_repair_preflight_unit_tests
    tests/unit/mesh_repair_preflight/main.cpp
)

target_link_libraries(mesh_repair_preflight_unit_tests PRIVATE slicer_core)

add_executable(mesh_repair_cleanup_unit_tests
    tests/unit/mesh_repair_cleanup/main.cpp
)

target_link_libraries(mesh_repair_cleanup_unit_tests PRIVATE slicer_core)

add_executable(mesh_repair_evidence_validator_unit_tests
    tests/unit/mesh_repair_evidence_validator/main.cpp
)

target_link_libraries(mesh_repair_evidence_validator_unit_tests PRIVATE slicer_core)

add_executable(mesh_non_manifold_pattern_classifier_unit_tests
    tests/unit/mesh_non_manifold_pattern_classifier/main.cpp
)

target_link_libraries(mesh_non_manifold_pattern_classifier_unit_tests PRIVATE slicer_core)

add_executable(mesh_complete_self_intersection_analyzer_unit_tests
    tests/unit/mesh_complete_self_intersection_analyzer/main.cpp
)

target_link_libraries(mesh_complete_self_intersection_analyzer_unit_tests PRIVATE slicer_core)

enable_testing()
add_test(NAME production_admission_policy_unit_tests COMMAND production_admission_policy_unit_tests)
add_test(NAME experimental_config_unit_tests COMMAND experimental_config_unit_tests)
add_test(NAME texture_fill_partition_service_unit_tests COMMAND texture_fill_partition_service_unit_tests)
add_test(NAME legacy_cpu_global_distance_unit_tests COMMAND legacy_cpu_global_distance_unit_tests)
add_test(NAME openvdb_texture_fill_conformance_unit_tests COMMAND openvdb_texture_fill_conformance_unit_tests)
add_test(NAME texture_fill_partition_width_sweep_unit_tests COMMAND texture_fill_partition_width_sweep_unit_tests)
add_test(NAME texture_fill_partition_report_unit_tests COMMAND texture_fill_partition_report_unit_tests)
add_test(NAME texture_fill_partition_texture_transfer_unit_tests COMMAND texture_fill_partition_texture_transfer_unit_tests)
add_test(NAME texture_fill_partition_diagnostic_composer_unit_tests COMMAND texture_fill_partition_diagnostic_composer_unit_tests)
add_test(NAME texture_fill_partition_closure_adapter_unit_tests COMMAND texture_fill_partition_closure_adapter_unit_tests)
add_test(NAME texture_fill_partition_raster_mapper_unit_tests COMMAND texture_fill_partition_raster_mapper_unit_tests)
add_test(NAME texture_fill_partition_full_closure_adapter_unit_tests COMMAND texture_fill_partition_full_closure_adapter_unit_tests)
add_test(NAME texture_fill_partition_release_benchmark_unit_tests COMMAND texture_fill_partition_release_benchmark_unit_tests)
add_test(
    NAME texture_fill_partition_release_benchmark_cli_test
    COMMAND texture_fill_partition_release_benchmark
        --config samples/configs/texture_fill_partition/global_surface_shell_unavailable.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_cli_test.json
        --case-name generated_sample
        --voxel-mm 0.05
        --width-mm 0.10
)
set_tests_properties(texture_fill_partition_release_benchmark_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME mesh_repair_preflight_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r1_preflight_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
)
set_tests_properties(mesh_repair_preflight_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME mesh_repair_cleanup_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r2_cleanup_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
        --execute-cleanup
)
set_tests_properties(mesh_repair_cleanup_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME mesh_repair_r2_02_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r2_02_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
        --execute-r2-02
        --weld-tolerance-mm 0.0001
)
set_tests_properties(mesh_repair_r2_02_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME mesh_repair_r2_03_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r2_03_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
        --execute-r2-03
        --weld-tolerance-mm 0.0001
        --max-boundary-loop-edges 16
        --max-boundary-loop-diameter-mm 2.0
        --max-boundary-loop-perimeter-mm 8.0
        --max-boundary-planarity-error-mm 0.01
        --max-hole-area-mm2 4.0
        --max-affected-face-ratio 0.5
)
set_tests_properties(mesh_repair_r2_03_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME mesh_repair_r2_04_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r2_04_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
        --execute-r2-04
        --weld-tolerance-mm 0.0001
        --max-boundary-loop-edges 16
        --max-boundary-loop-diameter-mm 2.0
        --max-boundary-loop-perimeter-mm 8.0
        --max-boundary-planarity-error-mm 0.01
        --max-hole-area-mm2 4.0
        --max-affected-face-ratio 0.5
)
set_tests_properties(mesh_repair_r2_04_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME mesh_repair_r3_01_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r3_01_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
        --classify-r3-01
)
set_tests_properties(mesh_repair_r3_01_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME mesh_repair_r3_01a_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r3_01a_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
        --analyze-r3-01a
        --complete-self-intersection-max-candidates 1000
)
set_tests_properties(mesh_repair_r3_01a_cli_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME geometry_kernel_service_unit_tests COMMAND geometry_kernel_service_unit_tests)
add_test(NAME openvdb_sdf_utility_report_unit_tests COMMAND openvdb_sdf_utility_report_unit_tests)
add_test(NAME material_closure_report_unit_tests COMMAND material_closure_report_unit_tests)
add_test(NAME material_closure_candidate_detector_unit_tests COMMAND material_closure_candidate_detector_unit_tests)
add_test(NAME material_closure_semantic_detector_unit_tests COMMAND material_closure_semantic_detector_unit_tests)
add_test(NAME surface_shell_texture_service_unit_tests COMMAND surface_shell_texture_service_unit_tests)
add_test(NAME material_channel_composer_unit_tests COMMAND material_channel_composer_unit_tests)
add_test(NAME mesh_repair_contract_unit_tests COMMAND mesh_repair_contract_unit_tests)
add_test(NAME mesh_repair_eligibility_unit_tests COMMAND mesh_repair_eligibility_unit_tests)
add_test(NAME mesh_repair_fixture_golden_unit_tests COMMAND mesh_repair_fixture_golden_unit_tests)
add_test(NAME mesh_repair_preflight_unit_tests COMMAND mesh_repair_preflight_unit_tests)
add_test(NAME mesh_repair_cleanup_unit_tests COMMAND mesh_repair_cleanup_unit_tests)
add_test(NAME mesh_repair_evidence_validator_unit_tests COMMAND mesh_repair_evidence_validator_unit_tests)
add_test(NAME mesh_non_manifold_pattern_classifier_unit_tests COMMAND mesh_non_manifold_pattern_classifier_unit_tests)
add_test(NAME mesh_complete_self_intersection_analyzer_unit_tests COMMAND mesh_complete_self_intersection_analyzer_unit_tests)

if(ENABLE_GEOMETRY_KERNEL_DEMO)
    add_executable(geometry_kernel_demo
        apps/geometry_kernel_demo/main.cpp
    )

    target_link_libraries(geometry_kernel_demo PRIVATE slicer_core)

    add_executable(openvdb_sdf_utility_probe
        apps/openvdb_sdf_utility_probe/main.cpp
    )

    target_link_libraries(openvdb_sdf_utility_probe PRIVATE slicer_core)

    add_executable(surface_shell_texture_demo
        apps/surface_shell_texture_demo/main.cpp
    )

    target_link_libraries(surface_shell_texture_demo PRIVATE slicer_core)

    add_executable(surface_shell_real_model_demo
        apps/surface_shell_real_model_demo/main.cpp
    )

    target_link_libraries(surface_shell_real_model_demo PRIVATE slicer_core)

    add_executable(surface_shell_robustness_demo
        apps/surface_shell_robustness_demo/main.cpp
    )

    target_link_libraries(surface_shell_robustness_demo PRIVATE slicer_core)
endif()

if(BUILD_SLICER_DEBUG_UI)
    find_package(Qt5 COMPONENTS Widgets QUIET)
    if(Qt5Widgets_FOUND)
        add_subdirectory(apps/slicer_debug_ui)
    else()
        message(STATUS "Qt5 Widgets not found; slicer_debug_ui target is skipped.")
    endif()
endif()
