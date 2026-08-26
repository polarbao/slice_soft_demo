#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class HostMaterialSettingsPanel;
class HostMatvolSettingsPanel;
class HostSupportSettingsPanel;
class HostTextureSettingsPanel;

/** @brief 由宿主持有的切片设置编辑器和有效的Profile 预览。 */
class HostSliceSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建具有面向生产的参考默认值的编辑器。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostSliceSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief 更新 H-B-04 选择的宿主Profile。
     * @param profileId 可用的宿主Profile 标识。
     * @param supportsSlice 当 Profile 需要受支持的切片能力时为 true。
     */
    void SetSelectedProfileId(
        const QString& profileId,
        bool supportsSlice = true);

    /**
     * @brief 更新有效Profile 预览所使用的模型。
     * @param modelPath 现有导入的 OBJ、3MF 或 STL 路径，或者为空。
     */
    void SetModelPath(const QString& modelPath);

    /**
     * @brief 记录已创建场景的权威上下文。
     * @param bound 当场景上下文对于此会话不可变时为 true。
     * @param profileId Profile 标识与场景相关。
     * @param buildVolume 随场景提交的设备构建体积。
     */
    void SetSceneAuthority(
        bool bound,
        const QString& profileId,
        const hostbuildvolume& buildVolume);

    /**
     * @brief 在创建场景之前应用经过验证的用户首选项。
     * @param settings 保留由宿主持有的参数；模型标识被忽略。
     * @return 该函数不返回值。
     */
    void SetPersistentSettings(const hostslicesettings& settings);

    /**
     * @brief 返回编辑器中当前显示的所有由宿主持有的值。
     * @return 设置快照独立于模块状态。
     */
    [[nodiscard]] hostslicesettings Settings() const;

    /**
     * @brief 返回精确有效 Profile 是否已准备就绪。
     * @return 仅在模型、路径和场景绑定验证通过后才为 true。
     */
    [[nodiscard]] bool IsReady() const;

    /**
     * @brief 返回最新验证的有效 Profile。
     * @return 当 IsReady 为 false 时为空对象。
     */
    [[nodiscard]] hosteffectiveprofile EffectiveProfile() const;

    /**
     * @brief 按当前控件值重新构造本次提交使用的有效 Profile。
     * @param effectiveProfile 接收重新校验后的提交快照。
     * @param error 接收当前设置无法提交的原因。
     * @return 当前设置与场景绑定均可提交时返回 true。
     */
    [[nodiscard]] bool BuildSubmissionProfile(
        hosteffectiveprofile* effectiveProfile,
        QString* error) const;

    /**
     * @brief 显示当前非阻塞的源纹理纯白预检。
     * @param message 用户可读的标识绑定预检状态。
     * @param warning 当纯白色证据需要操作员注意时，这是正确的。
     */
    void SetTextureWhitePreflightStatus(
        const QString& message,
        bool warning);

    /**
     * @brief 根据当前场景外观资源限制可选打印工艺。
     * @param restricted true 时只允许单材料白墨或光油。
     * @param reason 用于 UI 诊断的限制原因。
     */
    void SetSingleMaterialRestriction(
        bool restricted,
        const QString& reason);

    /**
     * @brief 成功作业结束后为自动管理的输出路径准备下一次会话。
     * @param completedPackageDirectory 刚完成作业实际发布的生产包目录。
     * @return 自动路径已轮换时为 true；自定义路径或身份不匹配时为 false。
     */
    bool PrepareNextAutomaticOutputDirectory(
        const QString& completedPackageDirectory);

signals:
    /** @brief 在操作员更改本地切片设置后发出。 */
    void SigSettingsChanged();

private:
    void BuildInterface();
    void OnBrowseOutput();
    void OnProcessPresetChanged(int index);
    void OnProcessSettingsEdited();
    void OnSettingsEdited();
    void RefreshPreview();
    bool ValidateSceneBinding(QString* error) const;

    QLabel* m_profileLabel{nullptr};
    QComboBox* m_processPresetCombo{nullptr};
    QSpinBox* m_dpiXSpin{nullptr};
    QSpinBox* m_dpiYSpin{nullptr};
    QDoubleSpinBox* m_layerThicknessSpin{nullptr};
    QComboBox* m_geometrySamplingCombo{nullptr};
    QCheckBox* m_tiffCompressionCheck{nullptr};
    QComboBox* m_tiffCompressionCombo{nullptr};
    QLineEdit* m_outputEdit{nullptr};
    QPushButton* m_outputBrowseButton{nullptr};
    QDoubleSpinBox* m_buildWidthSpin{nullptr};
    QDoubleSpinBox* m_buildHeightSpin{nullptr};
    QDoubleSpinBox* m_buildZSpin{nullptr};
    HostMaterialSettingsPanel* m_materialPanel{nullptr};
    HostMatvolSettingsPanel* m_matvolPanel{nullptr};
    HostTextureSettingsPanel* m_texturePanel{nullptr};
    HostSupportSettingsPanel* m_supportPanel{nullptr};
    QLabel* m_validationLabel{nullptr};
    QLabel* m_textureWhitePreflightLabel{nullptr};
    QPlainTextEdit* m_profilePreview{nullptr};
    QString m_profileId;
    bool m_profileSupportsSlice{true};
    QString m_modelPath;
    bool m_sceneBound{false};
    QString m_sceneProfileId;
    hostbuildvolume m_sceneBuildVolume;
    hosteffectiveprofile m_effectiveProfile;
    QString m_defaultOutputDirectory;
    bool m_outputUsesAutomaticDirectory{true};
    bool m_applyingProcessPreset{false};
    bool m_singleMaterialRestricted{false};
    QString m_singleMaterialRestrictionReason;
    HostPackageProtocol m_packageProtocol{HostPackageProtocol::Rgbwsv};
    hosttransferchannelsettings m_transferChannel;
};
