#include "HostPackageReviewPanel.h"

#include "HostPackageReviewChannels.h"

#include "HostChannelChartWidget.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
class FitPreviewLabel final : public QLabel
{
public:
    explicit FitPreviewLabel(const QString& placeholder, QWidget* parent)
        : QLabel(placeholder, parent)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumSize(240, 200);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }

    void SetImage(const QImage& image)
    {
        m_image = image;
        RefreshPixmap();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QLabel::resizeEvent(event);
        RefreshPixmap();
    }

private:
    void RefreshPixmap()
    {
        if (m_image.isNull() || width() <= 0 || height() <= 0)
        {
            return;
        }
        setPixmap(QPixmap::fromImage(m_image).scaled(
            size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QImage m_image;
};

QStringList Channels(std::initializer_list<const char*> names)
{
    QStringList channels;
    for (const char* name : names)
    {
        channels.append(QString::fromLatin1(name));
    }
    return channels;
}

QString SamplingStrategyText(const QString& strategyId)
{
    if (strategyId == QStringLiteral(
            "layer_slab_supersample_2x2_at_least_two_candidate"))
    {
        return QStringLiteral("S3 诊断候选｜层体积 2×2（至少 2/4）");
    }
    return QStringLiteral("S0 生产默认｜Legacy 中心采样");
}

qint64 PrintPixels(
    const hostlayerdescriptor& layer,
    const QString& channel)
{
    const QStringList channels = Channels({"R", "G", "B", "W", "S", "V", "T"});
    const int index = channels.indexOf(channel);
    return index >= 0
        ? static_cast<qint64>(layer.printpixels.values.at(
            static_cast<std::size_t>(index)))
        : 0;
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

    m_openPackageDirectoryButton = new QPushButton(
        QStringLiteral("打开包目录"), this);
    m_openPackageDirectoryButton->setObjectName(
        QStringLiteral("hostOpenPackageDirectoryButton"));
    m_openPackageDirectoryButton->setEnabled(false);
    m_openPackageDirectoryButton->setToolTip(
        QStringLiteral("完成切片并严格校验生产包后可用。"));
    rootLayout->addWidget(m_openPackageDirectoryButton, 0, Qt::AlignLeft);

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
    /* 下拉项由「全通道并集 + 单通道」两类构成。
       此前另有 RGB+白墨、RGB+支撑、RGB+光油 等两两组合，共 13 项：
       用户要在其中挑对一项，先得知道包是几通道、哪些通道有数据；
       而这些中间组合既不是判读材质本色的最佳视图（那是 RGB 单通道），
       也不是查看全部产出的最佳视图（那是并集）。故收敛为两类。
       并集项的通道集由 SelectDefaultPreviewMode 按包实际通道自动选中。 */
    /* 并集只保留【一项】，其通道集由生产包决定，在 SelectDefaultPreviewMode 中按包改写。
       此前是六通道、七通道两个固定项：对七通道包而言，七通道并集是六通道并集的严格超集
       （合成器 RgbSupportWhiteVarnishTransfer 依次叠加 RGB/W/S/V，最后才是 T），
       六通道那项纯属冗余；而对六通道包，七通道那项因缺 T 平面必然失败。
       两个固定项都要求用户先知道包是几通道，而这恰恰是软件自己知道的事。 */
    m_previewModeCombo->addItem(
        QStringLiteral("全通道并集"),
        Channels({"R", "G", "B", "W", "S", "V"}));
    for (const char* channel : {"R", "G", "B", "W", "S", "V"})
    {
        m_previewModeCombo->addItem(
            QString::fromLatin1(channel),
            Channels({channel}));
    }
    m_previewModeCombo->addItem(QStringLiteral("T（缩裹）"), Channels({"T"}));
    m_previewModeCombo->setItemData(
        0,
        QStringLiteral(
            "叠加本包全部非 RGB 通道的伪彩色：W 青蓝、S 纯绿、V 中灰；"
            "七通道包再叠加缩裹 T（品红，压在最上层以免被支撑盖住）。"
            "伪彩色不代表生产 TIFF 像素值。"),
        Qt::ToolTipRole);
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
    m_stage16SummaryLabel = new QLabel(
        QStringLiteral("Stage 16 诊断：等待生产包。"), previewPage);
    m_stage16SummaryLabel->setObjectName(
        QStringLiteral("hostPackageStage16SummaryLabel"));
    m_stage16SummaryLabel->setWordWrap(true);
    m_stage16SummaryLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse);
    previewLayout->addWidget(m_stage16SummaryLabel);

    auto* currentArea = new QScrollArea(previewPage);
    currentArea->setObjectName(QStringLiteral("hostPackagePreviewScroll"));
    currentArea->setWidgetResizable(true);
    currentArea->setAlignment(Qt::AlignCenter);
    m_previewLabel = new FitPreviewLabel(
        QStringLiteral("尚无生产层预览"), currentArea);
    m_previewLabel->setObjectName(QStringLiteral("hostPackagePreviewImage"));
    currentArea->setWidget(m_previewLabel);
    previewLayout->addWidget(currentArea, 2);
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
    connect(
        m_openPackageDirectoryButton,
        &QPushButton::clicked,
        this,
        &HostPackageReviewPanel::OnOpenPackageDirectory);
}

void HostPackageReviewPanel::SetPackage(const hostpackagereview& review)
{
    m_review = review;
    const QStringList packageChannels = review.channels.isEmpty()
        ? Channels({"R", "G", "B", "W", "S", "V"}) : review.channels;
    SelectDefaultPreviewMode(m_previewModeCombo, packageChannels);
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
            "目录：%1\n身份：%2\n协议：%3\n生产准入：%4\n通道：%5\n位深：%6\n极性：%7\n"
            "网格：%8 × %9 px\nDPI：%10 × %11\n层数：%12\n实例：%13\n"
            "Profile 版本：%14\nProfile Hash：%15")
            .arg(review.packagedirectory)
            .arg(review.packageidentity)
            .arg(review.schema)
            .arg(review.productionacceptance)
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
    m_channelChart->SetChannels(packageChannels);
    m_channelChart->SetLayers(review.layers);
    RefreshStage16Summary(0);

    const bool packageDirectoryAvailable = review.valid
        && !review.packagedirectory.isEmpty()
        && QFileInfo(review.packagedirectory).isDir();
    m_openPackageDirectoryButton->setEnabled(packageDirectoryAvailable);
    if (packageDirectoryAvailable)
    {
        m_openPackageDirectoryButton->setToolTip(
            QStringLiteral("打开本次作业返回的生产包目录：%1")
                .arg(review.packagedirectory));
    }
    else if (!review.valid)
    {
        m_openPackageDirectoryButton->setToolTip(
            QStringLiteral("生产包严格校验未通过，不能打开目录。"));
    }
    else if (review.packagedirectory.isEmpty())
    {
        m_openPackageDirectoryButton->setToolTip(
            QStringLiteral("切片作业未返回生产包目录。"));
    }
    else
    {
        m_openPackageDirectoryButton->setToolTip(
            QStringLiteral("切片作业返回的生产包目录不存在。"));
    }
}

void HostPackageReviewPanel::SetStage16Context(
    const QString& samplingStrategyId,
    const QJsonObject& timing)
{
    m_samplingStrategyId = samplingStrategyId;
    m_timing = timing;
    RefreshStage16Summary(m_layerSpin->value());
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
    static_cast<FitPreviewLabel*>(m_previewLabel)->SetImage(image);
    m_layerLabel->setText(
        QStringLiteral("layer=%1 · z=%2 mm · %3 × %4 px · %5")
            .arg(layer.layerindex)
            .arg(layer.zmm, 0, 'f', 3)
            .arg(layer.widthpx)
            .arg(layer.heightpx)
            .arg(layer.storagemode));
    RefreshStage16Summary(layer.layerindex);
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
    RefreshStage16Summary(layerIndex);
    EmitPreviewRequest();
}

void HostPackageReviewPanel::OnLayerSpinChanged(const int layerIndex)
{
    const QSignalBlocker blocker(m_layerSlider);
    m_layerSlider->setValue(layerIndex);
    RefreshStage16Summary(layerIndex);
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

void HostPackageReviewPanel::OnOpenPackageDirectory()
{
    if (!m_review.valid || m_review.packagedirectory.isEmpty()
        || !QFileInfo(m_review.packagedirectory).isDir())
    {
        m_openPackageDirectoryButton->setEnabled(false);
        ShowError(QStringLiteral("切片作业返回的生产包目录不存在，已停止打开。"));
        return;
    }
    emit SigOpenPackageDirectoryRequested(m_review.packagedirectory);
}

void HostPackageReviewPanel::EmitPreviewRequest()
{
    if (m_review.valid && m_layerSpin->isEnabled())
    {
        emit SigLayerPreviewRequested(
            m_layerSpin->value(), SelectedChannels());
    }
}

void HostPackageReviewPanel::RefreshStage16Summary(const int layerIndex)
{
    if (!m_review.valid || m_review.layers.isEmpty()
        || layerIndex < 0 || layerIndex >= m_review.layers.size())
    {
        m_stage16SummaryLabel->setText(
            QStringLiteral("Stage 16 诊断：等待生产包。"));
        return;
    }

    const hostlayerdescriptor& current = m_review.layers.at(layerIndex);
    QStringList channelPixels;
    const QStringList packageChannels = m_review.channels.isEmpty()
        ? Channels({"R", "G", "B", "W", "S", "V"}) : m_review.channels;
    for (const QString& channel : packageChannels)
    {
        channelPixels.append(QStringLiteral("%1=%2")
                                 .arg(channel)
                                 .arg(PrintPixels(current, channel)));
    }
    const int scanCount = m_timing.value(
        QStringLiteral("supportStatisticsScanCount")).toInt(-1);
    const double sliceProcessingMs = m_timing.value(
        QStringLiteral("sliceProcessingMs")).toDouble(-1.0);
    m_stage16SummaryLabel->setText(
        QStringLiteral(
            "几何采样：%1｜姿态：P0 生产默认，P3 仅诊断未应用\n"
            "当前生产层 layer=%2｜打印像素：%3\n"
            "性能：sliceProcessing=%4｜支撑统计扫描=%5\n"
            "通道显示：R/G/B 为真实颜色；W/S/V 为显示用伪彩色"
            "（S 纯绿、W 青蓝、V 中灰），不代表生产 TIFF 像素值")
            .arg(SamplingStrategyText(m_samplingStrategyId))
            .arg(current.layerindex)
            .arg(channelPixels.join(QStringLiteral("  ")))
            .arg(sliceProcessingMs >= 0.0
                    ? QStringLiteral("%1 ms").arg(
                        sliceProcessingMs, 0, 'f', 1)
                    : QStringLiteral("未提供"))
            .arg(scanCount >= 0
                    ? QStringLiteral("%1 次（实例累计）").arg(scanCount)
                    : QStringLiteral("未提供")));
}
