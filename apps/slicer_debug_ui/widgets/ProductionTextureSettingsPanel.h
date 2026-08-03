#pragma once

#include "../services/ProductionTextureSettingsContract.h"
#include "../services/SingleMaterialReliefResolver.h"

#include <QString>
#include <QWidget>

#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QStackedWidget;

/**
 * @brief Read-only and editable state shown by the production settings panel.
 */
struct ProductionTextureSettingsPresentation
{
    QString profileid;
    ProductionTextureControlState texture;
    std::optional<SingleMaterialReliefState> singlematerialrelief;
};

/**
 * @brief Edits Profile-supported production texture or single-material settings.
 */
class ProductionTextureSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the conditional production settings panel.
     * @param parent QWidget owner.
     */
    explicit ProductionTextureSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief Replace the complete presentation without emitting edit signals.
     * @param presentation Current Profile requested/effective production state.
     */
    void SetPresentation(
        const ProductionTextureSettingsPresentation& presentation);

signals:
    /**
     * @brief Request changing the Legacy top texture layer count.
     * @param layerCount Requested positive Z-layer count.
     */
    void SigLegacyTopLayersChanged(int layerCount);

    /**
     * @brief Request changing Global physical width or partition mode.
     * @param widthMm Requested shell width in millimeters.
     * @param mode Explicit partial-shell or all-texture mode.
     */
    void SigGlobalTextureChanged(
        double widthMm,
        ProductionTexturePartitionMode mode);

    /**
     * @brief Request atomically changing the single-material relief channel.
     * @param material White W or varnish V.
     */
    void SigSingleMaterialChanged(
        SingleMaterialReliefMaterial material);

private:
    void OnGlobalControlChanged();
    void UpdateStateLabels(
        bool stale,
        bool editable,
        bool valid,
        const QString& lockReason,
        const QStringList& issues);

    QStackedWidget* m_pages{nullptr};
    QWidget* m_unsupportedPage{nullptr};
    QWidget* m_legacyPage{nullptr};
    QWidget* m_globalPage{nullptr};
    QWidget* m_singleMaterialPage{nullptr};
    QLabel* m_strategyLabel{nullptr};
    QLabel* m_stateLabel{nullptr};
    QLabel* m_lockLabel{nullptr};
    QSpinBox* m_legacyTopLayers{nullptr};
    QLabel* m_legacyEffectiveThickness{nullptr};
    QComboBox* m_globalMode{nullptr};
    QDoubleSpinBox* m_globalWidth{nullptr};
    QLabel* m_globalEffectiveWidth{nullptr};
    QLabel* m_globalBackend{nullptr};
    QComboBox* m_singleMaterial{nullptr};
    QLabel* m_singleEffectiveChannel{nullptr};
    bool m_updating{false};
};
