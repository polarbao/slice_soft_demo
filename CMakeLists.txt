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
    src/slicer_core/config/SlicePipelineConfig.cpp
    src/slicer_core/config/SlicePipelineConfig.h
    src/slicer_core/config.cpp
    src/slicer_core/config.h
    src/slicer_core/diagnostics/Diagnostics.cpp
    src/slicer_core/diagnostics/Diagnostics.h
    src/slicer_core/diagnostics/MaterialClosureCandidateDetector.cpp
    src/slicer_core/diagnostics/MaterialClosureCandidateDetector.h
    src/slicer_core/diagnostics/MaterialClosureSemanticDetector.cpp
    src/slicer_core/diagnostics/MaterialClosureSemanticDetector.h
    src/slicer_core/diagnostics/ModelPreflightReport.cpp
    src/slicer_core/diagnostics/ModelPreflightReport.h
    src/slicer_core/diagnostics/RepairedAssetIntakeReport.cpp
    src/slicer_core/diagnostics/RepairedAssetIntakeReport.h
    src/slicer_core/diagnostics/MeshRepairReport.cpp
    src/slicer_core/diagnostics/MeshRepairReport.h
    src/slicer_core/diagnostics/TextureFillPartitionClosureAdapter.cpp
    src/slicer_core/diagnostics/TextureFillPartitionClosureAdapter.h
    src/slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.cpp
    src/slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h
    src/slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.cpp
    src/slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h
    src/slicer_core/diagnostics/TextureFillPartitionPositiveMatrix.cpp
    src/slicer_core/diagnostics/TextureFillPartitionPositiveMatrix.h
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
    src/slicer_core/layout/GridLayoutPolicy.cpp
    src/slicer_core/layout/GridLayoutPolicy.h
    src/slicer_core/layout/SceneCollisionService.cpp
    src/slicer_core/layout/SceneCollisionService.h
    src/slicer_core/geometry/repair/MeshRepairTypes.cpp
    src/slicer_core/geometry/repair/MeshRepairTypes.h
    src/slicer_core/geometry/NearestTriangleQuery.cpp
    src/slicer_core/geometry/NearestTriangleQuery.h
    src/slicer_core/geometry/PointInClosedMeshQuery.cpp
    src/slicer_core/geometry/PointInClosedMeshQuery.h
    src/slicer_core/geometry/SceneModelTriangleMeshAdapter.cpp
    src/slicer_core/geometry/SceneModelTriangleMeshAdapter.h
    src/slicer_core/geometry/TransformedModelAdapter.cpp
    src/slicer_core/geometry/TransformedModelAdapter.h
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
    src/slicer_core/materials/process_profile/ModelFillMaterialResolver.cpp
    src/slicer_core/materials/process_profile/ModelFillMaterialResolver.h
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
    src/slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.cpp
    src/slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h
    src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.cpp
    src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.h
    src/slicer_core/model.cpp
    src/slicer_core/model.h
    src/slicer_core/output/rgbwsv/RgbwsvPackage.cpp
    src/slicer_core/output/rgbwsv/RgbwsvPackage.h
    src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.cpp
    src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.h
    src/slicer_core/output/rgbwsv/RgbwsvSceneExtension.cpp
    src/slicer_core/output/rgbwsv/RgbwsvSceneExtension.h
    src/slicer_core/pipeline/PipelineContext.cpp
    src/slicer_core/pipeline/PipelineContext.h
    src/slicer_core/pipeline/DiagnosticEffectiveConfig.cpp
    src/slicer_core/pipeline/DiagnosticEffectiveConfig.h
    src/slicer_core/pipeline/TextureFillPartitionDiagnosticFacade.cpp
    src/slicer_core/pipeline/TextureFillPartitionDiagnosticFacade.h
    src/slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.cpp
    src/slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h
    src/slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.cpp
    src/slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h
    src/slicer_core/pipeline/GlobalSceneLayerAdapter.cpp
    src/slicer_core/pipeline/GlobalSceneLayerAdapter.h
    src/slicer_core/pipeline/LegacySceneLayerAdapter.cpp
    src/slicer_core/pipeline/LegacySceneLayerAdapter.h
    src/slicer_core/pipeline/MultiModelScenePackageWriter.cpp
    src/slicer_core/pipeline/MultiModelScenePackageWriter.h
    src/slicer_core/pipeline/MultiModelProductionService.cpp
    src/slicer_core/pipeline/MultiModelProductionService.h
    src/slicer_core/pipeline/MultiModelSliceOrchestrator.cpp
    src/slicer_core/pipeline/MultiModelSliceOrchestrator.h
    src/slicer_core/pipeline/SceneLayerComposer.cpp
    src/slicer_core/pipeline/SceneLayerComposer.h
    src/slicer_core/pipeline/SceneRasterTypes.cpp
    src/slicer_core/pipeline/SceneRasterTypes.h
    src/slicer_core/pipeline/TranslatedSceneRasterReuse.cpp
    src/slicer_core/pipeline/TranslatedSceneRasterReuse.h
    src/slicer_core/pipeline/GlobalSurfaceShellMaterialEvidence.cpp
    src/slicer_core/pipeline/GlobalSurfaceShellMaterialEvidence.h
    src/slicer_core/pipeline/GlobalSurfaceShellProductionPackage.cpp
    src/slicer_core/pipeline/GlobalSurfaceShellProductionPackage.h
    src/slicer_core/pipeline/GlobalSurfaceShellProductionPipeline.cpp
    src/slicer_core/pipeline/GlobalSurfaceShellProductionPipeline.h
    src/slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.cpp
    src/slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.h
    src/slicer_core/pipeline/OpenVdbCandidatePipeline.cpp
    src/slicer_core/pipeline/OpenVdbCandidatePipeline.h
    src/slicer_core/pipeline/ModelPreflightGate.cpp
    src/slicer_core/pipeline/ModelPreflightGate.h
    src/slicer_core/pipeline/PipelineStepResult.cpp
    src/slicer_core/pipeline/PipelineStepResult.h
    src/slicer_core/pipeline/SlicePipeline.cpp
    src/slicer_core/pipeline/SlicePipeline.h
    src/slicer_core/pipeline/SlicePipelineRouter.cpp
    src/slicer_core/pipeline/SlicePipelineRouter.h
    src/slicer_core/preview/ProductionLayerRef.h
    src/slicer_core/preview/MaterialPreviewComposer.cpp
    src/slicer_core/preview/MaterialPreviewComposer.h
    src/slicer_core/preview/TiffLayerCache.cpp
    src/slicer_core/preview/TiffLayerCache.h
    src/slicer_core/preview/TiffLayerSource.cpp
    src/slicer_core/preview/TiffLayerSource.h
    src/slicer_core/preview/TextureFillPartitionSemanticPreview.cpp
    src/slicer_core/preview/TextureFillPartitionSemanticPreview.h
    src/slicer_core/preflight/ModelPreflightCacheIdentity.cpp
    src/slicer_core/preflight/ModelPreflightCacheIdentity.h
    src/slicer_core/preflight/ModelPreflightAdmissionPolicy.cpp
    src/slicer_core/preflight/ModelPreflightAdmissionPolicy.h
    src/slicer_core/preflight/ModelPreflightService.cpp
    src/slicer_core/preflight/ModelPreflightService.h
    src/slicer_core/preflight/ModelPreflightTypes.cpp
    src/slicer_core/preflight/ModelPreflightTypes.h
    src/slicer_core/preflight/TransformedModelPreflight.cpp
    src/slicer_core/preflight/TransformedModelPreflight.h
    src/slicer_core/preflight/RepairedAssetIntakeService.cpp
    src/slicer_core/preflight/RepairedAssetIntakeService.h
    src/slicer_core/preflight/RepairedAssetIntakeTypes.cpp
    src/slicer_core/preflight/RepairedAssetIntakeTypes.h
    src/slicer_core/raster/RasterBoundary.cpp
    src/slicer_core/raster/RasterBoundary.h
    src/slicer_core/raster/TextureFillPartitionRasterMapper.cpp
    src/slicer_core/raster/TextureFillPartitionRasterMapper.h
    src/slicer_core/reports/ReportBase.cpp
    src/slicer_core/reports/ReportBase.h
    src/slicer_core/reports/MultiModelSceneReport.cpp
    src/slicer_core/reports/MultiModelSceneReport.h
    src/slicer_core/reports/MultiModelSceneMatrixReport.cpp
    src/slicer_core/reports/MultiModelSceneMatrixReport.h
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
    src/slicer_core/scene/SceneViewGeometry.cpp
    src/slicer_core/scene/SceneViewGeometry.h
    src/slicer_core/scene/SceneResourceIdentity.cpp
    src/slicer_core/scene/SceneResourceIdentity.h
    src/slicer_core/scene/ModelInstance.cpp
    src/slicer_core/scene/ModelInstance.h
    src/slicer_core/scene/ModelTransform.cpp
    src/slicer_core/scene/ModelTransform.h
    src/slicer_core/scene/MultiModelScene.cpp
    src/slicer_core/scene/MultiModelScene.h
    src/slicer_core/scene/SceneEffectiveConfig.cpp
    src/slicer_core/scene/SceneEffectiveConfig.h
    src/slicer_core/SliceRunTelemetry.h
    src/slicer_core/slicer.cpp
    src/slicer_core/slicer.h
    src/slicer_core/support/SupportComponentAnalysis.cpp
    src/slicer_core/support/SupportComponentAnalysis.h
    src/slicer_core/support/SupportBaseProjection.cpp
    src/slicer_core/support/SupportBaseProjection.h
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
    src/slicer_core/system/Sha256.cpp
    src/slicer_core/system/Sha256.h
    src/slicer_core/system/Sha256Internal.h
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
    target_compile_options(slicer_core PRIVATE /bigobj /MP)
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

add_executable(texture_fill_partition_positive_matrix
    apps/texture_fill_partition_positive_matrix/Main.cpp
)

target_link_libraries(texture_fill_partition_positive_matrix PRIVATE slicer_core)

add_executable(repaired_asset_intake
    apps/repaired_asset_intake/Main.cpp
)

target_link_libraries(repaired_asset_intake PRIVATE slicer_core)

add_executable(multi_model_scene_matrix
    apps/multi_model_scene_matrix/Main.cpp
)

target_link_libraries(multi_model_scene_matrix PRIVATE slicer_core)
target_compile_definitions(multi_model_scene_matrix PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    SLICESOFT_BUILD_CONFIG="$<CONFIG>"
)
if(WIN32)
    target_link_libraries(multi_model_scene_matrix PRIVATE Psapi)
endif()

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

add_executable(output_resolution_config_unit_tests
    tests/unit/output_resolution_config/Main.cpp
)

target_link_libraries(output_resolution_config_unit_tests PRIVATE slicer_core)

add_executable(preview_output_policy_unit_tests
    tests/unit/preview_output_policy/Main.cpp
)

target_link_libraries(preview_output_policy_unit_tests PRIVATE slicer_core)

add_executable(rip_reader_resolution_unit_tests
    tests/unit/rip_reader_resolution/Main.cpp
)

target_link_libraries(rip_reader_resolution_unit_tests PRIVATE slicer_core)

add_executable(tiff_layer_source_unit_tests
    tests/unit/tiff_layer_source/Main.cpp
)

target_link_libraries(tiff_layer_source_unit_tests PRIVATE slicer_core)

add_executable(tiff_layer_cache_unit_tests
    tests/unit/tiff_layer_cache/Main.cpp
)

target_link_libraries(tiff_layer_cache_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_semantic_preview_unit_tests
    tests/unit/texture_fill_partition_semantic_preview/Main.cpp
)

target_link_libraries(texture_fill_partition_semantic_preview_unit_tests PRIVATE slicer_core)

add_executable(material_preview_composer_unit_tests
    tests/unit/material_preview_composer/Main.cpp
)

target_link_libraries(material_preview_composer_unit_tests PRIVATE slicer_core)

add_executable(outer_varnish_discretization_unit_tests
    tests/unit/outer_varnish_discretization/Main.cpp
)

target_link_libraries(outer_varnish_discretization_unit_tests PRIVATE slicer_core)

add_executable(non_square_raster_pipeline_unit_tests
    tests/unit/non_square_raster_pipeline/Main.cpp
)

target_link_libraries(non_square_raster_pipeline_unit_tests PRIVATE slicer_core)

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

add_executable(texture_fill_partition_diagnostic_facade_unit_tests
    tests/unit/texture_fill_partition_diagnostic_facade/Main.cpp
)

target_link_libraries(texture_fill_partition_diagnostic_facade_unit_tests PRIVATE slicer_core)

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

add_executable(model_fill_material_resolver_unit_tests
    tests/unit/model_fill_material_resolver/Main.cpp
)

target_link_libraries(model_fill_material_resolver_unit_tests PRIVATE slicer_core)

add_executable(texture_fill_partition_positive_matrix_unit_tests
    tests/unit/texture_fill_partition_positive_matrix/Main.cpp
)

target_link_libraries(texture_fill_partition_positive_matrix_unit_tests PRIVATE slicer_core)
target_compile_definitions(texture_fill_partition_positive_matrix_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(repaired_asset_intake_unit_tests
    tests/unit/repaired_asset_intake/Main.cpp
)

target_link_libraries(repaired_asset_intake_unit_tests PRIVATE slicer_core)
target_compile_definitions(repaired_asset_intake_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

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

add_executable(model_preflight_contract_unit_tests
    tests/unit/model_preflight_contract/main.cpp
)

target_link_libraries(model_preflight_contract_unit_tests PRIVATE slicer_core)
target_compile_definitions(model_preflight_contract_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(model_transform_unit_tests
    tests/unit/model_transform/Main.cpp
)

target_link_libraries(model_transform_unit_tests PRIVATE slicer_core)

add_executable(auto_orient_unit_tests
    tests/unit/auto_orient/Main.cpp
)

target_link_libraries(auto_orient_unit_tests PRIVATE slicer_core)

add_executable(scene_view_geometry_unit_tests
    tests/unit/scene_view_geometry/Main.cpp
)

target_link_libraries(scene_view_geometry_unit_tests PRIVATE slicer_core)

add_executable(multimodel_scene_contract_unit_tests
    tests/unit/multimodel_scene_contract/Main.cpp
)

target_link_libraries(multimodel_scene_contract_unit_tests PRIVATE slicer_core)
target_compile_definitions(multimodel_scene_contract_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(grid_layout_policy_unit_tests
    tests/unit/grid_layout_policy/Main.cpp
)

target_link_libraries(grid_layout_policy_unit_tests PRIVATE slicer_core)

add_executable(scene_collision_admission_unit_tests
    tests/unit/scene_collision_admission/Main.cpp
)

target_link_libraries(scene_collision_admission_unit_tests PRIVATE slicer_core)

add_executable(multi_model_layer_composer_unit_tests
    tests/unit/multi_model_layer_composer/Main.cpp
)

target_link_libraries(multi_model_layer_composer_unit_tests PRIVATE slicer_core)

add_executable(scene_layer_adapters_unit_tests
    tests/unit/scene_layer_adapters/Main.cpp
)

target_link_libraries(scene_layer_adapters_unit_tests PRIVATE slicer_core)
target_compile_definitions(scene_layer_adapters_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(multi_model_package_writer_unit_tests
    tests/unit/multi_model_package_writer/Main.cpp
)

target_link_libraries(multi_model_package_writer_unit_tests PRIVATE slicer_core)

add_executable(multi_model_production_service_unit_tests
    tests/unit/multi_model_production_service/Main.cpp
)

target_link_libraries(multi_model_production_service_unit_tests PRIVATE slicer_core)
target_compile_definitions(multi_model_production_service_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(translated_scene_raster_reuse_unit_tests
    tests/unit/translated_scene_raster_reuse/Main.cpp
)

target_link_libraries(translated_scene_raster_reuse_unit_tests PRIVATE slicer_core)

add_executable(multi_model_scene_matrix_report_unit_tests
    tests/unit/multi_model_scene_matrix_report/Main.cpp
)

target_link_libraries(multi_model_scene_matrix_report_unit_tests PRIVATE slicer_core)

add_executable(diagnostic_effective_config_unit_tests
    tests/unit/diagnostic_effective_config/Main.cpp
)

target_link_libraries(diagnostic_effective_config_unit_tests PRIVATE slicer_core)
target_compile_definitions(diagnostic_effective_config_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(model_preflight_service_unit_tests
    tests/unit/model_preflight_service/main.cpp
)

target_link_libraries(model_preflight_service_unit_tests PRIVATE slicer_core)
target_compile_definitions(model_preflight_service_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(model_preflight_admission_unit_tests
    tests/unit/model_preflight_admission/main.cpp
)

target_link_libraries(model_preflight_admission_unit_tests PRIVATE slicer_core)

add_executable(transformed_model_preflight_unit_tests
    tests/unit/transformed_model_preflight/Main.cpp
)

target_link_libraries(transformed_model_preflight_unit_tests PRIVATE slicer_core)
target_compile_definitions(transformed_model_preflight_unit_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(model_preflight_pipeline_gate_unit_tests
    tests/unit/model_preflight_pipeline_gate/main.cpp
)

target_link_libraries(model_preflight_pipeline_gate_unit_tests PRIVATE slicer_core)

add_executable(slice_pipeline_router_unit_tests
    tests/unit/slice_pipeline_router/Main.cpp
)

target_link_libraries(slice_pipeline_router_unit_tests PRIVATE slicer_core)

add_executable(production_mode_catalog_unit_tests
    apps/slicer_debug_ui/services/ProductionModeCatalog.cpp
    tests/unit/production_mode_catalog/Main.cpp
)

target_include_directories(production_mode_catalog_unit_tests PRIVATE
    apps/slicer_debug_ui/services
)
target_link_libraries(production_mode_catalog_unit_tests PRIVATE slicer_core)

add_executable(global_surface_shell_production_layer_adapter_unit_tests
    tests/unit/global_surface_shell_production_layer_adapter/Main.cpp
)

target_link_libraries(global_surface_shell_production_layer_adapter_unit_tests PRIVATE slicer_core)

add_executable(rgbwsv_production_package_writer_unit_tests
    tests/unit/rgbwsv_production_package_writer/Main.cpp
)

target_link_libraries(rgbwsv_production_package_writer_unit_tests PRIVATE slicer_core)

add_executable(global_surface_shell_production_pipeline_unit_tests
    tests/unit/global_surface_shell_production_pipeline/Main.cpp
)

target_link_libraries(global_surface_shell_production_pipeline_unit_tests PRIVATE slicer_core)

add_executable(global_surface_shell_material_evidence_unit_tests
    tests/unit/global_surface_shell_material_evidence/Main.cpp
)

target_link_libraries(global_surface_shell_material_evidence_unit_tests PRIVATE slicer_core)

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
add_test(NAME output_resolution_config_unit_tests COMMAND output_resolution_config_unit_tests)
add_test(NAME preview_output_policy_unit_tests COMMAND preview_output_policy_unit_tests)
add_test(NAME rip_reader_resolution_unit_tests COMMAND rip_reader_resolution_unit_tests)
add_test(NAME tiff_layer_source_unit_tests COMMAND tiff_layer_source_unit_tests)
add_test(NAME tiff_layer_cache_unit_tests COMMAND tiff_layer_cache_unit_tests)
add_test(NAME texture_fill_partition_semantic_preview_unit_tests COMMAND texture_fill_partition_semantic_preview_unit_tests)
add_test(NAME material_preview_composer_unit_tests COMMAND material_preview_composer_unit_tests)
add_test(NAME outer_varnish_discretization_unit_tests COMMAND outer_varnish_discretization_unit_tests)
add_test(NAME non_square_raster_pipeline_unit_tests COMMAND non_square_raster_pipeline_unit_tests)
set_tests_properties(
    non_square_raster_pipeline_unit_tests
    PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME texture_fill_partition_service_unit_tests COMMAND texture_fill_partition_service_unit_tests)
add_test(NAME legacy_cpu_global_distance_unit_tests COMMAND legacy_cpu_global_distance_unit_tests)
add_test(NAME openvdb_texture_fill_conformance_unit_tests COMMAND openvdb_texture_fill_conformance_unit_tests)
add_test(NAME texture_fill_partition_width_sweep_unit_tests COMMAND texture_fill_partition_width_sweep_unit_tests)
add_test(NAME texture_fill_partition_report_unit_tests COMMAND texture_fill_partition_report_unit_tests)
add_test(NAME texture_fill_partition_texture_transfer_unit_tests COMMAND texture_fill_partition_texture_transfer_unit_tests)
add_test(NAME texture_fill_partition_diagnostic_composer_unit_tests COMMAND texture_fill_partition_diagnostic_composer_unit_tests)
add_test(NAME texture_fill_partition_diagnostic_facade_unit_tests COMMAND texture_fill_partition_diagnostic_facade_unit_tests)
add_test(NAME texture_fill_partition_closure_adapter_unit_tests COMMAND texture_fill_partition_closure_adapter_unit_tests)
add_test(NAME texture_fill_partition_raster_mapper_unit_tests COMMAND texture_fill_partition_raster_mapper_unit_tests)
add_test(NAME texture_fill_partition_full_closure_adapter_unit_tests COMMAND texture_fill_partition_full_closure_adapter_unit_tests)
add_test(NAME texture_fill_partition_release_benchmark_unit_tests COMMAND texture_fill_partition_release_benchmark_unit_tests)
add_test(NAME model_fill_material_resolver_unit_tests COMMAND model_fill_material_resolver_unit_tests)
add_test(NAME texture_fill_partition_positive_matrix_unit_tests COMMAND texture_fill_partition_positive_matrix_unit_tests)
add_test(NAME repaired_asset_intake_unit_tests COMMAND repaired_asset_intake_unit_tests)
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
add_test(
    NAME mesh_repair_r3_02_cli_test
    COMMAND mesh_repair_preflight
        --config samples/configs/3mf/three_mf_texture2d_checker.json
        --output ${CMAKE_CURRENT_BINARY_DIR}/output/12e_08c_r3_02_cli_test.json
        --source-id samples/models/3mf/texture2d_checker_cube.3mf
        --voxel-mm 0.10
        --require-openvdb-off
        --execute-r3-02
        --complete-self-intersection-max-candidates 1000
        --weld-tolerance-mm 0.0001
        --max-boundary-loop-edges 16
        --max-boundary-loop-diameter-mm 2.0
        --max-boundary-loop-perimeter-mm 8.0
        --max-boundary-planarity-error-mm 0.01
        --max-hole-area-mm2 4.0
        --max-affected-face-ratio 0.5
)
set_tests_properties(mesh_repair_r3_02_cli_test PROPERTIES
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
add_test(NAME model_preflight_contract_unit_tests COMMAND model_preflight_contract_unit_tests)
add_test(NAME model_transform_unit_tests COMMAND model_transform_unit_tests)
add_test(NAME auto_orient_unit_tests COMMAND auto_orient_unit_tests)
add_test(NAME scene_view_geometry_unit_tests COMMAND scene_view_geometry_unit_tests)
add_test(NAME multimodel_scene_contract_unit_tests COMMAND multimodel_scene_contract_unit_tests)
add_test(NAME grid_layout_policy_unit_tests COMMAND grid_layout_policy_unit_tests)
add_test(NAME scene_collision_admission_unit_tests COMMAND scene_collision_admission_unit_tests)
add_test(NAME multi_model_layer_composer_unit_tests COMMAND multi_model_layer_composer_unit_tests)
add_test(NAME scene_layer_adapters_unit_tests COMMAND scene_layer_adapters_unit_tests)
set_tests_properties(scene_layer_adapters_unit_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME multi_model_package_writer_unit_tests COMMAND multi_model_package_writer_unit_tests)
add_test(NAME multi_model_production_service_unit_tests COMMAND multi_model_production_service_unit_tests)
add_test(
    NAME scene_slice_route_fixture_setup
    COMMAND multi_model_production_service_unit_tests
        --emit-fixture
        "${CMAKE_CURRENT_BINARY_DIR}/scene_slice_route_fixture"
)
set_tests_properties(
    scene_slice_route_fixture_setup
    PROPERTIES FIXTURES_SETUP scene_slice_route_fixture
)
add_test(
    NAME scene_slice_route_positive
    COMMAND slicer_cli
        --scene-config
        "${CMAKE_CURRENT_BINARY_DIR}/scene_slice_route_fixture/scene_config.effective.json"
)
set_tests_properties(
    scene_slice_route_positive
    PROPERTIES
        FIXTURES_REQUIRED scene_slice_route_fixture
        PASS_REGULAR_EXPRESSION "slicer_cli: generated scene package"
)
add_test(
    NAME scene_slice_route_negative
    COMMAND ${CMAKE_COMMAND}
        "-DSLICER_CLI=$<TARGET_FILE:slicer_cli>"
        "-DMISSING_SCENE_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/scene_slice_route_missing.json"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/SceneSliceRouteNegative.cmake"
)
add_test(NAME translated_scene_raster_reuse_unit_tests COMMAND translated_scene_raster_reuse_unit_tests)
add_test(NAME multi_model_scene_matrix_report_unit_tests COMMAND multi_model_scene_matrix_report_unit_tests)
add_test(NAME diagnostic_effective_config_unit_tests COMMAND diagnostic_effective_config_unit_tests)
add_test(NAME model_preflight_service_unit_tests COMMAND model_preflight_service_unit_tests)
add_test(NAME model_preflight_admission_unit_tests COMMAND model_preflight_admission_unit_tests)
add_test(NAME transformed_model_preflight_unit_tests COMMAND transformed_model_preflight_unit_tests)
add_test(NAME model_preflight_pipeline_gate_unit_tests COMMAND model_preflight_pipeline_gate_unit_tests)
add_test(NAME slice_pipeline_router_unit_tests COMMAND slice_pipeline_router_unit_tests)
add_test(NAME production_mode_catalog_unit_tests COMMAND production_mode_catalog_unit_tests)
add_test(NAME global_surface_shell_production_layer_adapter_unit_tests COMMAND global_surface_shell_production_layer_adapter_unit_tests)
add_test(NAME rgbwsv_production_package_writer_unit_tests COMMAND rgbwsv_production_package_writer_unit_tests)
add_test(NAME global_surface_shell_production_pipeline_unit_tests COMMAND global_surface_shell_production_pipeline_unit_tests)
set_tests_properties(global_surface_shell_production_pipeline_unit_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME global_surface_shell_material_evidence_unit_tests COMMAND global_surface_shell_material_evidence_unit_tests)
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

if(TARGET slicer_debug_ui)
    add_custom_target(slicesoft_runtime
        DEPENDS
            slicer_cli
            rip_reader_test
            slicer_debug_ui
    )
endif()
