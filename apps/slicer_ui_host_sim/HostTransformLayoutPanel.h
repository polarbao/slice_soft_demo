#pragma once

#include "HostModelImportWorkflow.h"

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

/** @brief 用于提交实例变换与网格布局的宿主侧控件。 */
class HostTransformLayoutPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建变换和布局控件。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostTransformLayoutPanel(QWidget* parent = nullptr);

    /**
     * @brief 无需模块调用即可更新宿主本地实例选择。
     * @param instanceIds 稳定选定的实例标识。
     */
    void SetSelectedInstances(const QStringList& instanceIds);

    /**
     * @brief 更新面板显示的权威场景摘要。
     * @param instanceCount 宿主跟踪的场景实例的数量。
     * @param sceneRevision 最新已 Commit 的场景修订号。
     */
    void SetSceneState(int instanceCount, quint64 sceneRevision);

    /**
     * @brief 启用跨越公共模块边界的命令。
     * @param enabled 模块就绪且没有活动命令时为 true。
     */
    void SetCommandsEnabled(bool enabled);

    /** @brief 成功 Commit 后重置增量变换输入。 */
    void ResetTransformInputs();

    // 「导入落位与规则排版」已迁往 HostModelListPanel：那组开关在导入执行的
    // 那一刻被读，与「添加模型」按钮同属一页才不会让用户勾了个无效的开关。
    // LayoutRequest / AutoLayoutEnabled / ImportOptions 随之迁走。

signals:
    /** @brief 请求对所选实例变换执行一次原子 Commit。 */
    void SigTransformRequested(
        const QStringList& instanceIds,
        double deltaXMm,
        double deltaYMm,
        double deltaZMm,
        double rotateXDegrees,
        double rotateYDegrees,
        double rotateZDegrees,
        double uniformScaleFactor,
        bool mirrorX,
        bool mirrorY,
        bool landOnBuildPlate);

    /** @brief 请求将选中实例立即贴到构建平台 Z=0。 */
    void SigLandOnBuildPlateRequested(const QStringList& instanceIds);

private slots:
    void OnApplyTransform();
    void OnLandOnBuildPlate();

private:
    void UpdateControls();

    QStringList m_selectedInstanceIds;
    QLabel* m_selectionLabel{nullptr};
    QLabel* m_sceneLabel{nullptr};
    QDoubleSpinBox* m_deltaXSpin{nullptr};
    QDoubleSpinBox* m_deltaYSpin{nullptr};
    QDoubleSpinBox* m_deltaZSpin{nullptr};
    QDoubleSpinBox* m_rotateXSpin{nullptr};
    QDoubleSpinBox* m_rotateYSpin{nullptr};
    QDoubleSpinBox* m_rotateZSpin{nullptr};
    QDoubleSpinBox* m_scaleSpin{nullptr};
    QCheckBox* m_mirrorXCheck{nullptr};
    QCheckBox* m_mirrorYCheck{nullptr};
    QCheckBox* m_autoLandCheck{nullptr};
    QPushButton* m_applyTransformButton{nullptr};
    QPushButton* m_landOnBuildPlateButton{nullptr};
    int m_instanceCount{0};
    quint64 m_sceneRevision{0};
    bool m_commandsEnabled{false};
};
