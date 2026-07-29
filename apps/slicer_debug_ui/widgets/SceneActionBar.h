#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

/**
 * @brief Presentation state for the fixed job action bar.
 */
struct SceneActionBarPresentation
{
    bool canimport{true};
    bool cansave{false};
    bool canslice{false};
    bool cancancel{false};
    QString modelabel;
    QString profilelabel;
    QString statustext;
    QString savereason;
    QString slicereason;
};

/**
 * @brief Fixed workbench actions for model import, scene save, and slicing.
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
     * @param presentation Current workbench action presentation.
     */
    void SetPresentation(
        const SceneActionBarPresentation& presentation);

    /**
     * @brief Return the primary scene slice button.
     * @return Owned button, for compatibility and UI tests.
     */
    QPushButton* SliceButton() const;

signals:
    /**
     * @brief Request importing one or more models into the scene.
     */
    void SigImportRequested();

    /**
     * @brief Request saving the current scene and transforms.
     */
    void SigSaveRequested();

    /**
     * @brief Request slicing the current admitted scene.
     */
    void SigSliceRequested();

    /**
     * @brief Request cancelling the active scene slice.
     */
    void SigCancelRequested();

private:
    QPushButton* m_importButton{nullptr};
    QPushButton* m_saveButton{nullptr};
    QPushButton* m_sliceButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
    QLabel* m_modeLabel{nullptr};
    QLabel* m_profileLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
};
