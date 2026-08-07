#pragma once

#include "ModuleClient.h"

#include <QMainWindow>

#include <memory>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QTabWidget;
class ViewPresentationSettings;
class ViewWorkspaceWidget;

/**
 * @brief Minimal Qt reference shell that consumes only the public module ABI.
 */
class HostMainWindow final : public QMainWindow
{
public:
    /**
     * @brief Creates the reference host and attempts to load the module.
     * @param modulePath Runtime path to slicer_module.dll.
     * @param parent Optional Qt parent widget.
     */
    explicit HostMainWindow(
        const QString& modulePath,
        QWidget* parent = nullptr);

    /** @brief Releases host UI and session settings resources. */
    ~HostMainWindow() override;

private:
    void BuildInterface();
    void LoadModule(const QString& modulePath);
    void SaveViewSettings();

    ModuleClient m_client;
    std::unique_ptr<ViewPresentationSettings> m_viewSettings;
    ViewWorkspaceWidget* m_workspace{nullptr};
    QComboBox* m_defaultViewCombo{nullptr};
    QComboBox* m_projectionCombo{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_pathLabel{nullptr};
    QPlainTextEdit* m_moduleInfoView{nullptr};
};
