#include "ProductionTextureSettingsModel.h"

#include <QJsonObject>

#include <cmath>
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

QJsonObject LegacyConfig(const int topLayers, const double layerThicknessMm)
{
    return QJsonObject{
        {QStringLiteral("output"),
         QJsonObject{
             {QStringLiteral("layerThicknessMm"), layerThicknessMm}}},
        {QStringLiteral("texture"),
         QJsonObject{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("applyMode"), QStringLiteral("top_surface_band")},
             {QStringLiteral("topSurfaceLayers"), topLayers}}},
    };
}

QJsonObject GlobalConfig(
    const double widthMm,
    const QString& mode = QStringLiteral("partial_shell"))
{
    return QJsonObject{
        {QStringLiteral("texture"),
         QJsonObject{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("applyMode"), QStringLiteral("global_surface_shell")},
             {QStringLiteral("surfaceShell"),
              QJsonObject{
                  {QStringLiteral("widthMm"), widthMm},
                  {QStringLiteral("mode"), mode}}}}},
    };
}

bool TestReadsLegacyRequestedAndEffectiveValues()
{
    const ProductionTextureControlState state =
        ProductionTextureSettingsModel::Read(
            LegacyConfig(3, 0.038),
            true,
            true,
            false);

    return ExpectTrue(
               state.strategy == ProductionTextureStrategy::LegacyTopBand,
               "Legacy strategy detected")
        && ExpectTrue(state.valid, "Legacy state valid")
        && ExpectTrue(state.editable, "Legacy state editable")
        && ExpectTrue(
            state.requestedtoplayers == 3 && state.effectivetoplayers == 3,
            "Legacy requested and effective layers match")
        && ExpectTrue(
            std::abs(state.effectivetopthicknessmm - 0.114) < 1.0e-9,
            "Legacy effective Z thickness is derived from layer height")
        && ExpectTrue(
            state.backend == QStringLiteral("legacy_cpu_top_band"),
            "Legacy backend reported");
}

bool TestLegacyUpdateWritesOnlyOwnedFields()
{
    const QJsonObject original = LegacyConfig(1, 0.038);
    const ProductionTextureControlState current =
        ProductionTextureSettingsModel::Read(original, true, true, false);
    const ProductionTextureControlState updated =
        ProductionTextureSettingsModel::UpdateLegacyTopLayers(current, 5, 0.038);
    const ProductionTextureSettingsApplyResult result =
        ProductionTextureSettingsModel::Apply(original, updated);

    const QJsonObject texture =
        result.config.value(QStringLiteral("texture")).toObject();
    return ExpectTrue(result.applied, "Legacy update applied")
        && ExpectTrue(updated.stale, "Legacy update marks package stale")
        && ExpectTrue(
            updated.effectivetoplayers == 5
                && std::abs(updated.effectivetopthicknessmm - 0.190) < 1.0e-9,
            "Legacy effective values updated")
        && ExpectTrue(
            texture.value(QStringLiteral("applyMode")).toString()
                == QStringLiteral("top_surface_band"),
            "Legacy apply mode preserved")
        && ExpectTrue(
            texture.value(QStringLiteral("topSurfaceLayers")).toInt() == 5,
            "Legacy owned layer field written")
        && ExpectTrue(
            !texture.contains(QStringLiteral("surfaceShell")),
            "Legacy update does not create Global fields");
}

bool TestGlobalModeIsExplicitAndDoesNotUseLargeWidthSentinel()
{
    const QJsonObject original = GlobalConfig(0.40);
    const ProductionTextureControlState current =
        ProductionTextureSettingsModel::Read(original, true, true, false);
    const ProductionTextureControlState updated =
        ProductionTextureSettingsModel::UpdateGlobal(
            current,
            0.40,
            ProductionTexturePartitionMode::AllTexture);
    const ProductionTextureSettingsApplyResult result =
        ProductionTextureSettingsModel::Apply(original, updated);
    const QJsonObject surfaceShell =
        result.config.value(QStringLiteral("texture"))
            .toObject()
            .value(QStringLiteral("surfaceShell"))
            .toObject();

    return ExpectTrue(result.applied, "Global update applied")
        && ExpectTrue(
            updated.strategy == ProductionTextureStrategy::GlobalSurfaceShell,
            "Global strategy detected")
        && ExpectTrue(
            updated.partitionmode == ProductionTexturePartitionMode::AllTexture,
            "All-texture mode retained")
        && ExpectTrue(
            std::abs(surfaceShell.value(QStringLiteral("widthMm")).toDouble() - 0.40)
                < 1.0e-9,
            "All-texture does not replace width with a sentinel")
        && ExpectTrue(
            surfaceShell.value(QStringLiteral("mode")).toString()
                == QStringLiteral("all_texture"),
            "All-texture mode is serialized explicitly")
        && ExpectTrue(
            updated.backend == QStringLiteral("legacy_cpu_global_distance"),
            "Global backend reported");
}

bool TestInvalidAndLockedUpdatesFailClosed()
{
    const QJsonObject original = LegacyConfig(1, 0.038);
    ProductionTextureControlState current =
        ProductionTextureSettingsModel::Read(original, true, true, false);
    const ProductionTextureControlState invalid =
        ProductionTextureSettingsModel::UpdateLegacyTopLayers(current, 0, 0.038);
    const ProductionTextureSettingsApplyResult invalidResult =
        ProductionTextureSettingsModel::Apply(original, invalid);

    current.editable = false;
    current.lockreason = QStringLiteral("Profile locked");
    const ProductionTextureSettingsApplyResult lockedResult =
        ProductionTextureSettingsModel::Apply(original, current);

    return ExpectTrue(!invalid.valid, "Invalid Legacy value rejected")
        && ExpectTrue(!invalidResult.applied, "Invalid update is not applied")
        && ExpectTrue(
            invalidResult.config == original,
            "Invalid update preserves source config")
        && ExpectTrue(
            invalidResult.errorcode
                == QStringLiteral("E_PRODUCTION_TEXTURE_INVALID_TOP_LAYERS"),
            "Invalid Legacy error code stable")
        && ExpectTrue(!lockedResult.applied, "Locked update is not applied")
        && ExpectTrue(
            lockedResult.errorcode
                == QStringLiteral("E_PRODUCTION_TEXTURE_PROFILE_LOCKED"),
            "Locked Profile error code stable");
}

bool TestGlobalAdmissionFailsClosed()
{
    const ProductionTextureControlState state =
        ProductionTextureSettingsModel::Read(
            GlobalConfig(0.40),
            true,
            false,
            false);
    return ExpectTrue(!state.valid, "Unadmitted Global state invalid")
        && ExpectTrue(
            state.errorcode
                == ProductionTextureSettingsErrorCode::GlobalNotAdmitted,
            "Unadmitted Global error stable")
        && ExpectTrue(!state.editable, "Unadmitted Global state locked");
}

}  // namespace

int main()
{
    const bool passed = TestReadsLegacyRequestedAndEffectiveValues()
        && TestLegacyUpdateWritesOnlyOwnedFields()
        && TestGlobalModeIsExplicitAndDoesNotUseLargeWidthSentinel()
        && TestInvalidAndLockedUpdatesFailClosed()
        && TestGlobalAdmissionFailsClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS production_texture_settings_model_unit_tests\n";
    return 0;
}
