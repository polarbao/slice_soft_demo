#pragma once

#include "HostProfileCatalog.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;

/** @brief Host Profile selector backed by capability-intersection results. */
class HostProfilePanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates an empty and disabled Profile selector.
     * @param parent Optional Qt parent.
     */
    explicit HostProfilePanel(QWidget* parent = nullptr);

    /**
     * @brief Replaces the displayed Profile availability results.
     * @param resolution Valid host/module capability intersection.
     */
    void SetProfiles(const hostprofilecatalogresolution& resolution);

    /** @brief Clears all Profiles and displays a fail-closed reason. */
    void ClearProfiles(const QString& reason);

    /**
     * @brief Selects an available Profile by identity.
     * @param profileId Host-owned Profile identity.
     * @return True when an available Profile was selected.
     */
    bool SelectProfile(const QString& profileId);

    /** @brief Returns the selected host Profile identity. */
    [[nodiscard]] QString SelectedProfileId() const;

    /** @brief Returns the number of currently available Profiles. */
    [[nodiscard]] int AvailableProfileCount() const;

signals:
    /** @brief Emitted after the operator selects an available Profile. */
    void SigProfileChanged(const QString& profileId);

private:
    void OnProfileChanged(int index);
    void RefreshPresentation();
    const hostprofileavailability* CurrentProfile() const;

    QList<hostprofileavailability> m_profiles;
    QComboBox* m_profileCombo{nullptr};
    QLabel* m_safetyLabel{nullptr};
    QLabel* m_availabilityLabel{nullptr};
    QLabel* m_descriptionLabel{nullptr};
    QPlainTextEdit* m_capabilityView{nullptr};
};
