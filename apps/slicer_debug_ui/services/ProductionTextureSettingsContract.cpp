#include "ProductionTextureSettingsContract.h"

ProductionTextureFieldMapping ProductionTextureSettingsContract::FieldMapping(
    const ProductionTextureStrategy strategy)
{
    ProductionTextureFieldMapping mapping;
    switch (strategy)
    {
    case ProductionTextureStrategy::LegacyTopBand:
        mapping.applymodepath = QStringLiteral("texture.applyMode");
        mapping.requestedvaluepath =
            QStringLiteral("texture.topSurfaceLayers");
        mapping.layerthicknesspath =
            QStringLiteral("output.layerThicknessMm");
        mapping.requestedunit = QStringLiteral("layers");
        mapping.backend = QStringLiteral("legacy_cpu_top_band");
        mapping.production = true;
        break;
    case ProductionTextureStrategy::GlobalSurfaceShell:
        mapping.applymodepath = QStringLiteral("texture.applyMode");
        mapping.requestedvaluepath =
            QStringLiteral("texture.surfaceShell.widthMm");
        mapping.partitionmodepath =
            QStringLiteral("texture.surfaceShell.mode");
        mapping.requestedunit = QStringLiteral("mm_normal_distance");
        mapping.backend = QStringLiteral("legacy_cpu_global_distance");
        mapping.production = true;
        break;
    case ProductionTextureStrategy::DiagnosticOnly:
        mapping.requestedunit = QStringLiteral("mm_diagnostic_only");
        mapping.backend = QStringLiteral("diagnostic_only");
        break;
    case ProductionTextureStrategy::Unsupported:
    default:
        mapping.backend = QStringLiteral("unsupported");
        break;
    }
    return mapping;
}

QString ProductionTextureSettingsContract::StrategyValue(
    const ProductionTextureStrategy strategy)
{
    switch (strategy)
    {
    case ProductionTextureStrategy::LegacyTopBand:
        return QStringLiteral("legacy_top_band");
    case ProductionTextureStrategy::GlobalSurfaceShell:
        return QStringLiteral("global_surface_shell");
    case ProductionTextureStrategy::DiagnosticOnly:
        return QStringLiteral("diagnostic_only");
    case ProductionTextureStrategy::Unsupported:
    default:
        return QStringLiteral("unsupported");
    }
}

QString ProductionTextureSettingsContract::PartitionModeValue(
    const ProductionTexturePartitionMode mode)
{
    return mode == ProductionTexturePartitionMode::AllTexture
        ? QStringLiteral("all_texture")
        : QStringLiteral("partial_shell");
}

QString ProductionTextureSettingsContract::ErrorCodeValue(
    const ProductionTextureSettingsErrorCode code)
{
    switch (code)
    {
    case ProductionTextureSettingsErrorCode::UnsupportedStrategy:
        return QStringLiteral("E_PRODUCTION_TEXTURE_UNSUPPORTED_STRATEGY");
    case ProductionTextureSettingsErrorCode::InvalidLegacyTopLayers:
        return QStringLiteral("E_PRODUCTION_TEXTURE_INVALID_TOP_LAYERS");
    case ProductionTextureSettingsErrorCode::InvalidGlobalWidth:
        return QStringLiteral("E_PRODUCTION_TEXTURE_INVALID_WIDTH");
    case ProductionTextureSettingsErrorCode::InvalidPartitionMode:
        return QStringLiteral("E_PRODUCTION_TEXTURE_INVALID_PARTITION_MODE");
    case ProductionTextureSettingsErrorCode::ProfileLocked:
        return QStringLiteral("E_PRODUCTION_TEXTURE_PROFILE_LOCKED");
    case ProductionTextureSettingsErrorCode::GlobalNotAdmitted:
        return QStringLiteral("E_PRODUCTION_TEXTURE_GLOBAL_NOT_ADMITTED");
    case ProductionTextureSettingsErrorCode::None:
    default:
        return {};
    }
}
