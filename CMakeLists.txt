cmake_minimum_required(VERSION 3.24)

project(SliceSoftDemo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SLICER_DEBUG_UI "Build Qt slicer debug UI" ON)
option(BUILD_SLICER_UI_HOST_SIM "Build Qt public-ABI reference host" ON)
option(ENABLE_GEOMETRY_KERNEL_DEMO "Build experimental geometry kernel demo" ON)
option(USE_OPENVDB "Enable optional OpenVDB adapter" OFF)
set(
    SLICESOFT_TIFF_BACKEND
    "handwritten"
    CACHE STRING
    "TIFF dependency lane: handwritten or libtiff"
)
set_property(
    CACHE SLICESOFT_TIFF_BACKEND
    PROPERTY STRINGS handwritten libtiff
)
if(NOT SLICESOFT_TIFF_BACKEND STREQUAL "handwritten"
   AND NOT SLICESOFT_TIFF_BACKEND STREQUAL "libtiff")
    message(FATAL_ERROR
        "Unsupported SLICESOFT_TIFF_BACKEND='${SLICESOFT_TIFF_BACKEND}'. "
        "Expected handwritten or libtiff.")
endif()

set(SLICESOFT_CORE_SOURCES
    src/third_party/miniz/miniz.c
    src/third_party/miniz/miniz_tdef.c
    src/third_party/miniz/miniz_tinfl.c
    src/third_party/miniz/miniz_zip.c
    src/slicer_core/api/ApiResult.h
    src/slicer_core/api/Cancellation.h
    src/slicer_core/api/CommonDtos.h
    src/slicer_core/api/Facades.h
    src/slicer_core/api/ModelDtos.h
    src/slicer_core/api/ModelFacade.h
    src/slicer_core/api/PackageDtos.h
    src/slicer_core/api/PackageQueryFacade.h
    src/slicer_core/api/ProfileIdentity.cpp
    src/slicer_core/api/ProfileIdentity.h
    src/slicer_core/api/SceneDtos.h
    src/slicer_core/api/SceneFacade.h
    src/slicer_core/api/SceneViewDtos.h
    src/slicer_core/api/SliceDtos.h
    src/slicer_core/api/SliceFacade.h
    src/slicer_core/api/artifacts/PackageArtifactSafety.cpp
    src/slicer_core/api/artifacts/PackageArtifactSafety.h
    src/slicer_core/api/scene/SceneFacadeAuthority.cpp
    src/slicer_core/api/scene/SceneFacadeAuthority.h
    src/slicer_core/api/scene/SceneFacadeLayout.cpp
    src/slicer_core/api/scene/SceneFacadeOperation.cpp
    src/slicer_core/api/scene/SceneFacadeService.cpp
    src/slicer_core/api/scene/SceneFacadeService.h
    src/slicer_core/api/viewdata/SceneSurfacePreviewBuilder.cpp
    src/slicer_core/api/viewdata/SceneSurfacePreviewBuilder.h
    src/slicer_core/api/viewdata/SceneViewAppearanceBudget.cpp
    src/slicer_core/api/viewdata/SceneViewAppearanceBudget.h
    src/slicer_core/api/viewdata/SceneViewAssetResolver.cpp
    src/slicer_core/api/viewdata/SceneViewBudget.cpp
    src/slicer_core/api/viewdata/SceneViewBudget.h
    src/slicer_core/api/viewdata/SceneViewCandidateBuilder.cpp
    src/slicer_core/api/viewdata/SceneViewCandidateBuilder.h
    src/slicer_core/api/viewdata/SceneViewClosureValidator.cpp
    src/slicer_core/api/viewdata/SceneViewClosureValidator.h
    src/slicer_core/api/viewdata/SceneViewIdentity.cpp
    src/slicer_core/api/viewdata/SceneViewIdentity.h
    src/slicer_core/api/viewdata/MeshSimplifier.cpp
    src/slicer_core/api/viewdata/MeshSimplifier.h
    src/slicer_core/api/viewdata/SceneViewMeshInputBuilder.cpp
    src/slicer_core/api/viewdata/SceneViewMeshInputBuilder.h
    src/slicer_core/api/viewdata/SceneViewMeshBuilder.cpp
    src/slicer_core/api/viewdata/SceneViewMeshBuilder.h
    src/slicer_core/api/viewdata/SceneViewOutlineBuilder.cpp
    src/slicer_core/api/viewdata/SceneViewOutlineBuilder.h
    src/slicer_core/api/viewdata/SceneViewResolvedAsset.h
    src/slicer_core/api/viewdata/SceneViewResources.cpp
    src/slicer_core/api/viewdata/SceneViewResources.h
    src/slicer_core/api/viewdata/TexturedSceneViewDataProvider.cpp
    src/slicer_core/api/viewdata/TexturedSceneViewDataProvider.h
    src/slicer_core/api/implementation/ModelFacadeImplementation.cpp
    src/slicer_core/api/implementation/ModelFacadeImplementation.h
    src/slicer_core/api/implementation/PackageQueryFacadeCommon.cpp
    src/slicer_core/api/implementation/PackageQueryFacadeImplementation.cpp
    src/slicer_core/api/implementation/PackageQueryFacadeImplementation.h
    src/slicer_core/api/implementation/PackageQueryFacadeInternal.h
    src/slicer_core/api/implementation/PackageQueryFacadePackage.cpp
    src/slicer_core/api/implementation/PackageQueryFacadePreview.cpp
    src/slicer_core/api/implementation/PackageQueryFacadePreviewEncoding.cpp
    src/slicer_core/api/implementation/PackageQueryFacadeReport.cpp
    src/slicer_core/config/ConfigMigration.cpp
    src/slicer_core/config/ConfigMigration.h
    src/slicer_core/config/ConfigSchema.cpp
    src/slicer_core/config/ConfigSchema.h
    src/slicer_core/config/NormalizedConfig.cpp
    src/slicer_core/config/NormalizedConfig.h
    src/slicer_core/config/OutputResolution.cpp
    src/slicer_core/config/OutputResolution.h
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
    src/slicer_core/geometry/repair/DeterministicObjAssetWriter.cpp
    src/slicer_core/geometry/repair/DeterministicObjAssetWriter.h
    src/slicer_core/geometry/repair/StrictRepairAssetRecheck.cpp
    src/slicer_core/geometry/repair/StrictRepairAssetRecheck.h
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
    src/slicer_core/engine/ProductionSliceFacadeFactory.cpp
    src/slicer_core/engine/ProductionSliceFacadeFactory.h
    src/slicer_core/engine/ProductionPreflightFullFacadeFactory.cpp
    src/slicer_core/engine/ProductionPreflightFullFacadeFactory.h
    src/slicer_core/engine/ProductionRepairFacadeFactory.cpp
    src/slicer_core/engine/ProductionRepairFacadeFactory.h
    src/slicer_core/engine/SliceFacadeAdapter.cpp
    src/slicer_core/engine/SliceFacadeAdapter.h
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
    src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.cpp
    src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h
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
    src/slicer_core/model/ModelLoadConfig.h
    src/slicer_core/model/ModelLoadConfigAdapter.cpp
    src/slicer_core/model/ObjFaceParser.h
    src/slicer_core/model/ObjFaceParser.cpp
    src/slicer_core/output/rgbwsv/RgbwsvPackage.cpp
    src/slicer_core/output/rgbwsv/RgbwsvPackage.h
    src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.cpp
    src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.h
    src/slicer_core/output/rgbwsv/RgbwsvCapabilitySummary.cpp
    src/slicer_core/output/rgbwsv/RgbwsvCapabilitySummary.h
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
    src/slicer_core/preflight/SceneFullPreflightResourceResolver.cpp
    src/slicer_core/preflight/SceneFullPreflightService.cpp
    src/slicer_core/preflight/SceneFullPreflightService.h
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
    src/slicer_core/reports/SceneCapabilitySummary.cpp
    src/slicer_core/reports/SceneCapabilitySummary.h
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
    src/slicer_core/output/tiff/HandwrittenTiffStructureInternal.h
    src/slicer_core/output/tiff/HandwrittenTiffWriter.cpp
    src/slicer_core/output/tiff/ITiffWriter.h
    src/slicer_core/output/tiff/LibTiffWriter.cpp
    src/slicer_core/output/tiff/TiffBackendBuildInfo.cpp
    src/slicer_core/output/tiff/TiffBackendBuildInfo.h
    src/slicer_core/output/tiff/TiffWriterFactory.cpp
    src/slicer_core/output/tiff/TiffWriterFactory.h
    src/slicer_core/output/tiff/TiffWriterError.cpp
    src/slicer_core/output/tiff/TiffWriterError.h
    src/slicer_core/output/tiff/TiffWriterImplementations.h
    src/slicer_core/output/tiff/TiffPackBitsWriteInternal.h
    src/slicer_core/texture_image.cpp
    src/slicer_core/texture_image.h
    src/slicer_core/TiffPackBitsReadInternal.h
    src/slicer_core/TiffReadApi.h
    src/slicer_core/TiffReadStructureInternal.h
    src/slicer_core/tiff_io.cpp
    src/slicer_core/tiff_io.h
)

include(cmake/SliceSoftCoreLayering.cmake)
SliceSoftPartitionCoreSources(
    SLICESOFT_CORE_SOURCES
    SLICESOFT_BASE_SOURCES
    SLICESOFT_ENGINE_SOURCES
)

set(SLICESOFT_LAYER_ASSIGNMENT_FILE
    "${CMAKE_BINARY_DIR}/stage14b_layer_assignment.txt"
)
file(WRITE "${SLICESOFT_LAYER_ASSIGNMENT_FILE}" "")
foreach(source IN LISTS SLICESOFT_BASE_SOURCES)
    file(APPEND "${SLICESOFT_LAYER_ASSIGNMENT_FILE}" "base ${source}\n")
endforeach()
foreach(source IN LISTS SLICESOFT_ENGINE_SOURCES)
    file(APPEND "${SLICESOFT_LAYER_ASSIGNMENT_FILE}" "engine ${source}\n")
endforeach()

add_library(slicer_base STATIC ${SLICESOFT_BASE_SOURCES})
add_library(slicer_engine STATIC ${SLICESOFT_ENGINE_SOURCES})
add_library(slicer_core INTERFACE)

find_package(meshoptimizer CONFIG REQUIRED)

target_include_directories(slicer_base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniz
)
target_include_directories(slicer_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniz
)
target_link_libraries(slicer_engine PUBLIC slicer_base)
target_link_libraries(slicer_core INTERFACE slicer_engine)
target_link_libraries(slicer_base PRIVATE meshoptimizer::meshoptimizer)
target_compile_definitions(
    slicer_engine
    PRIVATE
        SLICESOFT_CONFIGURED_TIFF_BACKEND="${SLICESOFT_TIFF_BACKEND}"
)

if(MSVC)
    target_compile_options(slicer_base PRIVATE /bigobj /MP)
    target_compile_options(slicer_engine PRIVATE /bigobj /MP)
endif()

if(WIN32)
    target_link_libraries(slicer_base PUBLIC windowscodecs ole32)
    target_link_libraries(slicer_engine PRIVATE psapi)
endif()

set(_slicesoft_tiff_runtime_file "")
set(_slicesoft_tiff_license_file "")
if(SLICESOFT_TIFF_BACKEND STREQUAL "libtiff")
    find_package(TIFF REQUIRED)
    if(NOT TARGET TIFF::TIFF)
        message(FATAL_ERROR
            "LibTIFF was found but the TIFF::TIFF imported target is unavailable.")
    endif()
    target_compile_definitions(slicer_engine PRIVATE SLICER_CORE_HAS_LIBTIFF=1)
    target_link_libraries(slicer_engine PRIVATE TIFF::TIFF)
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        if(WIN32)
            find_file(
                _slicesoft_tiff_release_runtime
                NAMES tiff.dll libtiff.dll
                PATHS
                    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
                NO_DEFAULT_PATH
                REQUIRED
            )
            find_file(
                _slicesoft_tiff_debug_runtime
                NAMES tiffd.dll libtiffd.dll tiff.dll libtiff.dll
                PATHS
                    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin"
                NO_DEFAULT_PATH
                REQUIRED
            )
            set(
                _slicesoft_tiff_runtime_file
                "$<IF:$<CONFIG:Debug>,${_slicesoft_tiff_debug_runtime},${_slicesoft_tiff_release_runtime}>"
            )
        else()
            set(_slicesoft_tiff_runtime_file "$<TARGET_FILE:TIFF::TIFF>")
        endif()
        set(
            _slicesoft_tiff_license_file
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/tiff/copyright"
        )
    else()
        set(_slicesoft_tiff_runtime_file "$<TARGET_FILE:TIFF::TIFF>")
    endif()
endif()

file(
    GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/slicesoft_tiff_runtime_$<CONFIG>.txt"
    CONTENT "backend=${SLICESOFT_TIFF_BACKEND}\nruntime=${_slicesoft_tiff_runtime_file}\nlicense=${_slicesoft_tiff_license_file}\n"
)

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
    target_compile_definitions(slicer_engine PRIVATE SLICER_CORE_USE_OPENVDB=1)
    if(TARGET OpenVDB::openvdb)
        target_link_libraries(slicer_engine PRIVATE OpenVDB::openvdb)
    elseif(TARGET OpenVDB::OpenVDB)
        target_link_libraries(slicer_engine PRIVATE OpenVDB::OpenVDB)
    else()
        message(FATAL_ERROR "OpenVDB package found but no supported imported target is available.")
    endif()
endif()

add_library(print_module_spi_c_compile_test OBJECT
    tests/contracts/PrintModuleSpiCCompileTest.c
)
target_include_directories(print_module_spi_c_compile_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/contracts
)

add_library(print_module_spi_cpp_compile_test OBJECT
    tests/contracts/PrintModuleSpiCppCompileTest.cpp
)
target_include_directories(print_module_spi_cpp_compile_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/contracts
)

add_library(slicer_module SHARED
    src/slicer_module/BufferApi.cpp
    src/slicer_module/BufferApi.h
    src/slicer_module/CapabilityJsonAdapter.cpp
    src/slicer_module/CapabilityJsonAdapter.h
    src/slicer_module/CapabilityCarrierRouter.cpp
    src/slicer_module/CapabilityCarrierRouter.h
    src/slicer_module/ErrorApi.cpp
    src/slicer_module/ErrorApi.h
    src/slicer_module/DllMain.cpp
    src/slicer_module/Exports.cpp
    src/slicer_module/HandleRegistry.cpp
    src/slicer_module/HandleRegistry.h
    src/slicer_module/ModuleInfo.cpp
    src/slicer_module/ModuleInfo.h
    src/slicer_module/ModuleInitialization.cpp
    src/slicer_module/ModuleInitialization.h
    src/slicer_module/ModuleSelfTest.cpp
    src/slicer_module/ModuleSelfTest.h
    src/slicer_module/ModelCapabilityAdapter.cpp
    src/slicer_module/ModelCapabilityAdapter.h
    src/slicer_module/PackageCapabilityAdapter.cpp
    src/slicer_module/PackageCapabilityAdapter.h
    src/slicer_module/SceneCapabilityAdapter.cpp
    src/slicer_module/SceneCapabilityAdapter.h
    src/slicer_module/SceneCapabilitySerializationAdapter.cpp
    src/slicer_module/SceneCapabilitySerializationAdapter.h
    src/slicer_module/SceneLifecycleSupport.cpp
    src/slicer_module/SceneLifecycleSupport.h
    src/slicer_module/SceneViewDataAdapter.cpp
    src/slicer_module/SceneViewDataAdapter.h
    src/slicer_module/SyncCapabilityAdapter.cpp
    src/slicer_module/SyncCapabilityAdapter.h
    src/slicer_module/WorkerClient.cpp
    src/slicer_module/WorkerClient.h
    src/slicer_module/WorkerContract.cpp
    src/slicer_module/WorkerContract.h
    src/slicer_module/WorkerJobService.cpp
    src/slicer_module/WorkerJobService.h
    src/slicer_module/WorkerProcessWindows.cpp
    src/slicer_module/WorkerProcessWindows.h
    src/slicer_module/WorkerProtocol.cpp
    src/slicer_module/WorkerProtocol.h
    src/slicer_module/slicer_module.def
)
target_include_directories(slicer_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(slicer_module PRIVATE PM_MODULE_BUILD_SHARED)
target_link_libraries(slicer_module PRIVATE slicer_base)
set_target_properties(slicer_module PROPERTIES PREFIX "")

set(SLICESOFT_MODULE_RUNTIME "MSVC-x64-MDd")
set(SLICESOFT_MODULE_BUILD_CONFIG "Debug")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/slicer_module/module.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/slicer_module_manifest/Debug/module.json"
    @ONLY
)
set(SLICESOFT_MODULE_RUNTIME "MSVC-x64-MD")
set(SLICESOFT_MODULE_BUILD_CONFIG "Release")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/slicer_module/module.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/slicer_module_manifest/Release/module.json"
    @ONLY
)
add_custom_command(TARGET slicer_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/slicer_module_manifest/$<CONFIG>/module.json"
        "$<TARGET_FILE_DIR:slicer_module>/module.json"
    VERBATIM
)

add_executable(stage14c02_buffer_api_unit_tests
    tests/stage14c_02/BufferApiTests.cpp
    src/slicer_module/BufferApi.cpp
    src/slicer_module/BufferApi.h
)
target_include_directories(stage14c02_buffer_api_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(stage14c03_handle_registry_unit_tests
    tests/stage14c_03/HandleRegistryTests.cpp
    src/slicer_module/ErrorApi.cpp
    src/slicer_module/ErrorApi.h
    src/slicer_module/HandleRegistry.cpp
    src/slicer_module/HandleRegistry.h
)
target_include_directories(stage14c03_handle_registry_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(stage14c03_module_abi_tests
    tests/stage14c_03/ModuleAbiTests.cpp
)
target_include_directories(stage14c03_module_abi_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(stage14c03_module_abi_tests PRIVATE slicer_module)
if(WIN32)
    target_link_libraries(stage14c03_module_abi_tests PRIVATE Psapi)
endif()

add_executable(stage14c05_module_info_tests
    tests/stage14c_05/ModuleInfoTests.cpp
    src/slicer_module/ModuleInfo.cpp
    src/slicer_module/ModuleInfo.h
)
target_include_directories(stage14c05_module_info_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14c05_module_info_tests PRIVATE slicer_module)
if(MSVC)
    target_compile_options(stage14c05_module_info_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14c07_dll_initialization_tests
    tests/stage14c_07/DllInitializationTests.cpp
    src/slicer_module/ModuleInitialization.cpp
    src/slicer_module/ModuleInitialization.h
)
target_include_directories(stage14c07_dll_initialization_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14c07_dll_initialization_tests PRIVATE slicer_module)
if(MSVC)
    target_compile_options(stage14c07_dll_initialization_tests PRIVATE /W4 /WX)
endif()

add_executable(test_spi_conformance
    tests/stage14c_06/SpiConformanceTests.cpp
    tests/stage14c_06/SpiModuleApi.cpp
    tests/stage14c_06/SpiModuleApi.h
    tests/stage14c_06/WorkerLifecycleConformance.cpp
    tests/stage14c_06/WorkerLifecycleConformance.h
)
target_include_directories(test_spi_conformance PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_spi_conformance PRIVATE slicer_engine)
add_dependencies(test_spi_conformance slicer_module slicer_worker)
if(WIN32)
    target_link_libraries(test_spi_conformance PRIVATE Psapi)
endif()
if(MSVC)
    target_compile_options(test_spi_conformance PRIVATE /W4 /WX)
endif()

add_executable(stage14d02_worker_client_unit_tests
    tests/stage14d_02/WorkerClientTests.cpp
    src/slicer_module/WorkerClient.cpp
    src/slicer_module/WorkerClient.h
    src/slicer_module/WorkerProcessWindows.cpp
    src/slicer_module/WorkerProcessWindows.h
    src/slicer_module/WorkerProtocol.cpp
    src/slicer_module/WorkerProtocol.h
)
target_include_directories(stage14d02_worker_client_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d02_worker_client_unit_tests PRIVATE slicer_base)

add_executable(stage14d03_worker_contract_unit_tests
    tests/stage14d_03/WorkerContractTests.cpp
    src/slicer_module/WorkerClient.cpp
    src/slicer_module/WorkerClient.h
    src/slicer_module/WorkerContract.cpp
    src/slicer_module/WorkerContract.h
    src/slicer_module/WorkerProcessWindows.cpp
    src/slicer_module/WorkerProcessWindows.h
    src/slicer_module/WorkerProtocol.cpp
    src/slicer_module/WorkerProtocol.h
)
target_include_directories(stage14d03_worker_contract_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d03_worker_contract_unit_tests PRIVATE slicer_base)
add_dependencies(stage14d03_worker_contract_unit_tests slicer_worker)
if(MSVC)
    target_compile_options(stage14d03_worker_contract_unit_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d06_capability_carrier_tests
    tests/stage14d_06/CapabilityCarrierRouterTests.cpp
    src/slicer_module/CapabilityCarrierRouter.cpp
    src/slicer_module/CapabilityCarrierRouter.h
    src/slicer_module/CapabilityJsonAdapter.cpp
    src/slicer_module/CapabilityJsonAdapter.h
)
target_include_directories(stage14d06_capability_carrier_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d06_capability_carrier_tests PRIVATE slicer_base)
if(MSVC)
    target_compile_options(stage14d06_capability_carrier_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d06_public_worker_routing_tests
    tests/stage14d_06/PublicWorkerRoutingTests.cpp
)
target_include_directories(stage14d06_public_worker_routing_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(stage14d06_public_worker_routing_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(stage14d06_public_worker_routing_tests PRIVATE
    slicer_module
    slicer_engine
)
if(MSVC)
    target_compile_options(stage14d06_public_worker_routing_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d04a_cancellation_unit_tests
    tests/stage14d_04a/Stage14D04ACancellationTests.cpp
)
target_include_directories(stage14d04a_cancellation_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d04a_cancellation_unit_tests PRIVATE slicer_engine)
if(MSVC)
    target_compile_options(stage14d04a_cancellation_unit_tests PRIVATE /W4 /WX)
endif()

add_executable(model_import_layering_probe
    tests/contracts/ModelImportLayeringProbe.cpp
)
target_include_directories(model_import_layering_probe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniz
)
target_link_libraries(model_import_layering_probe PRIVATE slicer_base)
if(MSVC)
    target_compile_options(model_import_layering_probe PRIVATE /bigobj /MP)
endif()

add_executable(facade_contract_unit_tests
    tests/contracts/FacadeContractUnitTests.cpp
)
target_include_directories(facade_contract_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(scene_facade_14b03_unit_tests
    tests/contracts/scene_facade_14b03/Main.cpp
)
target_include_directories(scene_facade_14b03_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(scene_facade_14b03_unit_tests PRIVATE slicer_base)
if(MSVC)
    target_compile_options(scene_facade_14b03_unit_tests PRIVATE /bigobj /MP)
endif()

add_executable(hostflow_ha02_scene_lifecycle_tests
    tests/hostflow/SceneLifecycleAdapterTests.cpp
)
target_include_directories(hostflow_ha02_scene_lifecycle_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(
    hostflow_ha02_scene_lifecycle_tests
    PRIVATE slicer_module slicer_base
)
if(MSVC)
    target_compile_options(
        hostflow_ha02_scene_lifecycle_tests
        PRIVATE /bigobj /MP
    )
endif()

add_executable(hostflow_ha04_grid_layout_tests
    tests/hostflow/SceneLayoutAdapterTests.cpp
)
target_include_directories(hostflow_ha04_grid_layout_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(
    hostflow_ha04_grid_layout_tests
    PRIVATE slicer_module slicer_base
)
if(MSVC)
    target_compile_options(
        hostflow_ha04_grid_layout_tests
        PRIVATE /bigobj /MP
    )
endif()

add_executable(textured_scene_viewdata_14b03a_unit_tests
    tests/stage14b_03a/Main.cpp
    tests/stage14b_03a/PositiveCases.cpp
    tests/stage14b_03a/SimplificationCases.cpp
    tests/stage14b_03a/FailureCases.cpp
)
target_include_directories(textured_scene_viewdata_14b03a_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14b_03a
)
target_link_libraries(
    textured_scene_viewdata_14b03a_unit_tests
    PRIVATE slicer_base
)

add_executable(textured_scene_viewdata_14b03a_real_fixture_tests
    tests/stage14b_03a/RealFixtureMain.cpp
)
target_include_directories(
    textured_scene_viewdata_14b03a_real_fixture_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14b_03a
)
target_compile_definitions(
    textured_scene_viewdata_14b03a_real_fixture_tests
    PRIVATE
        SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        SLICESOFT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(
    textured_scene_viewdata_14b03a_real_fixture_tests
    PRIVATE slicer_base
)
if(MSVC)
    target_compile_options(
        textured_scene_viewdata_14b03a_unit_tests
        PRIVATE /bigobj /MP
    )
    target_compile_options(
        textured_scene_viewdata_14b03a_real_fixture_tests
        PRIVATE /bigobj /MP
    )
endif()

add_executable(package_query_facade_14b02_unit_tests
    tests/stage14b_02/Main.cpp
)
target_include_directories(package_query_facade_14b02_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14b_02
)
target_link_libraries(package_query_facade_14b02_unit_tests PRIVATE slicer_engine)

add_executable(package_query_facade_14b02_base_link_test
    tests/contracts/Stage14B02BaseLinkTest.cpp
)
target_include_directories(package_query_facade_14b02_base_link_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(package_query_facade_14b02_base_link_test PRIVATE slicer_base)
if(MSVC)
    target_compile_options(package_query_facade_14b02_unit_tests PRIVATE /bigobj /MP)
    target_compile_options(package_query_facade_14b02_base_link_test PRIVATE /bigobj /MP)
endif()

add_executable(slice_facade_14b04_unit_tests
    tests/contracts/Stage14B04SliceFacadeUnitTests.cpp
)
target_include_directories(slice_facade_14b04_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(slice_facade_14b04_unit_tests PRIVATE slicer_engine)

add_executable(slice_facade_14b04_production_binding_test
    tests/contracts/Stage14B04ProductionBindingLinkTest.cpp
)
target_include_directories(slice_facade_14b04_production_binding_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(
    slice_facade_14b04_production_binding_test
    PRIVATE slicer_engine
)
if(MSVC)
    target_compile_options(slice_facade_14b04_unit_tests PRIVATE /bigobj /MP)
    target_compile_options(
        slice_facade_14b04_production_binding_test
        PRIVATE /bigobj /MP
    )
endif()

add_executable(slicer_cli
    apps/slicer_cli/main.cpp
)

target_link_libraries(slicer_cli PRIVATE slicer_core)

add_library(slicer_worker_runtime STATIC
    apps/slicer_worker/runtime/WorkerCapabilityExecutor.cpp
    apps/slicer_worker/runtime/WorkerCapabilityExecutor.h
    apps/slicer_worker/runtime/WorkerJobIdentity.cpp
    apps/slicer_worker/runtime/WorkerJobIdentity.h
    apps/slicer_worker/runtime/WorkerJobDispatcher.cpp
    apps/slicer_worker/runtime/WorkerJobDispatcher.h
    apps/slicer_worker/runtime/WorkerJobRuntime.cpp
    apps/slicer_worker/runtime/WorkerJobRuntime.h
    apps/slicer_worker/runtime/WorkerRequestEnvelope.cpp
    apps/slicer_worker/runtime/WorkerRequestEnvelope.h
    apps/slicer_worker/runtime/WorkerRequestParser.cpp
    apps/slicer_worker/runtime/WorkerRequestParser.h
    apps/slicer_worker/runtime/WorkerResultEnvelope.cpp
    apps/slicer_worker/runtime/WorkerResultEnvelope.h
    apps/slicer_worker/runtime/WorkerResultWriter.cpp
    apps/slicer_worker/runtime/WorkerResultWriter.h
)
target_include_directories(slicer_worker_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(slicer_worker_runtime PUBLIC slicer_base)
if(MSVC)
    target_compile_options(slicer_worker_runtime PRIVATE /W4 /WX)
endif()

add_library(slicer_worker_slice_runtime STATIC
    apps/slicer_worker/slice/WorkerSliceExecutor.cpp
    apps/slicer_worker/slice/WorkerSliceExecutor.h
    apps/slicer_worker/slice/WorkerSliceRequestMaterializer.cpp
    apps/slicer_worker/slice/WorkerSliceRequestMaterializer.h
)
target_include_directories(slicer_worker_slice_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(slicer_worker_slice_runtime PUBLIC
    slicer_worker_runtime
    slicer_engine
)
if(MSVC)
    target_compile_options(slicer_worker_slice_runtime PRIVATE /W4 /WX)
endif()

add_library(slicer_worker_preflight_runtime STATIC
    apps/slicer_worker/preflight/WorkerPreflightExecutor.cpp
    apps/slicer_worker/preflight/WorkerPreflightExecutor.h
)
target_include_directories(slicer_worker_preflight_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(slicer_worker_preflight_runtime PUBLIC
    slicer_worker_runtime
    slicer_engine
)
if(MSVC)
    target_compile_options(slicer_worker_preflight_runtime PRIVATE /W4 /WX)
endif()

add_library(slicer_worker_repair_runtime STATIC
    apps/slicer_worker/repair/WorkerRepairExecutor.cpp
    apps/slicer_worker/repair/WorkerRepairExecutor.h
)
target_include_directories(slicer_worker_repair_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(slicer_worker_repair_runtime PUBLIC
    slicer_worker_runtime
    slicer_engine
)
if(MSVC)
    target_compile_options(slicer_worker_repair_runtime PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r1_worker_runtime_tests
    tests/stage14d_08_r1/WorkerRequestParserTests.cpp
)
target_include_directories(stage14d08_r1_worker_runtime_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d08_r1_worker_runtime_tests PRIVATE
    slicer_worker_runtime
)
if(MSVC)
    target_compile_options(stage14d08_r1_worker_runtime_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r1_result_writer_tests
    tests/stage14d_08_r1/WorkerResultWriterTests.cpp
)
target_include_directories(stage14d08_r1_result_writer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d08_r1_result_writer_tests PRIVATE
    slicer_worker_runtime
)
if(MSVC)
    target_compile_options(stage14d08_r1_result_writer_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r1_dispatcher_tests
    tests/stage14d_08_r1/WorkerJobDispatcherTests.cpp
)
target_include_directories(stage14d08_r1_dispatcher_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d08_r1_dispatcher_tests PRIVATE
    slicer_worker_runtime
)
if(MSVC)
    target_compile_options(stage14d08_r1_dispatcher_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r2_slice_materializer_tests
    tests/stage14d_08_r2/WorkerSliceRequestMaterializerTests.cpp
)
target_include_directories(stage14d08_r2_slice_materializer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d08_r2_slice_materializer_tests PRIVATE
    slicer_worker_slice_runtime
)
if(MSVC)
    target_compile_options(stage14d08_r2_slice_materializer_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r2_slice_executor_tests
    tests/stage14d_08_r2/WorkerSliceExecutorTests.cpp
)
target_include_directories(stage14d08_r2_slice_executor_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(stage14d08_r2_slice_executor_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(stage14d08_r2_slice_executor_tests PRIVATE
    slicer_worker_slice_runtime
)
if(MSVC)
    target_compile_options(stage14d08_r2_slice_executor_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r3_obj_writer_tests
    tests/stage14d_08_r3/DeterministicObjAssetWriterTests.cpp
)
target_include_directories(stage14d08_r3_obj_writer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d08_r3_obj_writer_tests PRIVATE slicer_engine)
if(MSVC)
    target_compile_options(stage14d08_r3_obj_writer_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r3_strict_recheck_tests
    tests/stage14d_08_r3/StrictRepairAssetRecheckTests.cpp
)
target_include_directories(stage14d08_r3_strict_recheck_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(stage14d08_r3_strict_recheck_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(stage14d08_r3_strict_recheck_tests PRIVATE slicer_engine)
if(MSVC)
    target_compile_options(stage14d08_r3_strict_recheck_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r3_repair_facade_tests
    tests/stage14d_08_r3/ProductionRepairFacadeTests.cpp
)
target_include_directories(stage14d08_r3_repair_facade_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(stage14d08_r3_repair_facade_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(stage14d08_r3_repair_facade_tests PRIVATE slicer_engine)
if(MSVC)
    target_compile_options(stage14d08_r3_repair_facade_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r3_worker_repair_tests
    tests/stage14d_08_r3/WorkerRepairExecutorTests.cpp
)
target_include_directories(stage14d08_r3_worker_repair_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(stage14d08_r3_worker_repair_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(stage14d08_r3_worker_repair_tests PRIVATE
    slicer_worker_repair_runtime
)
if(MSVC)
    target_compile_options(stage14d08_r3_worker_repair_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d05_artifact_safety_tests
    tests/stage14d_05/PackageArtifactSafetyTests.cpp
)
target_include_directories(stage14d05_artifact_safety_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d05_artifact_safety_tests PRIVATE slicer_base)
if(MSVC)
    target_compile_options(stage14d05_artifact_safety_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d05_real_worker_artifact_tests
    tests/stage14d_05/RealWorkerArtifactRecoveryTests.cpp
    src/slicer_module/WorkerClient.cpp
    src/slicer_module/WorkerClient.h
    src/slicer_module/WorkerProcessWindows.cpp
    src/slicer_module/WorkerProcessWindows.h
    src/slicer_module/WorkerProtocol.cpp
    src/slicer_module/WorkerProtocol.h
)
target_include_directories(stage14d05_real_worker_artifact_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(stage14d05_real_worker_artifact_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(stage14d05_real_worker_artifact_tests PRIVATE
    slicer_engine
)
if(MSVC)
    target_compile_options(stage14d05_real_worker_artifact_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d07_engine_conformance_gate
    tests/stage14d_07/EngineConformanceGate.cpp
    tests/stage14d_07/EngineConformanceSupport.cpp
    tests/stage14d_07/EngineConformanceSupport.h
    src/slicer_module/WorkerClient.cpp
    src/slicer_module/WorkerClient.h
    src/slicer_module/WorkerProcessWindows.cpp
    src/slicer_module/WorkerProcessWindows.h
    src/slicer_module/WorkerProtocol.cpp
    src/slicer_module/WorkerProtocol.h
)
target_include_directories(stage14d07_engine_conformance_gate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d07_engine_conformance_gate PRIVATE
    slicer_engine
)
if(MSVC)
    target_compile_options(stage14d07_engine_conformance_gate PRIVATE /W4 /WX)
endif()

add_executable(slicer_worker
    apps/slicer_worker/Main.cpp
    apps/slicer_worker/WorkerApplication.cpp
    apps/slicer_worker/WorkerApplication.h
)
target_include_directories(slicer_worker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/slicer_worker
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(slicer_worker PRIVATE
    slicer_engine
    slicer_worker_runtime
    slicer_worker_preflight_runtime
    slicer_worker_repair_runtime
    slicer_worker_slice_runtime
)
add_dependencies(stage14d05_real_worker_artifact_tests slicer_worker)
add_dependencies(stage14d07_engine_conformance_gate slicer_worker)
add_dependencies(slicer_module slicer_worker)
add_dependencies(stage14d06_public_worker_routing_tests slicer_worker)

add_executable(slicer_host_sim
    apps/slicer_host_sim/Main.c
    apps/slicer_host_sim/HostFlowEndToEnd.c
    apps/slicer_host_sim/HostFlowEndToEnd.h
    apps/slicer_host_sim/HostM1Intake.c
    apps/slicer_host_sim/HostM1Intake.h
    apps/slicer_host_sim/HostModuleApi.c
    apps/slicer_host_sim/HostModuleApi.h
    apps/slicer_host_sim/HostMaterialProfile.c
    apps/slicer_host_sim/HostMaterialProfile.h
    apps/slicer_host_sim/HostPath.c
    apps/slicer_host_sim/HostPath.h
    apps/slicer_host_sim/HostRequestBuilder.c
    apps/slicer_host_sim/HostRequestBuilder.h
    apps/slicer_host_sim/HostTextureProfile.c
    apps/slicer_host_sim/HostTextureProfile.h
    apps/slicer_host_sim/JsonText.c
    apps/slicer_host_sim/JsonText.h
)
set_target_properties(slicer_host_sim PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_include_directories(slicer_host_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/slicer_host_sim
)
target_link_libraries(slicer_host_sim PRIVATE bcrypt)
add_dependencies(slicer_host_sim slicer_module slicer_worker)
if(MSVC)
    target_compile_options(slicer_host_sim PRIVATE /W4 /WX)
endif()

add_executable(texture_fill_partition_release_benchmark
    apps/texture_fill_partition_release_benchmark/main.cpp
)

target_link_libraries(texture_fill_partition_release_benchmark PRIVATE slicer_core)

add_executable(tiff_writer_benchmark
    apps/tiff_writer_benchmark/Main.cpp
)

target_link_libraries(tiff_writer_benchmark PRIVATE slicer_core)

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

add_executable(texture_white_carrier_policy_unit_tests
    tests/unit/texture_white_carrier_policy/Main.cpp
)

target_link_libraries(texture_white_carrier_policy_unit_tests PRIVATE slicer_core)

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

add_executable(tiff_writer_contract_unit_tests
    tests/unit/tiff_writer_contract/Main.cpp
)

target_link_libraries(tiff_writer_contract_unit_tests PRIVATE slicer_core)

add_executable(tiff_backend_build_info_unit_tests
    tests/unit/tiff_backend_build_info/Main.cpp
)

target_link_libraries(tiff_backend_build_info_unit_tests PRIVATE slicer_core)
target_compile_definitions(
    tiff_backend_build_info_unit_tests
    PRIVATE
        SLICESOFT_TEST_EXPECTED_TIFF_BACKEND="${SLICESOFT_TIFF_BACKEND}"
)

add_executable(tiff_writer_backend_unit_tests
    tests/unit/tiff_writer_backend/Main.cpp
)

target_link_libraries(tiff_writer_backend_unit_tests PRIVATE slicer_core)

add_executable(tiff_writer_equivalence_unit_tests
    tests/unit/tiff_writer_equivalence/Main.cpp
)

target_link_libraries(tiff_writer_equivalence_unit_tests PRIVATE slicer_core)

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

add_executable(stage14d08_r3_scene_preflight_tests
    tests/stage14d_08_r3/SceneFullPreflightServiceTests.cpp
)

target_link_libraries(stage14d08_r3_scene_preflight_tests PRIVATE slicer_engine)
target_compile_definitions(stage14d08_r3_scene_preflight_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
if(MSVC)
    target_compile_options(stage14d08_r3_scene_preflight_tests PRIVATE /W4 /WX)
endif()

add_executable(stage14d08_r3_worker_preflight_tests
    tests/stage14d_08_r3/WorkerPreflightExecutorTests.cpp
)
target_include_directories(stage14d08_r3_worker_preflight_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(stage14d08_r3_worker_preflight_tests PRIVATE
    slicer_worker_preflight_runtime
)
target_compile_definitions(stage14d08_r3_worker_preflight_tests PRIVATE
    SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
if(MSVC)
    target_compile_options(stage14d08_r3_worker_preflight_tests PRIVATE /W4 /WX)
endif()

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

find_package(Python3 QUIET COMPONENTS Interpreter)

enable_testing()
add_test(
    NAME print_module_spi_contract_declaration_test
    COMMAND ${CMAKE_COMMAND}
        "-DCONTRACT_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/contracts/print_module_spi.h"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/VerifyPrintModuleSpiContract.cmake"
)
add_test(
    NAME slicer_error_codes_contract_test
    COMMAND ${CMAKE_COMMAND}
        "-DERROR_CODE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/slicer_error_codes.json"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/VerifySlicerErrorCodes.cmake"
)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME json_schema_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateJsonSchemas.py"
            --repo-root
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME file_contract_v1_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateFileContract.py"
    )
    add_test(
        NAME slicer_capability_dto_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateCapabilityDtos.py"
    )
    add_test(
        NAME slicer_three_lane_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateThreeLaneContract.py"
    )
    add_test(
        NAME slicer_cancel_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateCancelContract.py"
    )
    add_test(
        NAME slicer_stage14b_preparation_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14BPreparation.py"
    )
    add_test(
        NAME slicer_stage14b_layering_feasibility_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14BLayeringFeasibility.py"
    )
    add_test(
        NAME slicer_stage14b_target_graph_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14BTargetGraph.py"
            --assignment
            "${SLICESOFT_LAYER_ASSIGNMENT_FILE}"
    )
    add_test(
        NAME slicer_facade_header_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateFacadeHeaders.py"
    )
    add_test(
        NAME slicer_stage14b_facade_dto_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14BFacadeDtos.py"
    )
    add_test(
        NAME slicer_stage14b04_slice_facade_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14B04SliceFacade.py"
    )
    add_test(
        NAME slicer_stage14b05_cli_facade_route_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14BCliFacadeRoute.py"
    )
    add_test(
        NAME slicer_stage14c01_module_shell_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14C01ModuleShell.py"
    )
    add_test(
        NAME slicer_stage14c04_sync_capability_wiring_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14c_04/ValidateSyncCapabilityWiring.py"
            --library $<TARGET_FILE:slicer_module>
            --repo "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME slicer_stage14c04_sync_capability_safety_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14c_04/ValidateSyncCapabilitySafety.py"
            --library $<TARGET_FILE:slicer_module>
            --repo "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME slicer_stage14c05_module_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14c_05/ValidateModuleInfoContracts.py"
            --repo "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME slicer_stage14c07_dll_initialization_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14c_07/ValidateDllInitializationContracts.py"
            --repo "${CMAKE_CURRENT_SOURCE_DIR}"
            --library $<TARGET_FILE:slicer_module>
    )
    add_test(
        NAME slicer_stage14d01_worker_shell_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateStage14D01WorkerShell.py"
            --worker $<TARGET_FILE:slicer_worker>
    )
    add_test(
        NAME slicer_stage14d04a_cancel_propagation_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14d_04a/ValidateCancelTokenPropagation.py"
    )
    add_test(
        NAME slicer_stage14d08_r1_worker_runtime_boundary_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14d_08_r1/ValidateWorkerRuntimeBoundaries.py"
            --repo-root "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME slicer_stage14d08_r1_no_fake_worker_executor_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14d_08_r1/ValidateNoFakeWorkerExecutor.py"
            --worker $<TARGET_FILE:slicer_worker>
    )
    add_test(
        NAME slicer_stage14d07_r1_conformance_definition_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14d_07/ValidateEngineConformanceDefinition.py"
            --repo-root "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME slicer_stage14e01_pure_c_host_boundary_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/stage14e_01/ValidatePureCHost.py"
            --repo-root "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(
        NAME slicer_source_size_guard_self_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/ValidateSourceSizeGuard.py"
            --self-test
    )
    add_test(
        NAME third_party_notice_contract_test
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/contracts/ValidateThirdPartyNotices.py"
    )
    set_tests_properties(
        json_schema_contract_test
        file_contract_v1_test
        slicer_capability_dto_contract_test
        slicer_three_lane_contract_test
        slicer_cancel_contract_test
        slicer_stage14b_preparation_contract_test
        slicer_stage14b_layering_feasibility_test
        slicer_stage14b_target_graph_test
        slicer_facade_header_contract_test
        slicer_stage14b04_slice_facade_contract_test
        slicer_stage14b05_cli_facade_route_contract_test
        slicer_stage14c01_module_shell_contract_test
        slicer_stage14c04_sync_capability_wiring_test
        slicer_stage14c04_sync_capability_safety_test
        slicer_stage14c05_module_contract_test
        slicer_stage14d01_worker_shell_contract_test
        slicer_stage14d04a_cancel_propagation_test
        slicer_stage14d08_r1_worker_runtime_boundary_test
        slicer_stage14d08_r1_no_fake_worker_executor_test
        slicer_stage14e01_pure_c_host_boundary_test
        slicer_source_size_guard_self_test
        third_party_notice_contract_test
        PROPERTIES
            ENVIRONMENT "PYTHONUTF8=1"
    )
else()
    message(WARNING "Python 3 was not found; json_schema_contract_test is disabled.")
endif()
add_test(
    NAME model_import_layering_probe
    COMMAND model_import_layering_probe
        "${CMAKE_CURRENT_SOURCE_DIR}/samples/models/textured/fixtures/policy_textured_small.obj"
)
add_test(
    NAME model_import_normals_probe
    COMMAND model_import_layering_probe
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stage14b/model_with_normals.obj"
        --expect-normals
)
add_test(NAME facade_contract_unit_tests COMMAND facade_contract_unit_tests)
add_test(
    NAME hostflow_ha02_scene_lifecycle_tests
    COMMAND hostflow_ha02_scene_lifecycle_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stage14b/model_with_normals.obj"
)
add_test(
    NAME hostflow_ha04_grid_layout_tests
    COMMAND hostflow_ha04_grid_layout_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stage14b/model_with_normals.obj"
)
add_test(NAME stage14c02_buffer_api_unit_tests COMMAND stage14c02_buffer_api_unit_tests)
add_test(NAME stage14c03_handle_registry_unit_tests COMMAND stage14c03_handle_registry_unit_tests)
add_test(NAME stage14c03_module_abi_tests COMMAND stage14c03_module_abi_tests)
add_test(NAME stage14c05_module_info_tests COMMAND stage14c05_module_info_tests)
add_test(NAME stage14c07_dll_initialization_tests COMMAND stage14c07_dll_initialization_tests)
add_test(
    NAME stage14c06_spi_conformance
    COMMAND test_spi_conformance
        $<TARGET_FILE:slicer_module>
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_test(NAME stage14d02_worker_client_unit_tests COMMAND stage14d02_worker_client_unit_tests)
add_test(
    NAME stage14d03_worker_contract_unit_tests
    COMMAND stage14d03_worker_contract_unit_tests $<TARGET_FILE:slicer_worker>
)
add_test(
    NAME stage14d04a_cancellation_unit_tests
    COMMAND stage14d04a_cancellation_unit_tests
)
add_test(
    NAME stage14d05_real_worker_artifact_tests
    COMMAND stage14d05_real_worker_artifact_tests $<TARGET_FILE:slicer_worker>
)
add_test(
    NAME slicer_stage14d07_r2_engine_conformance_test
    COMMAND stage14d07_engine_conformance_gate
        $<TARGET_FILE:slicer_worker>
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/stage14d07_evidence/$<CONFIG>"
)
add_test(
    NAME stage14d08_r1_worker_runtime_tests
    COMMAND stage14d08_r1_worker_runtime_tests
)
add_test(
    NAME stage14d08_r1_result_writer_tests
    COMMAND stage14d08_r1_result_writer_tests
)
add_test(
    NAME stage14d08_r1_dispatcher_tests
    COMMAND stage14d08_r1_dispatcher_tests
)
add_test(
    NAME stage14d08_r2_slice_materializer_tests
    COMMAND stage14d08_r2_slice_materializer_tests
)
add_test(
    NAME stage14d08_r2_slice_executor_tests
    COMMAND stage14d08_r2_slice_executor_tests
)
add_test(
    NAME stage14e01_c_host_end_to_end
    COMMAND slicer_host_sim
        $<TARGET_FILE:slicer_module>
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/stage14e01_evidence/$<CONFIG>"
)
add_test(
    NAME hostflow_ha03_c_end_to_end
    COMMAND slicer_host_sim
        $<TARGET_FILE:slicer_module>
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/hostflow_ha03_c_evidence/$<CONFIG>"
)
add_test(
    NAME stage14f02_m1_intake_self_test
    COMMAND slicer_host_sim
        --m1-self-test
        $<TARGET_FILE:slicer_module>
)
add_test(
    NAME stage14f03_single_model_s1_gate
    COMMAND powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/Run14F03SingleModelS1Gate.ps1"
        -BuildDirectory "${CMAKE_CURRENT_BINARY_DIR}"
        -Config $<CONFIG>
        -RepositoryRoot "${CMAKE_CURRENT_SOURCE_DIR}"
        -OutputRoot "${CMAKE_CURRENT_BINARY_DIR}/stage14f03_evidence/$<CONFIG>"
)
add_test(
    NAME stage14f04_s2_contract_gate
    COMMAND powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/Run14F04S2ContractGate.ps1"
        -RepositoryRoot "${CMAKE_CURRENT_SOURCE_DIR}"
        -OutputRoot "${CMAKE_CURRENT_BINARY_DIR}/stage14f04_evidence/$<CONFIG>"
)
add_test(
    NAME stage14f05_local_closure_gate
    COMMAND powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/Run14F05StageClosureGate.ps1"
        -BuildDirectory "${CMAKE_CURRENT_BINARY_DIR}"
        -Config $<CONFIG>
        -RepositoryRoot "${CMAKE_CURRENT_SOURCE_DIR}"
        -OutputRoot "${CMAKE_CURRENT_BINARY_DIR}/stage14f05_evidence/$<CONFIG>"
)
set_tests_properties(
    stage14e01_c_host_end_to_end
    hostflow_ha03_c_end_to_end
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 90
)
set_tests_properties(
    stage14f02_m1_intake_self_test
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 30
)
set_tests_properties(
    stage14f03_single_model_s1_gate
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 120
)
set_tests_properties(
    stage14f04_s2_contract_gate
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 60
)
set_tests_properties(
    stage14f05_local_closure_gate
    PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 300
)
add_test(NAME scene_facade_14b03_unit_tests COMMAND scene_facade_14b03_unit_tests)
add_test(
    NAME textured_scene_viewdata_14b03a_unit_tests
    COMMAND textured_scene_viewdata_14b03a_unit_tests
)
add_test(
    NAME textured_scene_viewdata_14b03a_real_fixture_tests
    COMMAND textured_scene_viewdata_14b03a_real_fixture_tests
)
add_test(
    NAME package_query_facade_14b02_unit_tests
    COMMAND package_query_facade_14b02_unit_tests
)
add_test(
    NAME package_query_facade_14b02_base_link_test
    COMMAND package_query_facade_14b02_base_link_test
)
add_test(NAME slice_facade_14b04_unit_tests COMMAND slice_facade_14b04_unit_tests)
add_test(
    NAME slice_facade_14b04_production_binding_test
    COMMAND slice_facade_14b04_production_binding_test
)
add_test(NAME production_admission_policy_unit_tests COMMAND production_admission_policy_unit_tests)
add_test(NAME experimental_config_unit_tests COMMAND experimental_config_unit_tests)
add_test(NAME texture_white_carrier_policy_unit_tests COMMAND texture_white_carrier_policy_unit_tests)
add_test(NAME output_resolution_config_unit_tests COMMAND output_resolution_config_unit_tests)
add_test(NAME preview_output_policy_unit_tests COMMAND preview_output_policy_unit_tests)
add_test(NAME rip_reader_resolution_unit_tests COMMAND rip_reader_resolution_unit_tests)
add_test(NAME tiff_layer_source_unit_tests COMMAND tiff_layer_source_unit_tests)
add_test(NAME tiff_writer_contract_unit_tests COMMAND tiff_writer_contract_unit_tests)
add_test(NAME tiff_backend_build_info_unit_tests COMMAND tiff_backend_build_info_unit_tests)
add_test(NAME tiff_writer_backend_unit_tests COMMAND tiff_writer_backend_unit_tests)
add_test(NAME tiff_writer_equivalence_unit_tests COMMAND tiff_writer_equivalence_unit_tests)
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
add_test(
    NAME stage14d08_r3_scene_preflight_tests
    COMMAND stage14d08_r3_scene_preflight_tests
)
add_test(
    NAME stage14d08_r3_worker_preflight_tests
    COMMAND stage14d08_r3_worker_preflight_tests
)
add_test(
    NAME stage14d08_r3_worker_repair_tests
    COMMAND stage14d08_r3_worker_repair_tests
)
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
add_test(
    NAME stage14d06_capability_carrier_tests
    COMMAND stage14d06_capability_carrier_tests
)
add_test(
    NAME stage14d06_public_worker_routing_tests
    COMMAND stage14d06_public_worker_routing_tests
)
add_test(
    NAME stage14d05_r4b_public_worker_artifact_tests
    COMMAND stage14d06_public_worker_routing_tests
)
add_test(
    NAME stage14d04b_public_worker_cancellation_tests
    COMMAND stage14d06_public_worker_routing_tests
)

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

if(BUILD_SLICER_DEBUG_UI OR BUILD_SLICER_UI_HOST_SIM)
    find_package(Qt5 COMPONENTS Widgets QUIET)
endif()

if(BUILD_SLICER_DEBUG_UI)
    if(Qt5Widgets_FOUND)
        add_subdirectory(apps/slicer_debug_ui)
    else()
        message(STATUS "Qt5 Widgets not found; slicer_debug_ui target is skipped.")
    endif()
endif()

if(BUILD_SLICER_UI_HOST_SIM)
    if(Qt5Widgets_FOUND)
        add_subdirectory(apps/slicer_ui_host_sim)
    else()
        message(STATUS "Qt5 Widgets not found; slicer_ui_host_sim target is skipped.")
    endif()
endif()

if(TARGET slicer_debug_ui AND TARGET slicer_ui_host_sim)
    add_executable(stage14e04_render_benchmark
        tests/stage14e_04/Stage14E04RenderBenchmark.cpp
        apps/slicer_ui_host_sim/ModuleClient.cpp
        apps/slicer_ui_host_sim/ModuleClient.h
        apps/slicer_ui_host_sim/render/TopViewRenderPolicy.cpp
        apps/slicer_ui_host_sim/render/TopViewRenderPolicyData.cpp
        apps/slicer_ui_host_sim/render/TopViewRenderPolicy.h
        apps/slicer_debug_ui/models/SceneDocument.cpp
        apps/slicer_debug_ui/models/SceneDocument.h
        apps/slicer_debug_ui/models/SceneSelectionModel.cpp
        apps/slicer_debug_ui/models/SceneSelectionModel.h
        apps/slicer_debug_ui/widgets/ModelTopViewWidget.cpp
        apps/slicer_debug_ui/widgets/ModelTopViewWidget.h
    )

    set_target_properties(stage14e04_render_benchmark PROPERTIES
        AUTOMOC ON
    )

    target_include_directories(stage14e04_render_benchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(stage14e04_render_benchmark PRIVATE
        SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(stage14e04_render_benchmark PRIVATE
        Qt5::Widgets
        slicer_core
    )

    if(MSVC)
        target_compile_options(stage14e04_render_benchmark PRIVATE
            /MP
            /W4
            /WX
            /utf-8
        )

        if(MSVC_VERSION GREATER_EQUAL 1950)
            set(stage14e04CompatibilityHeader
                "${CMAKE_CURRENT_SOURCE_DIR}/apps/slicer_ui_host_sim/Qt515MsvcCompatibility.h"
            )
            target_sources(stage14e04_render_benchmark PRIVATE
                ${stage14e04CompatibilityHeader}
            )
            target_compile_options(stage14e04_render_benchmark PRIVATE
                "/FI${stage14e04CompatibilityHeader}"
            )
        endif()
    endif()

    add_test(
        NAME slicer_stage14e04_render_benchmark_smoke
        COMMAND stage14e04_render_benchmark --seconds 1
    )
    set_tests_properties(
        slicer_stage14e04_render_benchmark_smoke
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

if(TARGET slicer_debug_ui)
    add_custom_target(slicesoft_runtime
        DEPENDS
            slicer_cli
            rip_reader_test
            slicer_debug_ui
    )
endif()
