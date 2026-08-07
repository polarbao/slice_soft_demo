#pragma once

#include "ModuleClient.h"

#include <QMainWindow>

class QLabel;
class QPlainTextEdit;

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

private:
    void BuildInterface();
    void LoadModule(const QString& modulePath);

    ModuleClient m_client;
    QLabel* m_statusLabel{nullptr};
    QLabel* m_pathLabel{nullptr};
    QPlainTextEdit* m_moduleInfoView{nullptr};
};
