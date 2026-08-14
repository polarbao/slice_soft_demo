#pragma once

#include "HostModelImportWorkflow.h"

#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
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

private slots:
    void OnSelectAllRequested();
    void OnRemoveRequested();
    void OnSelectionChanged();

private:
    void UpdateControls();

    QLabel* m_summaryLabel{nullptr};
    QListWidget* m_modelList{nullptr};
    QToolButton* m_addButton{nullptr};
    QToolButton* m_selectAllButton{nullptr};
    QToolButton* m_removeButton{nullptr};
    bool m_commandsEnabled{false};
};
