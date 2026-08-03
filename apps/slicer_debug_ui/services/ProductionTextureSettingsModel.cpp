#include "ProductionTextureSettingsModel.h"

#include <cmath>

namespace
{

constexpr double kGlobalWidthStepMm{0.01};

void SetInvalid(
    ProductionTextureControlState& state,
    const ProductionTextureSettingsErrorCode errorCode,
    const QString& issue)
{
    state.valid = false;
    state.errorcode = errorCode;
    state.issues.clear();
    state.issues.push_back(issue);
}

void SetValid(ProductionTextureControlState& state)
{
    state.valid = true;
    state.errorcode = ProductionTextureSettingsErrorCode::None;
    state.issues.clear();
}

ProductionTexturePartitionMode ParsePartitionMode(
    const QString& value,
    bool& valid)
{
    if (value.isEmpty() || value == QStringLiteral("partial_shell"))
    {
        valid = true;
        return ProductionTexturePartitionMode::PartialShell;
    }
    if (value == QStringLiteral("all_texture"))
    {
        valid = true;
        return ProductionTexturePartitionMode::AllTexture;
    }
    valid = false;
    return ProductionTexturePartitionMode::PartialShell;
}

double QuantizeGlobalWidth(const double requestedWidthMm)
{
    return std::round(requestedWidthMm / kGlobalWidthStepMm)
        * kGlobalWidthStepMm;
}

ProductionTextureSettingsApplyResult FailureResult(
    const QJsonObject& config,
    const ProductionTextureControlState& state,
    const ProductionTextureSettingsErrorCode errorCode,
    const QString& issue)
{
    ProductionTextureSettingsApplyResult result;
    result.config = config;
    result.state = state;
    result.state.valid = false;
    result.state.errorcode = errorCode;
    result.errorcode =
        ProductionTextureSettingsContract::ErrorCodeValue(errorCode);
    result.issues.push_back(issue);
    return result;
}

}  // namespace

ProductionTextureControlState ProductionTextureSettingsModel::Read(
    const QJsonObject& config,
    const bool editable,
    const bool globalAdmitted,
    const bool stale)
{
    ProductionTextureControlState state;
    state.editable = editable;
    state.stale = stale;

    const QJsonObject texture =
        config.value(QStringLiteral("texture")).toObject();
    const QString applyMode =
        texture.value(QStringLiteral("applyMode")).toString();
    if (!texture.value(QStringLiteral("enabled")).toBool(false)
        || applyMode.isEmpty())
    {
        SetInvalid(
            state,
            ProductionTextureSettingsErrorCode::UnsupportedStrategy,
            QStringLiteral("当前 Profile 未启用可编辑的生产纹理策略。"));
        state.editable = false;
        state.backend = QStringLiteral("unsupported");
        return state;
    }

    if (applyMode == QStringLiteral("top_surface_band"))
    {
        state.strategy = ProductionTextureStrategy::LegacyTopBand;
        state.backend =
            ProductionTextureSettingsContract::FieldMapping(state.strategy)
                .backend;
        state.requestedtoplayers =
            texture.value(QStringLiteral("topSurfaceLayers")).toInt(1);
        state.effectivetoplayers = state.requestedtoplayers;
        const double layerThicknessMm =
            config.value(QStringLiteral("output"))
                .toObject()
                .value(QStringLiteral("layerThicknessMm"))
                .toDouble(0.0);
        state.effectivetopthicknessmm =
            static_cast<double>(state.effectivetoplayers)
            * layerThicknessMm;
        if (state.requestedtoplayers <= 0
            || !std::isfinite(layerThicknessMm)
            || layerThicknessMm <= 0.0)
        {
            SetInvalid(
                state,
                ProductionTextureSettingsErrorCode::InvalidLegacyTopLayers,
                QStringLiteral("Legacy 顶面纹理层数和层高必须为正值。"));
            return state;
        }
        SetValid(state);
        return state;
    }

    if (applyMode == QStringLiteral("global_surface_shell"))
    {
        state.strategy = ProductionTextureStrategy::GlobalSurfaceShell;
        state.backend =
            ProductionTextureSettingsContract::FieldMapping(state.strategy)
                .backend;
        const QJsonObject surfaceShell =
            texture.value(QStringLiteral("surfaceShell")).toObject();
        state.requestedwidthmm =
            surfaceShell.value(QStringLiteral("widthMm")).toDouble(0.0);
        state.effectivewidthmm = QuantizeGlobalWidth(state.requestedwidthmm);
        bool partitionModeValid{false};
        state.partitionmode = ParsePartitionMode(
            surfaceShell.value(QStringLiteral("mode")).toString(),
            partitionModeValid);
        if (!globalAdmitted)
        {
            SetInvalid(
                state,
                ProductionTextureSettingsErrorCode::GlobalNotAdmitted,
                QStringLiteral("当前模型或 Profile 未通过 Global production admission。"));
            state.editable = false;
            return state;
        }
        if (!partitionModeValid)
        {
            SetInvalid(
                state,
                ProductionTextureSettingsErrorCode::InvalidPartitionMode,
                QStringLiteral("Global 纹理分区模式必须为 partial_shell 或 all_texture。"));
            return state;
        }
        if (!std::isfinite(state.requestedwidthmm)
            || state.requestedwidthmm <= 0.0
            || state.effectivewidthmm <= 0.0)
        {
            SetInvalid(
                state,
                ProductionTextureSettingsErrorCode::InvalidGlobalWidth,
                QStringLiteral("Global 纹理壳层宽度必须为正的有限毫米值。"));
            return state;
        }
        SetValid(state);
        return state;
    }

    SetInvalid(
        state,
        ProductionTextureSettingsErrorCode::UnsupportedStrategy,
        QStringLiteral("当前纹理 applyMode 不属于 12E-09D 生产设置。"));
    state.editable = false;
    state.backend = QStringLiteral("unsupported");
    return state;
}

ProductionTextureControlState
ProductionTextureSettingsModel::UpdateLegacyTopLayers(
    const ProductionTextureControlState& current,
    const int requestedTopLayers,
    const double layerThicknessMm)
{
    ProductionTextureControlState state = current;
    state.requestedtoplayers = requestedTopLayers;
    state.effectivetoplayers = requestedTopLayers;
    state.effectivetopthicknessmm =
        static_cast<double>(requestedTopLayers) * layerThicknessMm;
    if (state.strategy != ProductionTextureStrategy::LegacyTopBand
        || requestedTopLayers <= 0
        || !std::isfinite(layerThicknessMm)
        || layerThicknessMm <= 0.0)
    {
        SetInvalid(
            state,
            ProductionTextureSettingsErrorCode::InvalidLegacyTopLayers,
            QStringLiteral("Legacy 顶面纹理层数和层高必须为正值。"));
        return state;
    }
    SetValid(state);
    state.stale = current.requestedtoplayers != requestedTopLayers
        || current.stale;
    return state;
}

ProductionTextureControlState ProductionTextureSettingsModel::UpdateGlobal(
    const ProductionTextureControlState& current,
    const double requestedWidthMm,
    const ProductionTexturePartitionMode partitionMode)
{
    ProductionTextureControlState state = current;
    state.requestedwidthmm = requestedWidthMm;
    state.effectivewidthmm = QuantizeGlobalWidth(requestedWidthMm);
    state.partitionmode = partitionMode;
    if (state.strategy != ProductionTextureStrategy::GlobalSurfaceShell
        || !std::isfinite(requestedWidthMm)
        || requestedWidthMm <= 0.0
        || state.effectivewidthmm <= 0.0)
    {
        SetInvalid(
            state,
            ProductionTextureSettingsErrorCode::InvalidGlobalWidth,
            QStringLiteral("Global 纹理壳层宽度必须为正的有限毫米值。"));
        return state;
    }
    SetValid(state);
    state.stale = current.requestedwidthmm != requestedWidthMm
        || current.partitionmode != partitionMode
        || current.stale;
    return state;
}

ProductionTextureSettingsApplyResult ProductionTextureSettingsModel::Apply(
    const QJsonObject& config,
    const ProductionTextureControlState& state)
{
    if (!state.editable)
    {
        return FailureResult(
            config,
            state,
            ProductionTextureSettingsErrorCode::ProfileLocked,
            state.lockreason.isEmpty()
                ? QStringLiteral("当前 Production Profile 锁定了纹理设置。")
                : state.lockreason);
    }
    if (!state.valid)
    {
        return FailureResult(
            config,
            state,
            state.errorcode,
            state.issues.isEmpty()
                ? QStringLiteral("生产纹理设置无效。")
                : state.issues.front());
    }

    QJsonObject root = config;
    QJsonObject texture =
        root.value(QStringLiteral("texture")).toObject();
    if (state.strategy == ProductionTextureStrategy::LegacyTopBand)
    {
        texture.insert(
            QStringLiteral("applyMode"),
            QStringLiteral("top_surface_band"));
        texture.insert(
            QStringLiteral("topSurfaceLayers"),
            state.effectivetoplayers);
    }
    else if (state.strategy == ProductionTextureStrategy::GlobalSurfaceShell)
    {
        texture.insert(
            QStringLiteral("applyMode"),
            QStringLiteral("global_surface_shell"));
        QJsonObject surfaceShell =
            texture.value(QStringLiteral("surfaceShell")).toObject();
        surfaceShell.insert(
            QStringLiteral("widthMm"),
            state.effectivewidthmm);
        surfaceShell.insert(
            QStringLiteral("mode"),
            ProductionTextureSettingsContract::PartitionModeValue(
                state.partitionmode));
        texture.insert(QStringLiteral("surfaceShell"), surfaceShell);
    }
    else
    {
        return FailureResult(
            config,
            state,
            ProductionTextureSettingsErrorCode::UnsupportedStrategy,
            QStringLiteral("诊断或未知策略不能写入生产配置。"));
    }
    root.insert(QStringLiteral("texture"), texture);

    ProductionTextureSettingsApplyResult result;
    result.applied = true;
    result.config = root;
    result.state = state;
    return result;
}
