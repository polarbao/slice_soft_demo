#include "HostModelListPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
// 与 HostTransformLayoutPanel 各留一份：两处的 spin 语义相同但归属不同面板，
// 为一个 8 行的构造函数提一个公共头，收益不抵多出来的耦合。
QDoubleSpinBox* CreateDistanceSpin(const QString& objectName, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(-1000.0, 1000.0);
    spin->setDecimals(2);
    spin->setSingleStep(0.1);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setKeyboardTracking(false);
    return spin;
}

QDoubleSpinBox* CreateGapSpin(const QString& objectName, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(0.0, 1000.0);
    spin->setDecimals(2);
    spin->setSingleStep(0.1);
    spin->setValue(10.0);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setKeyboardTracking(false);
    return spin;
}

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
    m_addButton->setToolTip(QStringLiteral("导入 OBJ、3MF 或 STL 模型"));
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

    // 紧邻「添加模型」：这组开关在导入执行的那一刻被读
    //（HostMainWindowImport 取 ImportOptions / AutoLayoutEnabled），
    // 放在别的标签页会让用户导入完才发现它们、且勾选已经无效。
    auto* layoutGroup = new QGroupBox(
        QStringLiteral("导入落位与规则排版"), this);
    // 紧凑网格而非 QFormLayout：模型页还要放列表与预检表，
    // hostux 的零滚动判据（第 0/1/4/5 页不得出现滚动条）容不下 8 行表单。
    // 两个开关并排、原点与排版参数各挤一行，高度由约 360px 降到约 4 行。
    auto* layoutGrid = new QGridLayout(layoutGroup);
    layoutGrid->setContentsMargins(6, 4, 6, 4);
    layoutGrid->setHorizontalSpacing(6);
    layoutGrid->setVerticalSpacing(4);
    m_columnsSpin = new QSpinBox(layoutGroup);
    m_columnsSpin->setObjectName(QStringLiteral("hostLayoutColumnsSpin"));
    m_columnsSpin->setRange(1, 11);
    m_columnsSpin->setValue(11);
    m_columnsSpin->setKeyboardTracking(false);
    m_columnsSpin->setToolTip(QStringLiteral("每行模型数"));
    m_rowsSpin = new QSpinBox(layoutGroup);
    m_rowsSpin->setObjectName(QStringLiteral("hostLayoutRowsSpin"));
    m_rowsSpin->setRange(1, 2);
    m_rowsSpin->setValue(2);
    m_rowsSpin->setKeyboardTracking(false);
    m_rowsSpin->setToolTip(QStringLiteral("最大行数"));
    m_columnGapSpin = CreateGapSpin(
        QStringLiteral("hostLayoutColumnGapSpin"), layoutGroup);
    m_columnGapSpin->setToolTip(QStringLiteral("列间净距"));
    m_rowGapSpin = CreateGapSpin(
        QStringLiteral("hostLayoutRowGapSpin"), layoutGroup);
    m_rowGapSpin->setToolTip(QStringLiteral("行间净距"));
    m_autoOrientCheck = new QCheckBox(
        QStringLiteral("导入时自动定向"), layoutGroup);
    m_autoOrientCheck->setObjectName(
        QStringLiteral("hostImportAutoOrientCheck"));
    m_autoOrientCheck->setChecked(true);
    m_autoOrientCheck->setToolTip(QStringLiteral(
        "取消后保留模型源姿态；Z 轴仍在添加实例时自动触底"));
    m_autoLayoutCheck = new QCheckBox(
        QStringLiteral("导入后自动排版"), layoutGroup);
    m_autoLayoutCheck->setObjectName(
        QStringLiteral("hostLayoutAutoApplyCheck"));
    m_autoLayoutCheck->setChecked(true);
    m_autoLayoutCheck->setToolTip(QStringLiteral(
        "取消后使用模型源 XY 坐标及批次原点偏移；仍可手动执行规则排版"));
    m_importOriginXSpin = CreateDistanceSpin(
        QStringLiteral("hostImportOriginXSpin"), layoutGroup);
    m_importOriginYSpin = CreateDistanceSpin(
        QStringLiteral("hostImportOriginYSpin"), layoutGroup);
    m_importOriginXSpin->setToolTip(QStringLiteral(
        "对下一批导入模型统一增加 X 偏移，保持模型之间的相对位置"));
    m_importOriginYSpin->setToolTip(QStringLiteral(
        "对下一批导入模型统一增加 Y 偏移，保持模型之间的相对位置"));
    m_applyLayoutButton = new QPushButton(
        QStringLiteral("执行规则排版"), layoutGroup);
    m_applyLayoutButton->setObjectName(QStringLiteral("hostLayoutApplyButton"));
    m_applyLayoutButton->setToolTip(QStringLiteral(
        "排版算法由切片能力模块执行；宿主不自行计算实例落位"));
    layoutGrid->addWidget(m_autoOrientCheck, 0, 0, 1, 2);
    layoutGrid->addWidget(m_autoLayoutCheck, 0, 2, 1, 2);
    layoutGrid->addWidget(
        new QLabel(QStringLiteral("批次原点"), layoutGroup), 1, 0);
    layoutGrid->addWidget(m_importOriginXSpin, 1, 1);
    layoutGrid->addWidget(m_importOriginYSpin, 1, 2);
    layoutGrid->addWidget(m_applyLayoutButton, 1, 3);
    layoutGrid->addWidget(
        new QLabel(QStringLiteral("排版"), layoutGroup), 2, 0);
    layoutGrid->addWidget(m_columnsSpin, 2, 1);
    layoutGrid->addWidget(m_rowsSpin, 2, 2);
    layoutGrid->addWidget(m_columnGapSpin, 3, 1);
    layoutGrid->addWidget(m_rowGapSpin, 3, 2);
    rootLayout->addWidget(layoutGroup);

    connect(
        m_autoLayoutCheck,
        &QCheckBox::toggled,
        this,
        [this]
        {
            UpdateControls();
        });
    connect(
        m_applyLayoutButton,
        &QPushButton::clicked,
        this,
        &HostModelListPanel::OnApplyLayout);

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

// Clear 此前只清列表不刷控件：摘要文案与「执行规则排版」的启用态会停在
// 清空前的值。搬入排版按钮后这一点变得可见，顺带修掉。
void HostModelListPanel::Clear()
{
    const QSignalBlocker blocker(m_modelList);
    m_modelList->clear();
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

hostgridlayoutrequest HostModelListPanel::LayoutRequest() const
{
    return hostgridlayoutrequest{
        m_columnsSpin->value(),
        m_rowsSpin->value(),
        m_columnGapSpin->value(),
        m_rowGapSpin->value()};
}

bool HostModelListPanel::AutoLayoutEnabled() const
{
    return m_autoLayoutCheck->isChecked();
}

hostmodelimportoptions HostModelListPanel::ImportOptions() const
{
    return hostmodelimportoptions{
        m_autoOrientCheck->isChecked(),
        m_autoLayoutCheck->isChecked() ? 0.0 : m_importOriginXSpin->value(),
        m_autoLayoutCheck->isChecked() ? 0.0 : m_importOriginYSpin->value()};
}

void HostModelListPanel::OnApplyLayout()
{
    const hostgridlayoutrequest request = LayoutRequest();
    emit SigLayoutRequested(
        request.maxcolumns,
        request.maxrows,
        request.columngapmm,
        request.rowgapmm);
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
    // 搬自 HostTransformLayoutPanel::UpdateControls。原判据用的是场景实例数
    // （SetSceneState 喂），这里改用展示列表计数——排版针对的正是已导入的模型，
    // 两者语义一致，且省掉把 SetSceneState 再接到五个调用点。
    m_applyLayoutButton->setEnabled(
        m_commandsEnabled && m_modelList->count() > 0);
    const bool sourcePlacementEnabled = !m_autoLayoutCheck->isChecked();
    m_importOriginXSpin->setEnabled(sourcePlacementEnabled);
    m_importOriginYSpin->setEnabled(sourcePlacementEnabled);
    m_selectAllButton->setEnabled(
        m_commandsEnabled && m_modelList->count() > 0);
    m_removeButton->setEnabled(
        m_commandsEnabled && selectedCount > 0);
}
