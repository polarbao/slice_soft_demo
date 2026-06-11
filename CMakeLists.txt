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
    src/slicer_core/geometry/DistanceField2D.cpp
    src/slicer_core/geometry/DistanceField2D.h
    src/slicer_core/geometry/GeometryKernelReport.cpp
    src/slicer_core/geometry/GeometryKernelReport.h
    src/slicer_core/geometry/GeometryKernelTypes.h
    src/slicer_core/geometry/OpenVdbAdapter.cpp
    src/slicer_core/geometry/OpenVdbAdapter.h
    src/slicer_core/geometry/ShellMask.cpp
    src/slicer_core/geometry/ShellMask.h
    src/slicer_core/importers/mtl/MtlImporter.cpp
    src/slicer_core/importers/mtl/MtlImporter.h
    src/slicer_core/importers/obj/ObjImporter.cpp
    src/slicer_core/importers/obj/ObjImporter.h
    src/slicer_core/importers/three_mf/ThreeMfImporter.cpp
    src/slicer_core/importers/three_mf/ThreeMfImporter.h
    src/slicer_core/json_value.cpp
    src/slicer_core/json_value.h
    src/slicer_core/materials/material_policy/MaterialPolicy.cpp
    src/slicer_core/materials/material_policy/MaterialPolicy.h
    src/slicer_core/materials/process_profile/MaterialProcessProfile.cpp
    src/slicer_core/materials/process_profile/MaterialProcessProfile.h
    src/slicer_core/materials/role_mapping/MaterialRoleMapping.cpp
    src/slicer_core/materials/role_mapping/MaterialRoleMapping.h
    src/slicer_core/materials/texture_application/TextureApplicationPolicy.cpp
    src/slicer_core/materials/texture_application/TextureApplicationPolicy.h
    src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.cpp
    src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.h
    src/slicer_core/model.cpp
    src/slicer_core/model.h
    src/slicer_core/output/rgbwsv/RgbwsvPackage.cpp
    src/slicer_core/output/rgbwsv/RgbwsvPackage.h
    src/slicer_core/pipeline/PipelineContext.cpp
    src/slicer_core/pipeline/PipelineContext.h
    src/slicer_core/pipeline/PipelineStepResult.cpp
    src/slicer_core/pipeline/PipelineStepResult.h
    src/slicer_core/pipeline/SlicePipeline.cpp
    src/slicer_core/pipeline/SlicePipeline.h
    src/slicer_core/raster/RasterBoundary.cpp
    src/slicer_core/raster/RasterBoundary.h
    src/slicer_core/reports/ReportBase.cpp
    src/slicer_core/reports/ReportBase.h
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
    src/slicer_core/texture_image.cpp
    src/slicer_core/texture_image.h
    src/slicer_core/tiff_io.cpp
    src/slicer_core/tiff_io.h
)

target_include_directories(slicer_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniz
)

if(WIN32)
    target_link_libraries(slicer_core PRIVATE windowscodecs ole32)
endif()

if(USE_OPENVDB)
    find_package(OpenVDB CONFIG REQUIRED)
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

add_executable(rip_reader_test
    apps/rip_reader_test/main.cpp
)

target_link_libraries(rip_reader_test PRIVATE slicer_core)

add_executable(support_shape_unit_tests
    tests/unit/support_shape/main.cpp
)

target_link_libraries(support_shape_unit_tests PRIVATE slicer_core)

if(ENABLE_GEOMETRY_KERNEL_DEMO)
    add_executable(geometry_kernel_demo
        apps/geometry_kernel_demo/main.cpp
    )

    target_link_libraries(geometry_kernel_demo PRIVATE slicer_core)
endif()

if(BUILD_SLICER_DEBUG_UI)
    find_package(Qt5 COMPONENTS Widgets QUIET)
    if(Qt5Widgets_FOUND)
        add_subdirectory(apps/slicer_debug_ui)
    else()
        message(STATUS "Qt5 Widgets not found; slicer_debug_ui target is skipped.")
    endif()
endif()
