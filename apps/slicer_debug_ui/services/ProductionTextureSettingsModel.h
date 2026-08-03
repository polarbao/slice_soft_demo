#pragma once

#include "ProductionTextureSettingsContract.h"

#include <QJsonObject>

/**
 * @brief Result of applying one validated production texture state.
 */
struct ProductionTextureSettingsApplyResult
{
    bool applied{false};
    QJsonObject config;
    ProductionTextureControlState state;
    QString errorcode;
    QStringList issues;
};

/**
 * @brief Resolves and applies production texture settings without owning UI widgets.
 */
class ProductionTextureSettingsModel final
{
public:
    /**
     * @brief Read production texture state from one effective configuration.
     * @param config Effective configuration root.
     * @param editable Whether the current Profile permits editing.
     * @param globalAdmitted Whether Global production admission is satisfied.
     * @param stale Whether an existing package is stale.
     * @return Validated requested/effective production texture state.
     */
    static ProductionTextureControlState Read(
        const QJsonObject& config,
        bool editable,
        bool globalAdmitted,
        bool stale);

    /**
     * @brief Return a Legacy state updated with a requested top-layer count.
     * @param current Current production texture state.
     * @param requestedTopLayers Requested Z-layer count.
     * @param layerThicknessMm Effective slicing layer height in millimeters.
     * @return Revalidated state with derived effective Z thickness.
     */
    static ProductionTextureControlState UpdateLegacyTopLayers(
        const ProductionTextureControlState& current,
        int requestedTopLayers,
        double layerThicknessMm);

    /**
     * @brief Return a Global state updated with physical width and explicit mode.
     * @param current Current production texture state.
     * @param requestedWidthMm Requested normal-distance width in millimeters.
     * @param partitionMode Explicit partial-shell or all-texture mode.
     * @return Revalidated state with 0.01 mm quantized effective width.
     */
    static ProductionTextureControlState UpdateGlobal(
        const ProductionTextureControlState& current,
        double requestedWidthMm,
        ProductionTexturePartitionMode partitionMode);

    /**
     * @brief Apply one validated state to its owned production fields.
     * @param config Source effective configuration.
     * @param state Requested production texture state.
     * @return Atomic apply result; source fields remain unchanged on failure.
     */
    static ProductionTextureSettingsApplyResult Apply(
        const QJsonObject& config,
        const ProductionTextureControlState& state);
};
