#pragma once

#include "DiagnosticSettingsPanel.h"
#include "ProductionTextureSettingsPanel.h"

#include <QStringList>
#include <QWidget>

class QLabel;
class QPushButton;
class QTabWidget;

/**
 * @brief Hosts the single right-side context for scene editing and admission.
 */
class ContextInspector final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the context inspector by reusing existing panel instances.
     * @param scenePage Existing model-list panel.
     * @param transformPage Existing transform panel.
     * @param layoutPage Existing scene-layout panel.
     * @param preflightPage Existing model-preflight panel.
     * @param diagnosticPage Existing actionable diagnostic panel.
     * @param parent QWidget owner.
     */
    explicit ContextInspector(
        QWidget* scenePage,
        QWidget* transformPage,
        QWidget* layoutPage,
        QWidget* preflightPage,
        QWidget* diagnosticPage,
        QWidget* parent = nullptr);

    /**
     * @brief Update the read-only slice settings summary.
     * @param modeLabel Localized product mode.
     * @param profileLabel Current Profile id or display name.
     * @param availability Current scene slicing availability.
     */
    void SetSliceSettingsSummary(
        const QString& modeLabel,
        const QString& profileLabel,
        const QString& availability);

    /**
     * @brief Replace the editable diagnostic request values.
     * @param widthMm Requested texture-surface width in millimetres.
     * @param modelFillMaterial Stable model-fill material id.
     */
    void SetDiagnosticRequestedSettings(
        double widthMm,
        const QString& modelFillMaterial);

    /**
     * @brief Update the diagnostic subject, bounds, backend, and status.
     * @param presentation Read-only diagnostic context.
     */
    void SetDiagnosticPresentation(
        const DiagnosticSettingsPresentation& presentation);

    /**
     * @brief Update conditional production texture or single-material controls.
     * @param presentation Current requested/effective production state.
     */
    void SetProductionTexturePresentation(
        const ProductionTextureSettingsPresentation& presentation);

    /**
     * @brief Select the scene page after an import request.
     */
    void ShowScenePage();

    /**
     * @brief Select the independent texture-diagnostic page.
     */
    void ShowTextureDiagnosticPage();

    /**
     * @brief Return ordered inspector page titles.
     * @return Chinese page titles exposed by the inspector.
     */
    QStringList PageTitles() const;

    /**
     * @brief Return the number of context pages.
     * @return Current page count.
     */
    int PageCount() const;

    /**
     * @brief Return the selected context page index.
     * @return Current page index.
     */
    int CurrentPageIndex() const;

    /**
     * @brief Select a context page when the index is valid.
     * @param index Zero-based context page index.
     * @return true when the page was selected.
     */
    bool SetCurrentPageIndex(int index);

signals:
    /**
     * @brief Request opening the complete central configuration workspace.
     */
    void SigOpenConfigRequested();

    /**
     * @brief Forward a diagnostic texture-width edit.
     * @param widthMm Requested width in millimetres.
     */
    void SigDiagnosticTextureSurfaceWidthChanged(
        double widthMm);

    /**
     * @brief Forward a diagnostic model-fill material edit.
     * @param material Stable material id.
     */
    void SigDiagnosticModelFillMaterialChanged(
        const QString& material);

    /**
     * @brief Forward a request to start background diagnostics.
     */
    void SigDiagnosticStartRequested();

    /**
     * @brief Forward a request to cancel background diagnostics.
     */
    void SigDiagnosticCancelRequested();

    /**
     * @brief Forward a Legacy production top-layer edit.
     * @param layerCount Requested positive Z-layer count.
     */
    void SigProductionLegacyTopLayersChanged(int layerCount);

    /**
     * @brief Forward a Global production texture edit.
     * @param widthMm Requested shell width in millimeters.
     * @param mode Explicit partition mode.
     */
    void SigProductionGlobalTextureChanged(
        double widthMm,
        ProductionTexturePartitionMode mode);

    /**
     * @brief Forward a single-material relief W/V edit.
     * @param material Requested white or varnish material.
     */
    void SigProductionSingleMaterialChanged(
        SingleMaterialReliefMaterial material);

private:
    QTabWidget* m_tabs{nullptr};
    QWidget* m_scenePage{nullptr};
    QLabel* m_modeLabel{nullptr};
    QLabel* m_profileLabel{nullptr};
    QLabel* m_availabilityLabel{nullptr};
    QPushButton* m_openConfigButton{nullptr};
    QTabWidget* m_preflightDiagnosticsTabs{nullptr};
    QWidget* m_preflightDiagnosticsPage{nullptr};
    DiagnosticSettingsPanel* m_diagnosticSettingsPanel{
        nullptr};
    ProductionTextureSettingsPanel* m_productionTextureSettingsPanel{
        nullptr};
};
