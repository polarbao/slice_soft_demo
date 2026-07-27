#include "SceneLayoutPanel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SceneLayoutPanel::SceneLayoutPanel(
    SceneDocument* document,
    QWidget* parent)
    : QWidget(parent),
      m_document(document)
{
    Q_ASSERT(m_document != nullptr);
    setObjectName(QStringLiteral("sceneLayoutPanel"));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(
        QStringLiteral("sceneLayoutSummaryLabel"));
    m_summaryLabel->setWordWrap(true);
    rootLayout->addWidget(m_summaryLabel);

    auto* formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);
    m_columnCountSpin = new QSpinBox(this);
    m_columnCountSpin->setObjectName(
        QStringLiteral("sceneLayoutColumnCountSpin"));
    m_columnCountSpin->setRange(1, 11);
    m_columnCountSpin->setToolTip(
        QStringLiteral("每行最多放置 1 至 11 个模型"));
    formLayout->addRow(
        QStringLiteral("每行模型数"),
        m_columnCountSpin);

    m_rowCountSpin = new QSpinBox(this);
    m_rowCountSpin->setObjectName(
        QStringLiteral("sceneLayoutRowCountSpin"));
    m_rowCountSpin->setRange(1, 2);
    m_rowCountSpin->setToolTip(
        QStringLiteral("场景最多排布 1 至 2 行"));
    formLayout->addRow(
        QStringLiteral("最大行数"),
        m_rowCountSpin);

    m_columnGapSpin = new QDoubleSpinBox(this);
    m_columnGapSpin->setObjectName(
        QStringLiteral("sceneLayoutColumnGapSpin"));
    m_columnGapSpin->setRange(0.0, 1000.0);
    m_columnGapSpin->setDecimals(2);
    m_columnGapSpin->setSingleStep(0.01);
    m_columnGapSpin->setSuffix(QStringLiteral(" mm"));
    m_columnGapSpin->setToolTip(
        QStringLiteral("相邻模型 XY 包围盒边缘之间的水平净距"));
    formLayout->addRow(
        QStringLiteral("列间净距"),
        m_columnGapSpin);

    m_rowGapSpin = new QDoubleSpinBox(this);
    m_rowGapSpin->setObjectName(
        QStringLiteral("sceneLayoutRowGapSpin"));
    m_rowGapSpin->setRange(0.0, 1000.0);
    m_rowGapSpin->setDecimals(2);
    m_rowGapSpin->setSingleStep(0.01);
    m_rowGapSpin->setSuffix(QStringLiteral(" mm"));
    m_rowGapSpin->setToolTip(
        QStringLiteral("相邻模型 XY 包围盒边缘之间的垂直净距"));
    formLayout->addRow(
        QStringLiteral("行间净距"),
        m_rowGapSpin);
    rootLayout->addLayout(formLayout);

    m_applyButton = new QPushButton(
        QStringLiteral("执行规则排版"),
        this);
    m_applyButton->setObjectName(
        QStringLiteral("sceneLayoutApplyButton"));
    m_applyButton->setToolTip(
        QStringLiteral(
            "按稳定导入顺序从左到右、从下到上排布；锁定模型保持原位"));
    rootLayout->addWidget(m_applyButton);

    m_restoreButton = new QPushButton(
        QStringLiteral("恢复排版前位置"),
        this);
    m_restoreButton->setObjectName(
        QStringLiteral("sceneLayoutRestoreButton"));
    m_restoreButton->setToolTip(
        QStringLiteral("恢复最近一次成功排版前的全部模型位置"));
    rootLayout->addWidget(m_restoreButton);
    rootLayout->addStretch(1);

    connect(
        m_document,
        &SceneDocument::SigChanged,
        this,
        &SceneLayoutPanel::OnDocumentChanged);
    connect(
        m_applyButton,
        &QPushButton::clicked,
        this,
        &SceneLayoutPanel::OnApplyRequested);
    connect(
        m_restoreButton,
        &QPushButton::clicked,
        this,
        &SceneLayoutPanel::OnRestoreRequested);
    OnDocumentChanged();
}

void SceneLayoutPanel::OnDocumentChanged()
{
    const slicer_core::SceneLayout& layout =
        m_document->Layout();
    m_columnCountSpin->setValue(layout.maxcolumns);
    m_rowCountSpin->setValue(layout.maxrows);
    m_columnGapSpin->setValue(layout.columngapmm);
    m_rowGapSpin->setValue(layout.rowgapmm);

    const std::size_t capacity =
        static_cast<std::size_t>(layout.maxcolumns)
        * static_cast<std::size_t>(layout.maxrows);
    m_summaryLabel->setText(
        QStringLiteral(
            "按行优先排布：%1 个模型，当前容量 %2。"
            "隐藏模型仍占位，锁定模型保持原位。")
            .arg(m_document->InstanceCount())
            .arg(capacity));
    m_applyButton->setEnabled(
        m_document->InstanceCount() > 0U);
    m_restoreButton->setEnabled(
        m_document->CanRestoreGridLayout());
}

void SceneLayoutPanel::OnApplyRequested()
{
    slicer_core::SceneLayout layout =
        m_document->Layout();
    layout.policy = "grid";
    layout.maxcolumns = m_columnCountSpin->value();
    layout.maxrows = m_rowCountSpin->value();
    layout.columngapmm = m_columnGapSpin->value();
    layout.rowgapmm = m_rowGapSpin->value();
    layout.spacingmode = "edge_clearance";
    layout.order = "row_major";

    ShowResult(
        m_document->ApplyGridLayout(
            layout,
            m_document->SceneRevision()),
        QStringLiteral("规则排版已完成。"));
}

void SceneLayoutPanel::OnRestoreRequested()
{
    ShowResult(
        m_document->RestoreGridLayout(
            m_document->SceneRevision()),
        QStringLiteral("已恢复排版前位置。"));
}

void SceneLayoutPanel::ShowResult(
    const SceneDocumentOperationResult& result,
    const QString& successMessage)
{
    if (result.IsValid())
    {
        emit SigStatusMessage(
            result.changed
                ? successMessage
                : QStringLiteral("当前排版无需变更。"));
        return;
    }
    emit SigStatusMessage(
        QString::fromLatin1(
            SceneDocumentOperationErrorCodeName(
                result.error->code)
                .data())
        + QStringLiteral("：") + result.error->message);
}
