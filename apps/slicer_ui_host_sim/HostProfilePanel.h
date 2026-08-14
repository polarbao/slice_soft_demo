#pragma once

#include "HostProfileCatalog.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;

/** @brief 由能力交集结果驱动的宿主 Profile 选择器。 */
class HostProfilePanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建空且禁用的 Profile 选择器。
     * @param parent 可选的 Qt 父对象。
     */
    explicit HostProfilePanel(QWidget* parent = nullptr);

    /**
     * @brief 替换显示的 Profile 可用性结果。
     * @param resolution 有效的宿主/模块能力交集。
     */
    void SetProfiles(const hostprofilecatalogresolution& resolution);

    /** @brief 清空所有 Profile 并显示失败即拒绝原因。 */
    void ClearProfiles(const QString& reason);

    /**
     * @brief 按标识选择一个可用 Profile。
     * @param profileId 由宿主持有的 Profile 标识。
     * @return 成功选择可用 Profile 时返回 true。
     */
    bool SelectProfile(const QString& profileId);

    /** @brief 返回选中的宿主 Profile 标识。 */
    [[nodiscard]] QString SelectedProfileId() const;

    /** @brief 返回当前可用 Profile 数量。 */
    [[nodiscard]] int AvailableProfileCount() const;

signals:
    /** @brief 操作员选择可用 Profile 后发出。 */
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
