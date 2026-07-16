#include "MaterialClosurePanel.h"

#include <QAbstractItemView>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

MaterialClosurePanel::MaterialClosurePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("materialClosurePanel"));

    auto* layout = new QVBoxLayout(this);
    m_status = new QLabel(QStringLiteral("尚未加载材料闭环报告。"), this);
    m_status->setObjectName(QStringLiteral("materialClosureStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_summaryView = new QTextEdit(this);
    m_summaryView->setObjectName(QStringLiteral("materialClosureSummary"));
    m_summaryView->setReadOnly(true);
    m_summaryView->setMinimumHeight(120);
    layout->addWidget(m_summaryView);

    m_worstLayers = new QTableWidget(this);
    m_worstLayers->setObjectName(QStringLiteral("materialClosureWorstLayers"));
    m_worstLayers->setColumnCount(5);
    m_worstLayers->setHorizontalHeaderLabels(
        QStringList{
            QStringLiteral("层"),
            QStringLiteral("Z (mm)"),
            QStringLiteral("Gap 像素"),
            QStringLiteral("类型"),
            QStringLiteral("诊断预览")});
    m_worstLayers->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_worstLayers->setSelectionMode(QAbstractItemView::SingleSelection);
    m_worstLayers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_worstLayers->verticalHeader()->setVisible(false);
    m_worstLayers->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_worstLayers->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_worstLayers->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_worstLayers->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_worstLayers->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    layout->addWidget(m_worstLayers, 1);

    m_locateButton = new QPushButton(QStringLiteral("定位到选中层"), this);
    m_locateButton->setObjectName(QStringLiteral("materialClosureLocateButton"));
    m_locateButton->setToolTip(
        QStringLiteral(
            "按报告中的真实 layerIndex 跳转统一预览；仅当报告提供且文件存在时显示诊断 Gap 伪彩图。"));
    m_locateButton->setEnabled(false);
    layout->addWidget(m_locateButton);

    connect(
        m_locateButton,
        &QPushButton::clicked,
        this,
        &MaterialClosurePanel::OnLocateSelectedLayer);
    connect(
        m_worstLayers,
        &QTableWidget::cellDoubleClicked,
        this,
        &MaterialClosurePanel::OnWorstLayerActivated);
    connect(
        m_worstLayers,
        &QTableWidget::itemSelectionChanged,
        this,
        &MaterialClosurePanel::OnWorstLayerSelectionChanged);
}

void MaterialClosurePanel::LoadPackage(const PackageSummary& package)
{
    m_summary = MaterialClosureReportInterpreter::Interpret(
        FindReportPath(package),
        package.package_dir);
    RebuildView();
}

QString MaterialClosurePanel::SummaryForTest() const
{
    return m_summaryView == nullptr ? QString{} : m_summaryView->toPlainText();
}

int MaterialClosurePanel::WorstLayerCountForTest() const
{
    return m_worstLayers == nullptr ? 0 : m_worstLayers->rowCount();
}

bool MaterialClosurePanel::SelectWorstLayerForTest(const int row)
{
    if (m_worstLayers == nullptr || row < 0 || row >= m_worstLayers->rowCount())
    {
        return false;
    }
    m_worstLayers->selectRow(row);
    return true;
}

bool MaterialClosurePanel::TriggerSelectedLayerForTest()
{
    return EmitSelectedLayer();
}

void MaterialClosurePanel::OnLocateSelectedLayer()
{
    EmitSelectedLayer();
}

void MaterialClosurePanel::OnWorstLayerActivated(const int row, const int column)
{
    Q_UNUSED(column);
    if (SelectWorstLayerForTest(row))
    {
        EmitSelectedLayer();
    }
}

void MaterialClosurePanel::OnWorstLayerSelectionChanged()
{
    m_locateButton->setEnabled(SelectedWorstLayerRow() >= 0);
}

QString MaterialClosurePanel::FindReportPath(const PackageSummary& package) const
{
    for (const QString& path : package.report_paths)
    {
        if (QFileInfo(path).fileName().compare(
                QStringLiteral("material_closure_report.json"),
                Qt::CaseInsensitive)
            == 0)
        {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    return {};
}

void MaterialClosurePanel::RebuildView()
{
    const QString text = MaterialClosureReportInterpreter::BuildSummaryText(m_summary);
    m_summaryView->setPlainText(text);
    m_worstLayers->setRowCount(0);

    if (!m_summary.reportavailable)
    {
        m_status->setText(QStringLiteral("材料闭环报告：未生成"));
        m_status->setStyleSheet(QStringLiteral("color: #8a5a00;"));
    }
    else if (!m_summary.schemavalid)
    {
        m_status->setText(QStringLiteral("材料闭环报告：无效"));
        m_status->setStyleSheet(QStringLiteral("color: #b00020;"));
    }
    else if (m_summary.candidateonly)
    {
        m_status->setText(QStringLiteral("材料闭环报告：候选诊断，不能作为生产通过依据"));
        m_status->setStyleSheet(QStringLiteral("color: #8a5a00;"));
    }
    else if (m_summary.closurestatus == QStringLiteral("not_available"))
    {
        m_status->setText(QStringLiteral("材料闭环报告：精确证据不可用"));
        m_status->setStyleSheet(QStringLiteral("color: #8a5a00;"));
    }
    else if (m_summary.closurestatus == QStringLiteral("pass"))
    {
        m_status->setText(QStringLiteral("材料闭环报告：精确诊断通过"));
        m_status->setStyleSheet(QStringLiteral("color: #176b2c;"));
    }
    else
    {
        m_status->setText(QStringLiteral("材料闭环报告：存在待处理 Gap"));
        m_status->setStyleSheet(QStringLiteral("color: #b00020;"));
    }

    for (const MaterialClosureWorstLayerUi& layer : m_summary.worstlayers)
    {
        const int row = m_worstLayers->rowCount();
        m_worstLayers->insertRow(row);
        m_worstLayers->setItem(
            row,
            0,
            new QTableWidgetItem(QString::number(layer.layerindex)));
        m_worstLayers->setItem(
            row,
            1,
            new QTableWidgetItem(QString::number(layer.zmm, 'f', 3)));
        m_worstLayers->setItem(
            row,
            2,
            new QTableWidgetItem(QString::number(layer.gappixels)));

        QStringList displayTypes;
        for (const QString& type : layer.types)
        {
            displayTypes.push_back(
                MaterialClosureReportInterpreter::DisplayGapType(type));
        }
        m_worstLayers->setItem(
            row,
            3,
            new QTableWidgetItem(
                displayTypes.isEmpty()
                    ? QStringLiteral("未分类")
                    : displayTypes.join(QStringLiteral(", "))));
        m_worstLayers->setItem(
            row,
            4,
            new QTableWidgetItem(
                layer.gappreviewpath.isEmpty()
                    ? QStringLiteral("无")
                    : QStringLiteral("可用")));
    }

    if (m_worstLayers->rowCount() > 0)
    {
        m_worstLayers->selectRow(0);
    }
    OnWorstLayerSelectionChanged();
}

int MaterialClosurePanel::SelectedWorstLayerRow() const
{
    if (m_worstLayers == nullptr)
    {
        return -1;
    }
    const QList<QTableWidgetItem*> selectedItems = m_worstLayers->selectedItems();
    return selectedItems.isEmpty() ? -1 : selectedItems.first()->row();
}

bool MaterialClosurePanel::EmitSelectedLayer()
{
    const int row = SelectedWorstLayerRow();
    if (row < 0 || row >= m_summary.worstlayers.size())
    {
        return false;
    }

    const MaterialClosureWorstLayerUi& layer = m_summary.worstlayers.at(row);
    emit SigLayerRequested(layer.layerindex, layer.gappreviewpath);
    return true;
}
