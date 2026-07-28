#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

/**
 * @brief Compact current-scene primary action without changing the workbench layout.
 */
class SceneActionBar final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the current-scene action bar.
     * @param parent QWidget owner.
     */
    explicit SceneActionBar(QWidget* parent = nullptr);

    /**
     * @brief Update action availability and explanatory status.
     * @param canSlice Whether a new scene process may start.
     * @param canCancel Whether the active scene process may be cancelled.
     * @param status Short Chinese status.
     * @param reason Tooltip explanation for the primary action.
     */
    void SetPresentation(
        bool canSlice,
        bool canCancel,
        const QString& status,
        const QString& reason);

    /**
     * @brief Return the primary scene slice button.
     * @return Owned button, for compatibility and UI tests.
     */
    QPushButton* SliceButton() const;

signals:
    void SigSliceRequested();
    void SigCancelRequested();

private:
    QPushButton* m_sliceButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
    QLabel* m_statusLabel{nullptr};
};
