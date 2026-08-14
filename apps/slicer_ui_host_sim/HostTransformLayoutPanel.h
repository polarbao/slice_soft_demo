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

    /**
     * @brief 返回当前显示给操作员的网格值。
     * @return 自动和手动布局使用的由宿主持有的布局请求。
     */
    [[nodiscard]] hostgridlayoutrequest LayoutRequest() const;

signals:
    /** @brief 请求对所选实例变换执行一次原子 Commit。 */
    void SigTransformRequested(
        const QStringList& instanceIds,
        double deltaXMm,
        double deltaYMm,
        double deltaZMm,
        double rotateZDegrees,
        double uniformScaleFactor,
        bool mirrorX,
        bool mirrorY);

    /** @brief 请求一次权威的 applyGridLayout 提交。 */
    void SigLayoutRequested(
        int maxColumns,
        int maxRows,
        double columnGapMm,
        double rowGapMm);

private slots:
    void OnApplyTransform();
    void OnApplyLayout();

private:
    void UpdateControls();

    QStringList m_selectedInstanceIds;
    QLabel* m_selectionLabel{nullptr};
    QLabel* m_sceneLabel{nullptr};
    QDoubleSpinBox* m_deltaXSpin{nullptr};
    QDoubleSpinBox* m_deltaYSpin{nullptr};
    QDoubleSpinBox* m_deltaZSpin{nullptr};
    QDoubleSpinBox* m_rotateZSpin{nullptr};
    QDoubleSpinBox* m_scaleSpin{nullptr};
    QCheckBox* m_mirrorXCheck{nullptr};
    QCheckBox* m_mirrorYCheck{nullptr};
    QPushButton* m_applyTransformButton{nullptr};
    QSpinBox* m_columnsSpin{nullptr};
    QSpinBox* m_rowsSpin{nullptr};
    QDoubleSpinBox* m_columnGapSpin{nullptr};
    QDoubleSpinBox* m_rowGapSpin{nullptr};
    QPushButton* m_applyLayoutButton{nullptr};
    int m_instanceCount{0};
    quint64 m_sceneRevision{0};
    bool m_commandsEnabled{false};
};
