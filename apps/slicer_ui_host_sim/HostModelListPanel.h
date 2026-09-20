#pragma once

#include "HostModelImportWorkflow.h"

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QToolButton;

/** @brief 带本地多选命令的宿主侧模型实例列表。 */
class HostModelListPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建公共 SPI 参考宿主使用的模型列表。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostModelListPanel(QWidget* parent = nullptr);

    /**
     * @brief 追加一个成功导入的模型实例。
     * @param result HostModelImportWorkflow 返回的导入元数据。
     */
    void AddModel(const hostmodelimportresult& result);

    /**
     * @brief 从本地展示列表移除已提交实例。
     * @param instanceIds 模块已移除的稳定实例标识。
     */
    void RemoveInstances(const QStringList& instanceIds);

    /** @brief 清空展示列表，供「新建场景」整体解绑时使用。 */
    void Clear();

    /**
     * @brief 启用或禁用跨越公共模块边界的命令。
     * @param enabled 模块就绪且无活动命令时为 true。
     */
    void SetCommandsEnabled(bool enabled);

    /** @brief 返回选中的稳定实例标识。 */
    [[nodiscard]] QStringList SelectedInstanceIds() const;

    /** @brief 返回宿主显示的已导入实例数。 */
    [[nodiscard]] int ModelCount() const;

    /**
     * @brief 返回当前显示给操作员的网格值。
     * @return 自动和手动布局使用的由宿主持有的布局请求。
     */
    [[nodiscard]] hostgridlayoutrequest LayoutRequest() const;

    /** @brief 返回操作员是否启用了导入后的自动规则排版。 */
    [[nodiscard]] bool AutoLayoutEnabled() const;

    /** @brief 返回下一批模型的自动定向与 XY 原点偏移。 */
    [[nodiscard]] hostmodelimportoptions ImportOptions() const;

    /**
     * @brief 选择从宿主本地画布拾取的实例。
     * @param instanceId 稳定场景实例标识。
     * @return 实例存在于展示列表时返回 true。
     */
    bool SelectInstance(const QString& instanceId);

signals:
    /** @brief 请求打开模型导入流程。 */
    void SigAddRequested();

    /** @brief 请求原子移除选中的模块实例。 */
    void SigRemoveRequested(const QStringList& instanceIds);

    /** @brief 发布用于视图高亮的宿主本地选择。 */
    void SigSelectionChanged(const QStringList& instanceIds);

    /** @brief 请求一次权威的 applyGridLayout 提交。 */
    void SigLayoutRequested(
        int maxColumns,
        int maxRows,
        double columnGapMm,
        double rowGapMm);

private slots:
    void OnSelectAllRequested();
    void OnRemoveRequested();
    void OnSelectionChanged();
    void OnApplyLayout();

private:
    void UpdateControls();

    QLabel* m_summaryLabel{nullptr};
    QListWidget* m_modelList{nullptr};
    QToolButton* m_addButton{nullptr};
    QToolButton* m_selectAllButton{nullptr};
    QToolButton* m_removeButton{nullptr};
    // 「导入落位与规则排版」——这组开关在导入执行的那一刻被读，
    // 故与「添加模型」按钮同属一个面板；曾经它们分居两个标签页，
    // 用户按标签顺序操作时导入完才发现开关，勾选无任何效果。
    QSpinBox* m_columnsSpin{nullptr};
    QSpinBox* m_rowsSpin{nullptr};
    QDoubleSpinBox* m_columnGapSpin{nullptr};
    QDoubleSpinBox* m_rowGapSpin{nullptr};
    QCheckBox* m_autoOrientCheck{nullptr};
    QCheckBox* m_autoLayoutCheck{nullptr};
    QDoubleSpinBox* m_importOriginXSpin{nullptr};
    QDoubleSpinBox* m_importOriginYSpin{nullptr};
    QPushButton* m_applyLayoutButton{nullptr};
    bool m_commandsEnabled{false};
};
