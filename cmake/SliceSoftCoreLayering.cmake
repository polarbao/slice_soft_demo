function(SliceSoftPartitionCoreSources allSourcesVar baseSourcesVar engineSourcesVar)
    set(baseSources)
    set(engineSources)

    set(basePrefixes
        "src/third_party/miniz/"
        "src/slicer_core/api/"
        "src/slicer_core/importers/"
        "src/slicer_core/layout/"
        "src/slicer_core/model/"
        "src/slicer_core/scene/"
    )
    set(baseExactStems
        "src/slicer_core/config/OutputResolution"
        "src/slicer_core/diagnostics/Diagnostics"
        "src/slicer_core/diagnostics/ProductionAdmissionPolicy"
        "src/slicer_core/diagnostics/ValidationIssue"
        "src/slicer_core/geometry/SceneModelTriangleMeshAdapter"
        "src/slicer_core/geometry/MeshTopologyDiagnostics"
        "src/slicer_core/geometry/TransformedModelAdapter"
        "src/slicer_core/geometry/TriangleMeshData"
        "src/slicer_core/json_value"
        "src/slicer_core/model"
        "src/slicer_core/output/rgbwsv/RgbwsvPackage"
        "src/slicer_core/output/rgbwsv/RgbwsvSceneExtension"
        "src/slicer_core/reports/ReportBase"
        "src/slicer_core/reports/ReportSchema"
        "src/slicer_core/reports/ReportSchemaValidator"
        "src/slicer_core/rip_reader"
        "src/slicer_core/system/Sha256"
        "src/slicer_core/system/Sha256Internal"
        "src/slicer_core/texture_image"
        "src/slicer_core/tiff_io"
    )
    set(engineExactSources
        "src/slicer_core/model/ModelLoadConfigAdapter.cpp"
        "src/slicer_core/scene/SceneEffectiveConfig.cpp"
        "src/slicer_core/scene/SceneEffectiveConfig.h"
        "src/slicer_core/tiff_io.cpp"
    )

    foreach(source IN LISTS ${allSourcesVar})
        set(isBase false)
        string(REGEX REPLACE "\\.[^.]+$" "" sourceStem "${source}")
        foreach(prefix IN LISTS basePrefixes)
            string(FIND "${source}" "${prefix}" prefixPosition)
            if(prefixPosition EQUAL 0)
                set(isBase true)
            endif()
        endforeach()
        if(sourceStem IN_LIST baseExactStems)
            set(isBase true)
        endif()
        if(source IN_LIST engineExactSources)
            set(isBase false)
        endif()

        if(isBase)
            list(APPEND baseSources "${source}")
        else()
            list(APPEND engineSources "${source}")
        endif()
    endforeach()

    set(${baseSourcesVar} "${baseSources}" PARENT_SCOPE)
    set(${engineSourcesVar} "${engineSources}" PARENT_SCOPE)
endfunction()
