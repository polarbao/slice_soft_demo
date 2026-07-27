#include "ModelListPanel.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QStyle>
#include <QToolButton>

#include <algorithm>

namespace
{

QString InstanceId(const SceneDocumentItem& item)
{
    return QString::fromStdString(item.instance.instanceid);
}

QString DisplayText(const SceneDocumentItem& item)
{
    const QString name = QFileInfo(item.modelpath).completeBaseName();
    const QString format =
        QFileInfo(item.modelpath).suffix().toUpper();
    const double width =
        item.instance.effectivebboxmm.max.x
        - item.instance.effectivebboxmm.min.x;
    const double height =
        item.instance.effectivebboxmm.max.y
        - item.instance.effectivebboxmm.min.y;
    QString admission = QStringLiteral("待检测");
    if (item.geometry.has_value())
    {
        admission =
            item.geometry->admissionstatus
                    == slicer_core::SceneViewAdmissionStatus::Blocked
                ? QStringLiteral("阻断")
                : item.geometry->admissionstatus
                          == slicer_core::
                              SceneViewAdmissionStatus::Admitted
                    ? QStringLiteral("通过")
                    : QStringLiteral("待检测");
    }
    const QString state = item.instance.visible
        ? item.instance.locked
            ? QStringLiteral("显示 / 锁定")
            : QStringLiteral("显示")
        : item.instance.locked
            ? QStringLiteral("隐藏 / 锁定")
            : QStringLiteral("隐藏");
    return QStringLiteral("%1\n%2  %3 x %4 mm\n%5  %6  %7")
        .arg(name)
        .arg(format)
        .arg(width, 0, 'f', 2)
        .arg(height, 0, 'f', 2)
        .arg(InstanceId(item), admission, state);
}

}  // namespace

ModelListPanel::ModelListPanel(
    SceneDocument* document,
    SceneSelectionModel* selectionModel,
    QWidget* parent)
    : QWidget(parent),
      m_document(document),
      m_selectionModel(selectionModel)
{
    Q_ASSERT(m_document != nullptr);
    Q_ASSERT(m_selectionModel != nullptr);
    setObjectName(QStringLiteral("modelListPanel"));
    setMinimumWidth(210);
    setMaximumWidth(300);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(6);
    m_summaryLabel = new QLabel(QStringLiteral("模型 0 / 22"), this);
    m_summaryLabel->setObjectName(QStringLiteral("modelListSummaryLabel"));
    rootLayout->addWidget(m_summaryLabel);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);
    m_addButton = new QToolButton(this);
    m_addButton->setObjectName(QStringLiteral("modelListAddButton"));
    m_addButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    m_addButton->setToolTip(QStringLiteral("添加模型"));
    m_duplicateButton = new QToolButton(this);
    m_duplicateButton->setObjectName(
        QStringLiteral("modelListDuplicateButton"));
    m_duplicateButton->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_duplicateButton->setToolTip(QStringLiteral("复制选中实例"));
    m_deleteButton = new QToolButton(this);
    m_deleteButton->setObjectName(QStringLiteral("modelListDeleteButton"));
    m_deleteButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    m_deleteButton->setToolTip(QStringLiteral("删除选中实例"));
    m_visibilityButton = new QToolButton(this);
    m_visibilityButton->setObjectName(
        QStringLiteral("modelListVisibilityButton"));
    m_visibilityButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogYesButton));
    m_visibilityButton->setToolTip(QStringLiteral("显示或隐藏选中实例"));
    m_lockButton = new QToolButton(this);
    m_lockButton->setObjectName(QStringLiteral("modelListLockButton"));
    m_lockButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogApplyButton));
    m_lockButton->setToolTip(QStringLiteral("锁定或解锁选中实例"));
    for (QToolButton* button : {
             m_addButton,
             m_duplicateButton,
             m_deleteButton,
             m_visibilityButton,
             m_lockButton})
    {
        button->setFixedSize(28, 28);
        toolbar->addWidget(button);
    }
    toolbar->addStretch(1);
    rootLayout->addLayout(toolbar);

    m_modelList = new QListWidget(this);
    m_modelList->setObjectName(QStringLiteral("modelInstanceList"));
    m_modelList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modelList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rootLayout->addWidget(m_modelList, 1);

    connect(
        m_document,
        &SceneDocument::SigChanged,
        this,
        &ModelListPanel::OnDocumentChanged);
    connect(
        m_selectionModel,
        &SceneSelectionModel::SigSelectionChanged,
        this,
        &ModelListPanel::OnSelectionChanged);
    connect(
        m_modelList,
        &QListWidget::currentRowChanged,
        this,
        &ModelListPanel::OnCurrentRowChanged);
    connect(
        m_addButton,
        &QToolButton::clicked,
        this,
        &ModelListPanel::SigAddRequested);
    connect(
        m_duplicateButton,
        &QToolButton::clicked,
        this,
        &ModelListPanel::OnDuplicateRequested);
    connect(
        m_deleteButton,
        &QToolButton::clicked,
        this,
        &ModelListPanel::OnDeleteRequested);
    connect(
        m_visibilityButton,
        &QToolButton::clicked,
        this,
        &ModelListPanel::OnVisibilityRequested);
    connect(
        m_lockButton,
        &QToolButton::clicked,
        this,
        &ModelListPanel::OnLockRequested);
    OnDocumentChanged();
}

void ModelListPanel::OnDocumentChanged()
{
    m_refreshing = true;
    const QString selected = m_selectionModel->SelectedInstance();
    m_modelList->clear();
    int selectedRow{-1};
    int row{0};
    for (const SceneDocumentItem& item : m_document->Items())
    {
        auto* listItem = new QListWidgetItem(DisplayText(item), m_modelList);
        listItem->setData(Qt::UserRole, InstanceId(item));
        listItem->setToolTip(
            QStringLiteral("%1\nmodelId=%2\ninstanceId=%3")
                .arg(
                    item.modelpath,
                    QString::fromStdString(item.instance.modelid),
                    InstanceId(item)));
        if (!item.instance.visible)
        {
            listItem->setForeground(QColor(125, 125, 125));
        }
        if (InstanceId(item) == selected)
        {
            selectedRow = row;
        }
        ++row;
    }
    m_summaryLabel->setText(
        QStringLiteral("模型 %1 / 22").arg(m_document->InstanceCount()));
    if (selectedRow >= 0)
    {
        m_modelList->setCurrentRow(selectedRow);
    }
    m_refreshing = false;
    UpdateButtons();
}

void ModelListPanel::OnSelectionChanged(const QString& instanceId)
{
    if (m_refreshing)
    {
        return;
    }
    for (int row = 0; row < m_modelList->count(); ++row)
    {
        if (m_modelList->item(row)->data(Qt::UserRole).toString()
            == instanceId)
        {
            m_refreshing = true;
            m_modelList->setCurrentRow(row);
            m_refreshing = false;
            break;
        }
    }
    UpdateButtons();
}

void ModelListPanel::OnCurrentRowChanged(const int row)
{
    if (m_refreshing || row < 0)
    {
        return;
    }
    const QString instanceId =
        m_modelList->item(row)->data(Qt::UserRole).toString();
    if (m_document->SetCurrentInstance(instanceId))
    {
        m_selectionModel->SetSelectedInstance(instanceId);
    }
    UpdateButtons();
}

void ModelListPanel::OnDuplicateRequested()
{
    const QString sourceInstanceId = SelectedInstanceId();
    if (sourceInstanceId.isEmpty())
    {
        return;
    }
    const SceneDocumentOperationResult result =
        m_document->DuplicateInstance(
            sourceInstanceId,
            BuildDuplicateInstanceId(sourceInstanceId),
            m_document->SceneRevision());
    if (result.IsValid() && result.changed)
    {
        m_selectionModel->SetSelectedInstance(
            m_document->CurrentInstanceId());
    }
    ShowResult(result, QStringLiteral("已复制模型实例。"));
}

void ModelListPanel::OnDeleteRequested()
{
    const QString instanceId = SelectedInstanceId();
    if (instanceId.isEmpty())
    {
        return;
    }
    const SceneDocumentOperationResult result =
        m_document->DeleteInstance(
            instanceId,
            m_document->SceneRevision());
    if (result.IsValid() && result.changed)
    {
        m_selectionModel->SetSelectedInstance(
            m_document->CurrentInstanceId());
    }
    ShowResult(result, QStringLiteral("已删除模型实例。"));
}

void ModelListPanel::OnVisibilityRequested()
{
    const QString instanceId = SelectedInstanceId();
    const SceneDocumentItem* item = FindItem(instanceId);
    if (item == nullptr)
    {
        return;
    }
    ShowResult(
        m_document->SetInstanceVisible(
            instanceId,
            !item->instance.visible,
            m_document->SceneRevision()),
        item->instance.visible
            ? QStringLiteral("已隐藏模型实例。")
            : QStringLiteral("已显示模型实例。"));
}

void ModelListPanel::OnLockRequested()
{
    const QString instanceId = SelectedInstanceId();
    const SceneDocumentItem* item = FindItem(instanceId);
    if (item == nullptr)
    {
        return;
    }
    ShowResult(
        m_document->SetInstanceLocked(
            instanceId,
            !item->instance.locked,
            m_document->SceneRevision()),
        item->instance.locked
            ? QStringLiteral("已解锁模型实例。")
            : QStringLiteral("已锁定模型实例。"));
}

QString ModelListPanel::SelectedInstanceId() const
{
    return m_selectionModel->SelectedInstance();
}

QString ModelListPanel::BuildDuplicateInstanceId(
    const QString& sourceInstanceId) const
{
    for (int suffix = 1; suffix <= 22; ++suffix)
    {
        const QString candidate =
            sourceInstanceId + QStringLiteral("-copy-")
            + QString::number(suffix);
        if (FindItem(candidate) == nullptr)
        {
            return candidate;
        }
    }
    return sourceInstanceId + QStringLiteral("-copy-overflow");
}

const SceneDocumentItem* ModelListPanel::FindItem(
    const QString& instanceId) const
{
    const auto found = std::find_if(
        m_document->Items().begin(),
        m_document->Items().end(),
        [&instanceId](const SceneDocumentItem& item)
        {
            return InstanceId(item) == instanceId;
        });
    return found == m_document->Items().end()
        ? nullptr
        : &(*found);
}

void ModelListPanel::ShowResult(
    const SceneDocumentOperationResult& result,
    const QString& successMessage)
{
    if (result.IsValid())
    {
        if (result.changed)
        {
            emit SigStatusMessage(successMessage);
        }
        return;
    }
    emit SigStatusMessage(
        QString::fromLatin1(
            SceneDocumentOperationErrorCodeName(result.error->code).data())
        + QStringLiteral("：") + result.error->message);
}

void ModelListPanel::UpdateButtons()
{
    const SceneDocumentItem* item = FindItem(SelectedInstanceId());
    const bool hasSelection = item != nullptr;
    const bool sceneReady =
        m_document->State() != SceneDocumentState::Loading
        && !m_document->IsGeometryStale();
    m_addButton->setEnabled(
        m_document->InstanceCount() < 22U && sceneReady);
    m_duplicateButton->setEnabled(
        hasSelection
        && m_document->InstanceCount() < 22U
        && sceneReady);
    m_deleteButton->setEnabled(
        hasSelection && !item->instance.locked);
    m_visibilityButton->setEnabled(hasSelection);
    m_lockButton->setEnabled(hasSelection);
}
