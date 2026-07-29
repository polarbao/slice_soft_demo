#pragma once

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
     * @param legacyToolsPage Temporary legacy diagnostics page for 13D-03.
     * @param parent QWidget owner.
     */
    explicit ContextInspector(
        QWidget* scenePage,
        QWidget* transformPage,
        QWidget* layoutPage,
        QWidget* preflightPage,
        QWidget* legacyToolsPage,
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
     * @brief Select the scene page after an import request.
     */
    void ShowScenePage();

    /**
     * @brief Return ordered inspector page titles.
     * @return Chinese page titles exposed by the inspector.
     */
    QStringList PageTitles() const;

signals:
    /**
     * @brief Request opening the complete central configuration workspace.
     */
    void SigOpenConfigRequested();

private:
    QTabWidget* m_tabs{nullptr};
    QWidget* m_scenePage{nullptr};
    QLabel* m_modeLabel{nullptr};
    QLabel* m_profileLabel{nullptr};
    QLabel* m_availabilityLabel{nullptr};
    QPushButton* m_openConfigButton{nullptr};
};
