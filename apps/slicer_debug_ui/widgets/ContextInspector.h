#pragma once

#include "DiagnosticSettingsPanel.h"

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
     * @param parent QWidget owner.
     */
    explicit ContextInspector(
        QWidget* scenePage,
        QWidget* transformPage,
        QWidget* layoutPage,
        QWidget* preflightPage,
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
     * @brief Select the scene page after an import request.
     */
    void ShowScenePage();

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

private:
    QTabWidget* m_tabs{nullptr};
    QWidget* m_scenePage{nullptr};
    QLabel* m_modeLabel{nullptr};
    QLabel* m_profileLabel{nullptr};
    QLabel* m_availabilityLabel{nullptr};
    QPushButton* m_openConfigButton{nullptr};
    DiagnosticSettingsPanel* m_diagnosticSettingsPanel{
        nullptr};
};
