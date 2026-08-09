#include "HostModelListPanel.h"

#include <QAbstractItemView>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
QString AdmissionText(const QString& admission)
{
    if (admission == QStringLiteral("passed"))
    {
        return QStringLiteral("通过");
    }
    if (admission == QStringLiteral("manual_repair_required"))
    {
        return QStringLiteral("需要人工修复");
    }
    return QStringLiteral("阻断");
}
}

HostModelListPanel::HostModelListPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("hostModelListPanel"));
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(6);

    m_summaryLabel = new QLabel(QStringLiteral("模型 0 / 22"), this);
    m_summaryLabel->setObjectName(QStringLiteral("hostModelListSummaryLabel"));
    rootLayout->addWidget(m_summaryLabel);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);
    m_addButton = new QToolButton(this);
    m_addButton->setObjectName(QStringLiteral("hostImportModelButton"));
    m_addButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_addButton->setToolTip(QStringLiteral("导入 OBJ 或 3MF 模型"));
    m_addButton->setFixedSize(28, 28);

    m_selectAllButton = new QToolButton(this);
    m_selectAllButton->setObjectName(
        QStringLiteral("hostModelListSelectAllButton"));
    m_selectAllButton->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogListView));
    m_selectAllButton->setToolTip(QStringLiteral("全选模型（Ctrl+A）"));
    m_selectAllButton->setFixedSize(28, 28);

    m_removeButton = new QToolButton(this);
    m_removeButton->setObjectName(QStringLiteral("hostModelListRemoveButton"));
    m_removeButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    m_removeButton->setToolTip(QStringLiteral("删除选中实例"));
    m_removeButton->setFixedSize(28, 28);

    toolbar->addWidget(m_addButton);
    toolbar->addWidget(m_selectAllButton);
    toolbar->addWidget(m_removeButton);
    toolbar->addStretch(1);
    rootLayout->addLayout(toolbar);

    m_modelList = new QListWidget(this);
    m_modelList->setObjectName(QStringLiteral("hostImportedModelList"));
    m_modelList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_modelList->setMinimumHeight(100);
    m_modelList->setToolTip(QStringLiteral(
        "Ctrl/Shift 多选；当前选择同步到中央工作区，不触发 DLL 调用"));
    rootLayout->addWidget(m_modelList, 1);

    connect(
        m_addButton,
        &QToolButton::clicked,
        this,
        &HostModelListPanel::SigAddRequested);
    connect(
        m_selectAllButton,
        &QToolButton::clicked,
        this,
        &HostModelListPanel::OnSelectAllRequested);
    connect(
        m_removeButton,
        &QToolButton::clicked,
        this,
        &HostModelListPanel::OnRemoveRequested);
    connect(
        m_modelList,
        &QListWidget::itemSelectionChanged,
        this,
        &HostModelListPanel::OnSelectionChanged);
    UpdateControls();
}

void HostModelListPanel::AddModel(const hostmodelimportresult& result)
{
    const QFileInfo source(result.sourcepath);
    auto* item = new QListWidgetItem(
        QStringLiteral("%1\n%2 · %3")
            .arg(
                source.fileName(),
                result.instanceid,
                AdmissionText(result.admission)),
        m_modelList);
    item->setData(Qt::UserRole, result.instanceid);
    item->setData(Qt::UserRole + 1, result.modelid);
    item->setToolTip(QStringLiteral("%1\nmodelId=%2\ninstanceId=%3")
        .arg(result.sourcepath, result.modelid, result.instanceid));
    m_modelList->setCurrentItem(
        item, QItemSelectionModel::ClearAndSelect);
    UpdateControls();
}

void HostModelListPanel::RemoveInstances(const QStringList& instanceIds)
{
    const QSignalBlocker blocker(m_modelList);
    for (int rowIndex = m_modelList->count() - 1; rowIndex >= 0; --rowIndex)
    {
        if (instanceIds.contains(
                m_modelList->item(rowIndex)->data(Qt::UserRole).toString()))
        {
            delete m_modelList->takeItem(rowIndex);
        }
    }
    if (m_modelList->count() > 0)
    {
        const int nextRow = qMax(
            0, qMin(m_modelList->currentRow(), m_modelList->count() - 1));
        m_modelList->setCurrentRow(
            nextRow,
            QItemSelectionModel::ClearAndSelect);
    }
    UpdateControls();
    emit SigSelectionChanged(SelectedInstanceIds());
}

void HostModelListPanel::SetCommandsEnabled(const bool enabled)
{
    m_commandsEnabled = enabled;
    UpdateControls();
}

QStringList HostModelListPanel::SelectedInstanceIds() const
{
    QStringList selected;
    const QList<QListWidgetItem*> items = m_modelList->selectedItems();
    selected.reserve(items.size());
    for (const QListWidgetItem* item : items)
    {
        selected.append(item->data(Qt::UserRole).toString());
    }
    return selected;
}

int HostModelListPanel::ModelCount() const
{
    return m_modelList->count();
}

bool HostModelListPanel::SelectInstance(const QString& instanceId)
{
    for (int rowIndex = 0; rowIndex < m_modelList->count(); ++rowIndex)
    {
        QListWidgetItem* item = m_modelList->item(rowIndex);
        if (item->data(Qt::UserRole).toString() == instanceId)
        {
            m_modelList->setCurrentItem(
                item, QItemSelectionModel::ClearAndSelect);
            return true;
        }
    }
    return false;
}

void HostModelListPanel::OnSelectAllRequested()
{
    m_modelList->selectAll();
    if (m_modelList->currentRow() < 0 && m_modelList->count() > 0)
    {
        m_modelList->setCurrentRow(
            0, QItemSelectionModel::SelectCurrent);
    }
    OnSelectionChanged();
}

void HostModelListPanel::OnRemoveRequested()
{
    const QStringList selected = SelectedInstanceIds();
    if (!selected.isEmpty())
    {
        emit SigRemoveRequested(selected);
    }
}

void HostModelListPanel::OnSelectionChanged()
{
    UpdateControls();
    emit SigSelectionChanged(SelectedInstanceIds());
}

void HostModelListPanel::UpdateControls()
{
    const int selectedCount = SelectedInstanceIds().size();
    m_summaryLabel->setText(
        selectedCount > 0
            ? QStringLiteral("模型 %1 / 22 · 已选 %2")
                  .arg(m_modelList->count())
                  .arg(selectedCount)
            : QStringLiteral("模型 %1 / 22").arg(m_modelList->count()));
    m_addButton->setEnabled(
        m_commandsEnabled && m_modelList->count() < 22);
    m_selectAllButton->setEnabled(
        m_commandsEnabled && m_modelList->count() > 0);
    m_removeButton->setEnabled(
        m_commandsEnabled && selectedCount > 0);
}
