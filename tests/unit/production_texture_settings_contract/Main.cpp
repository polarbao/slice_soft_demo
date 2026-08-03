#include "ProductionTextureSettingsContract.h"

#include <iostream>

namespace
{

bool ExpectTrue(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool TestLegacyFieldMappingFreezesZLayerSemantics()
{
    const ProductionTextureFieldMapping mapping =
        ProductionTextureSettingsContract::FieldMapping(
            ProductionTextureStrategy::LegacyTopBand);

    return ExpectTrue(mapping.production, "Legacy mapping is production")
        && ExpectTrue(
            mapping.applymodepath == QStringLiteral("texture.applyMode"),
            "Legacy apply mode path")
        && ExpectTrue(
            mapping.requestedvaluepath
                == QStringLiteral("texture.topSurfaceLayers"),
            "Legacy requested value path")
        && ExpectTrue(
            mapping.layerthicknesspath
                == QStringLiteral("output.layerThicknessMm"),
            "Legacy layer thickness path")
        && ExpectTrue(
            mapping.partitionmodepath.isEmpty(),
            "Legacy has no Global partition mode path")
        && ExpectTrue(
            mapping.requestedunit == QStringLiteral("layers"),
            "Legacy requested unit is layers")
        && ExpectTrue(
            mapping.backend == QStringLiteral("legacy_cpu_top_band"),
            "Legacy backend identity");
}

bool TestGlobalFieldMappingFreezesPhysicalShellSemantics()
{
    const ProductionTextureFieldMapping mapping =
        ProductionTextureSettingsContract::FieldMapping(
            ProductionTextureStrategy::GlobalSurfaceShell);

    return ExpectTrue(mapping.production, "Global mapping is production")
        && ExpectTrue(
            mapping.requestedvaluepath
                == QStringLiteral("texture.surfaceShell.widthMm"),
            "Global requested width path")
        && ExpectTrue(
            mapping.partitionmodepath
                == QStringLiteral("texture.surfaceShell.mode"),
            "Global explicit partition mode path")
        && ExpectTrue(
            mapping.layerthicknesspath.isEmpty(),
            "Global does not use Z layer thickness as shell width")
        && ExpectTrue(
            mapping.requestedunit == QStringLiteral("mm_normal_distance"),
            "Global requested unit is normal-distance mm")
        && ExpectTrue(
            mapping.backend
                == QStringLiteral("legacy_cpu_global_distance"),
            "Global backend identity");
}

bool TestDiagnosticMappingCannotWriteProductionConfig()
{
    const ProductionTextureFieldMapping mapping =
        ProductionTextureSettingsContract::FieldMapping(
            ProductionTextureStrategy::DiagnosticOnly);

    return ExpectTrue(!mapping.production, "Diagnostic mapping is read-only")
        && ExpectTrue(
            mapping.applymodepath.isEmpty(),
            "Diagnostic mapping has no production apply mode path")
        && ExpectTrue(
            mapping.requestedvaluepath.isEmpty(),
            "Diagnostic mapping has no production value path")
        && ExpectTrue(
            mapping.partitionmodepath.isEmpty(),
            "Diagnostic mapping has no production mode path")
        && ExpectTrue(
            mapping.backend == QStringLiteral("diagnostic_only"),
            "Diagnostic backend identity");
}

bool TestStableValuesAndErrorCodes()
{
    return ExpectTrue(
               ProductionTextureSettingsContract::StrategyValue(
                   ProductionTextureStrategy::LegacyTopBand)
                   == QStringLiteral("legacy_top_band"),
               "Legacy strategy value")
        && ExpectTrue(
            ProductionTextureSettingsContract::StrategyValue(
                ProductionTextureStrategy::GlobalSurfaceShell)
                == QStringLiteral("global_surface_shell"),
            "Global strategy value")
        && ExpectTrue(
            ProductionTextureSettingsContract::PartitionModeValue(
                ProductionTexturePartitionMode::PartialShell)
                == QStringLiteral("partial_shell"),
            "Partial-shell mode value")
        && ExpectTrue(
            ProductionTextureSettingsContract::PartitionModeValue(
                ProductionTexturePartitionMode::AllTexture)
                == QStringLiteral("all_texture"),
            "All-texture mode value")
        && ExpectTrue(
            ProductionTextureSettingsContract::ErrorCodeValue(
                ProductionTextureSettingsErrorCode::InvalidLegacyTopLayers)
                == QStringLiteral(
                    "E_PRODUCTION_TEXTURE_INVALID_TOP_LAYERS"),
            "Legacy layer error code")
        && ExpectTrue(
            ProductionTextureSettingsContract::ErrorCodeValue(
                ProductionTextureSettingsErrorCode::InvalidGlobalWidth)
                == QStringLiteral("E_PRODUCTION_TEXTURE_INVALID_WIDTH"),
            "Global width error code")
        && ExpectTrue(
            ProductionTextureSettingsContract::ErrorCodeValue(
                ProductionTextureSettingsErrorCode::GlobalNotAdmitted)
                == QStringLiteral("E_PRODUCTION_TEXTURE_GLOBAL_NOT_ADMITTED"),
            "Global admission error code");
}

bool TestControlStateDefaultsFailClosed()
{
    const ProductionTextureControlState state;
    return ExpectTrue(
               state.strategy == ProductionTextureStrategy::Unsupported,
               "Default strategy is unsupported")
        && ExpectTrue(!state.editable, "Default state is locked")
        && ExpectTrue(!state.valid, "Default state is invalid")
        && ExpectTrue(
            state.errorcode
                == ProductionTextureSettingsErrorCode::UnsupportedStrategy,
            "Default error is unsupported strategy")
        && ExpectTrue(
            state.requestedtoplayers == 1
                && state.effectivetoplayers == 1,
            "Legacy layer defaults are explicit")
        && ExpectTrue(
            state.requestedwidthmm == 0.0
                && state.effectivewidthmm == 0.0,
            "Global width defaults do not inherit diagnostics");
}

}  // namespace

int main()
{
    const bool passed = TestLegacyFieldMappingFreezesZLayerSemantics()
        && TestGlobalFieldMappingFreezesPhysicalShellSemantics()
        && TestDiagnosticMappingCannotWriteProductionConfig()
        && TestStableValuesAndErrorCodes()
        && TestControlStateDefaultsFailClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS production_texture_settings_contract_unit_tests\n";
    return 0;
}
