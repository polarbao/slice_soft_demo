#pragma once

#include <QString>
#include <QStringList>

/**
 * @brief Production texture semantics exposed by the Qt workbench.
 */
enum class ProductionTextureStrategy
{
    Unsupported,
    LegacyTopBand,
    GlobalSurfaceShell,
    DiagnosticOnly,
};

/**
 * @brief Explicit partition mode used by the Global Surface Shell pipeline.
 */
enum class ProductionTexturePartitionMode
{
    PartialShell,
    AllTexture,
};

/**
 * @brief Stable validation errors for production texture controls.
 */
enum class ProductionTextureSettingsErrorCode
{
    None,
    UnsupportedStrategy,
    InvalidLegacyTopLayers,
    InvalidGlobalWidth,
    InvalidPartitionMode,
    ProfileLocked,
    GlobalNotAdmitted,
};

/**
 * @brief Stable JSON field ownership for one production texture strategy.
 */
struct ProductionTextureFieldMapping
{
    QString applymodepath;
    QString requestedvaluepath;
    QString partitionmodepath;
    QString layerthicknesspath;
    QString requestedunit;
    QString backend;
    bool production{false};
};

/**
 * @brief Widget-independent requested and effective production texture state.
 */
struct ProductionTextureControlState
{
    ProductionTextureStrategy strategy{
        ProductionTextureStrategy::Unsupported};
    int requestedtoplayers{1};
    int effectivetoplayers{1};
    double effectivetopthicknessmm{0.0};
    double requestedwidthmm{0.0};
    double effectivewidthmm{0.0};
    ProductionTexturePartitionMode partitionmode{
        ProductionTexturePartitionMode::PartialShell};
    QString backend;
    bool editable{false};
    QString lockreason;
    bool stale{false};
    bool valid{false};
    ProductionTextureSettingsErrorCode errorcode{
        ProductionTextureSettingsErrorCode::UnsupportedStrategy};
    QStringList issues;
};

/**
 * @brief Defines stable identities and field mappings for 12E-09D settings.
 */
class ProductionTextureSettingsContract final
{
public:
    /**
     * @brief Return the production JSON fields owned by a strategy.
     * @param strategy Texture-control strategy.
     * @return Stable field mapping. Diagnostic and unsupported strategies do
     * not expose writable production paths.
     */
    static ProductionTextureFieldMapping FieldMapping(
        ProductionTextureStrategy strategy);

    /**
     * @brief Convert a strategy to its stable serialized value.
     * @param strategy Texture-control strategy.
     * @return Stable strategy value.
     */
    static QString StrategyValue(ProductionTextureStrategy strategy);

    /**
     * @brief Convert a Global partition mode to its stable serialized value.
     * @param mode Global texture partition mode.
     * @return Stable partition mode value.
     */
    static QString PartitionModeValue(ProductionTexturePartitionMode mode);

    /**
     * @brief Convert a validation error to its stable machine-readable code.
     * @param code Production texture validation error.
     * @return Stable error code, or an empty string for None.
     */
    static QString ErrorCodeValue(ProductionTextureSettingsErrorCode code);
};
