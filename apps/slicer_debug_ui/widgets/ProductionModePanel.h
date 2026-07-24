#pragma once

#include "../services/ProductionModeCatalog.h"

#include <optional>
#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;

/**
 * @brief Product-facing selector for Legacy and admitted Global production modes.
 */
class ProductionModePanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the production mode and Profile selector.
     * @param parent Qt parent widget.
     */
    explicit ProductionModePanel(QWidget* parent = nullptr);

    /**
     * @brief Return the explicitly selected production mode.
     * @return Legacy by default, or Global Surface Shell after explicit selection.
     */
    slicer_core::SlicePipelineMode SelectedMode() const;

    /**
     * @brief Return the selected admitted Global Profile.
     * @return Empty for Legacy; stable Profile ID for Global Surface Shell.
     */
    QString SelectedProfileId() const;

    /**
     * @brief Mark the current admission result stale after a relevant input change.
     * @param reason Chinese explanation shown to the operator.
     */
    void MarkAdmissionStale(const QString& reason);

    /**
     * @brief Show a production admission state without changing the selection.
     * @param state Current fail-closed state.
     * @param detail Chinese state or blocking detail.
     */
    void ShowAdmissionState(ProductionAdmissionState state, const QString& detail);

    /**
     * @brief Show the validated result of the current production session.
     * @param result Fail-closed production package and resource presentation.
     */
    void ShowProductionResult(const ProductionModeUiDto& result);

    /**
     * @brief Clear any previous package result before a new run starts.
     */
    void ClearProductionResult();

signals:
    /**
     * @brief Emitted after the product mode or Global Profile changes.
     */
    void SigSelectionChanged();

private slots:
    void OnModeChanged(int index);
    void OnProfileChanged(int index);

private:
    void RefreshProfileItems();
    void RefreshPresentation();

    QComboBox* m_modeCombo{nullptr};
    QComboBox* m_profileCombo{nullptr};
    QLabel* m_capabilityLabel{nullptr};
    QLabel* m_admissionLabel{nullptr};
    QLabel* m_blockingLabel{nullptr};
    QLabel* m_resourceLabel{nullptr};
    QLabel* m_resultIdentityLabel{nullptr};
    QLabel* m_resultOutputLabel{nullptr};
    QLabel* m_resultResourceLabel{nullptr};
    ProductionAdmissionState m_admissionState{
        ProductionAdmissionState::Pending};
    QString m_admissionDetail;
    std::optional<ProductionModeUiDto> m_result;
};
