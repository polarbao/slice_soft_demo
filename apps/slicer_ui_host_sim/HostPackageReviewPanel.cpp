#include "HostPackageReviewPanel.h"

#include "HostChannelChartWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
QStringList Channels(std::initializer_list<const char*> names)
{
    QStringList channels;
    for (const char* name : names)
    {
        channels.append(QString::fromLatin1(name));
    }
    return channels;
}
}

HostPackageReviewPanel::HostPackageReviewPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("hostPackageReviewPanel"));
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    m_validationLabel = new QLabel(
        QStringLiteral("尚未加载生产结果。"), this);
    m_validationLabel->setObjectName(
        QStringLiteral("hostPackageValidationLabel"));
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rootLayout->addWidget(m_validationLabel);

    auto* controls = new QHBoxLayout();
    controls->addWidget(new QLabel(QStringLiteral("生产层"), this));
    m_layerSlider = new QSlider(Qt::Horizontal, this);
    m_layerSlider->setObjectName(QStringLiteral("hostPackageLayerSlider"));
    m_layerSlider->setEnabled(false);
    controls->addWidget(m_layerSlider, 1);
    m_layerSpin = new QSpinBox(this);
    m_layerSpin->setObjectName(QStringLiteral("hostPackageLayerSpin"));
    m_layerSpin->setEnabled(false);
    controls->addWidget(m_layerSpin);
    m_previewModeCombo = new QComboBox(this);
    m_previewModeCombo->setObjectName(
        QStringLiteral("hostPackagePreviewModeCombo"));
    m_previewModeCombo->addItem(
        QStringLiteral("RGB + W + S + V"), Channels({"R", "G", "B", "W", "S", "V"}));
    m_previewModeCombo->addItem(
        QStringLiteral("RGB"), Channels({"R", "G", "B"}));
    m_previewModeCombo->addItem(
        QStringLiteral("RGB + W"), Channels({"R", "G", "B", "W"}));
    m_previewModeCombo->addItem(
        QStringLiteral("RGB + S"), Channels({"R", "G", "B", "S"}));
    m_previewModeCombo->addItem(
        QStringLiteral("RGB + V"), Channels({"R", "G", "B", "V"}));
    for (const char* channel : {"R", "G", "B", "W", "S", "V"})
    {
        m_previewModeCombo->addItem(
            QString::fromLatin1(channel),
            Channels({channel}));
    }
    controls->addWidget(m_previewModeCombo);
    rootLayout->addLayout(controls);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("hostPackageResultSplitter"));

    auto* previewPage = new QWidget(splitter);
    auto* previewLayout = new QVBoxLayout(previewPage);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    m_layerLabel = new QLabel(
        QStringLiteral("请选择生产层。"), previewPage);
    m_layerLabel->setObjectName(QStringLiteral("hostPackageLayerLabel"));
    m_layerLabel->setWordWrap(true);
    previewLayout->addWidget(m_layerLabel);
    auto* scrollArea = new QScrollArea(previewPage);
    scrollArea->setObjectName(QStringLiteral("hostPackagePreviewScroll"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setAlignment(Qt::AlignCenter);
    m_previewLabel = new QLabel(
        QStringLiteral("尚无生产层预览"), scrollArea);
    m_previewLabel->setObjectName(QStringLiteral("hostPackagePreviewImage"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(320, 240);
    scrollArea->setWidget(m_previewLabel);
    previewLayout->addWidget(scrollArea, 2);
    m_channelChart = new HostChannelChartWidget(previewPage);
    previewLayout->addWidget(m_channelChart, 1);
    splitter->addWidget(previewPage);

    auto* detailPage = new QWidget(splitter);
    auto* detailLayout = new QVBoxLayout(detailPage);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    auto* summaryGroup = new QGroupBox(QStringLiteral("生产包摘要"), detailPage);
    auto* summaryLayout = new QVBoxLayout(summaryGroup);
    m_summaryView = new QPlainTextEdit(summaryGroup);
    m_summaryView->setObjectName(QStringLiteral("hostPackageSummaryView"));
    m_summaryView->setReadOnly(true);
    summaryLayout->addWidget(m_summaryView);
    detailLayout->addWidget(summaryGroup, 1);

    auto* reportGroup = new QGroupBox(QStringLiteral("命名报告"), detailPage);
    auto* reportLayout = new QVBoxLayout(reportGroup);
    m_reportCombo = new QComboBox(reportGroup);
    m_reportCombo->setObjectName(QStringLiteral("hostPackageReportCombo"));
    const QStringList reportNames{
        QStringLiteral("slice"),
        QStringLiteral("preview"),
        QStringLiteral("scene")};
    m_reportCombo->addItems(reportNames);
    reportLayout->addWidget(m_reportCombo);
    m_reportView = new QPlainTextEdit(reportGroup);
    m_reportView->setObjectName(QStringLiteral("hostPackageReportView"));
    m_reportView->setReadOnly(true);
    reportLayout->addWidget(m_reportView, 1);
    detailLayout->addWidget(reportGroup, 2);
    splitter->addWidget(detailPage);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    rootLayout->addWidget(splitter, 1);

    connect(
        m_layerSlider,
        &QSlider::valueChanged,
        this,
        &HostPackageReviewPanel::OnLayerSliderChanged);
    connect(
        m_layerSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &HostPackageReviewPanel::OnLayerSpinChanged);
    connect(
        m_previewModeCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &HostPackageReviewPanel::OnPreviewModeChanged);
    connect(
        m_reportCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &HostPackageReviewPanel::OnReportChanged);
}

void HostPackageReviewPanel::SetPackage(const hostpackagereview& review)
{
    m_review = review;
    const QSignalBlocker sliderBlocker(m_layerSlider);
    const QSignalBlocker spinBlocker(m_layerSpin);
    const int maximum = (std::max)(0, review.layercount - 1);
    m_layerSlider->setRange(0, maximum);
    m_layerSpin->setRange(0, maximum);
    m_layerSlider->setValue(0);
    m_layerSpin->setValue(0);
    m_layerSlider->setEnabled(review.valid && review.layercount > 0);
    m_layerSpin->setEnabled(review.valid && review.layercount > 0);
    m_validationLabel->setText(
        review.valid
            ? QStringLiteral("生产包严格校验通过 · %1 层 · %2")
                  .arg(review.layercount)
                  .arg(review.packageidentity)
            : QStringLiteral("生产包严格校验失败：%1")
                  .arg(review.verificationerrors.join(QStringLiteral("；"))));
    m_summaryView->setPlainText(
        QStringLiteral(
            "目录：%1\n身份：%2\n协议：%3\n通道：%4\n位深：%5\n极性：%6\n"
            "网格：%7 × %8 px\nDPI：%9 × %10\n层数：%11\n实例：%12\n"
            "Profile 版本：%13\nProfile Hash：%14")
            .arg(review.packagedirectory)
            .arg(review.packageidentity)
            .arg(review.schema)
            .arg(review.channels.join(QStringLiteral(" ")))
            .arg(review.bitdepth)
            .arg(review.polarity)
            .arg(review.widthpx)
            .arg(review.heightpx)
            .arg(review.dpix)
            .arg(review.dpiy)
            .arg(review.layercount)
            .arg(review.instancecount)
            .arg(review.profileversion)
            .arg(review.profilehash));
    m_channelChart->SetLayers(review.layers);
}

void HostPackageReviewPanel::ShowPreview(
    const QString& imagePath,
    const hostlayerdescriptor& layer)
{
    const QImage image(imagePath);
    if (image.isNull())
    {
        ShowError(QStringLiteral("宿主无法读取模块生成的层预览：%1")
                      .arg(imagePath));
        return;
    }
    m_previewLabel->setPixmap(QPixmap::fromImage(image));
    m_previewLabel->resize(image.size());
    m_layerLabel->setText(
        QStringLiteral("layer=%1 · z=%2 mm · %3 × %4 px · %5")
            .arg(layer.layerindex)
            .arg(layer.zmm, 0, 'f', 3)
            .arg(layer.widthpx)
            .arg(layer.heightpx)
            .arg(layer.storagemode));
}

void HostPackageReviewPanel::ShowReport(const hostpackagereport& report)
{
    m_reportView->setPlainText(
        QStringLiteral("报告：%1\nSchema：%2\n来源：%3\n\n%4")
            .arg(
                report.name,
                report.schema,
                report.sourcepath,
                QString::fromUtf8(QJsonDocument(report.data)
                    .toJson(QJsonDocument::Indented))));
}

void HostPackageReviewPanel::ShowError(const QString& message)
{
    m_validationLabel->setText(QStringLiteral("结果查看失败：%1").arg(message));
}

QStringList HostPackageReviewPanel::SelectedChannels() const
{
    return m_previewModeCombo->currentData().toStringList();
}

void HostPackageReviewPanel::OnLayerSliderChanged(const int layerIndex)
{
    const QSignalBlocker blocker(m_layerSpin);
    m_layerSpin->setValue(layerIndex);
    EmitPreviewRequest();
}

void HostPackageReviewPanel::OnLayerSpinChanged(const int layerIndex)
{
    const QSignalBlocker blocker(m_layerSlider);
    m_layerSlider->setValue(layerIndex);
    EmitPreviewRequest();
}

void HostPackageReviewPanel::OnPreviewModeChanged(const int index)
{
    (void)index;
    EmitPreviewRequest();
}

void HostPackageReviewPanel::OnReportChanged(const int index)
{
    if (index >= 0)
    {
        emit SigReportRequested(m_reportCombo->itemText(index));
    }
}

void HostPackageReviewPanel::EmitPreviewRequest()
{
    if (m_review.valid && m_layerSpin->isEnabled())
    {
        emit SigLayerPreviewRequested(
            m_layerSpin->value(), SelectedChannels());
    }
}
