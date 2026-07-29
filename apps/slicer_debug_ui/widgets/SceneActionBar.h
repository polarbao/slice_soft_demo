#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

/**
 * @brief One production Profile displayed by the quick selector.
 */
struct SceneActionBarProfileOption
{
    QString id;
    QString label;
    QString tooltip;
};

/**
 * @brief Presentation state for the fixed job action bar.
 */
struct SceneActionBarPresentation
{
    bool canimport{true};
    bool cansave{false};
    bool canslice{false};
    bool cancancel{false};
    bool canselectprofile{true};
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
     * @brief Replace the production Profile choices shown in the action bar.
     * @param options User-facing Profile choices in display order.
     * @param selectedProfileId Stable id that should remain selected.
     */
    void SetProfileOptions(
        const QList<SceneActionBarProfileOption>& options,
        const QString& selectedProfileId);

    /**
     * @brief Select a Profile without emitting a user request.
     * @param profileId Stable Profile id, or empty for a custom configuration.
     */
    void SetSelectedProfileId(const QString& profileId);

    /**
     * @brief Return the stable id selected by the quick Profile selector.
     * @return Profile id, or empty for a custom configuration.
     */
    QString SelectedProfileId() const;

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

    /**
     * @brief Request applying one production Profile from the scenario catalog.
     * @param profileId Stable Profile id.
     */
    void SigProfileRequested(const QString& profileId);

private:
    QPushButton* m_importButton{nullptr};
    QPushButton* m_saveButton{nullptr};
    QPushButton* m_sliceButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
    QLabel* m_modeLabel{nullptr};
    QComboBox* m_profileCombo{nullptr};
    QLabel* m_statusLabel{nullptr};
};
