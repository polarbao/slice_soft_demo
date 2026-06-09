#include "ChannelChartPanel.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include <functional>

namespace {

QCheckBox* makeCheck(const QString& text, const bool checked, QWidget* parent) {
    auto* check = new QCheckBox(text, parent);
    check->setChecked(checked);
    return check;
}

void drawSeries(QPainter& painter,
                const QVector<ChannelChartPanel::LayerStats>& layers,
                const QRect& plot,
                const int max_value,
                const QColor& color,
                const std::function<int(const ChannelChartPanel::LayerStats&)>& select) {
    if (layers.size() < 2 || max_value <= 0) {
        return;
    }
    QPainterPath path;
    auto point_for = [&](const int index) {
        const double x = plot.left() + (plot.width() * index) / static_cast<double>(layers.size() - 1);
        const double y = plot.bottom() - (plot.height() * select(layers.at(index))) / static_cast<double>(max_value);
        return QPointF(x, y);
    };
    path.moveTo(point_for(0));
    for (int i = 1; i < layers.size(); ++i) {
        path.lineTo(point_for(i));
    }
    painter.setPen(QPen(color, 2));
    painter.drawPath(path);
}

}  // namespace

ChannelChartPanel::ChannelChartPanel(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();
    rgb_ = makeCheck("RGB", true, this);
    white_ = makeCheck("W 白墨", true, this);
    varnish_ = makeCheck("V 光油", true, this);
    support_ = makeCheck("S 支撑", true, this);
    controls->addWidget(rgb_);
    controls->addWidget(white_);
    controls->addWidget(varnish_);
    controls->addWidget(support_);
    controls->addStretch(1);
    layout->addLayout(controls);

    status_ = new QLabel("尚未加载材料工艺报告。", this);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    setMinimumHeight(360);

    connect(rgb_, &QCheckBox::toggled, this, qOverload<>(&QWidget::update));
    connect(white_, &QCheckBox::toggled, this, qOverload<>(&QWidget::update));
    connect(varnish_, &QCheckBox::toggled, this, qOverload<>(&QWidget::update));
    connect(support_, &QCheckBox::toggled, this, qOverload<>(&QWidget::update));
}

void ChannelChartPanel::loadPackage(const PackageSummary& package) {
    layers_.clear();
    const QString path = findMaterialProcessReport(package);
    if (path.isEmpty()) {
        status_message_ = "未找到 material_process_report.json。";
        status_->setText(status_message_);
        update();
        return;
    }

    const JsonReport report = loader_.load(path);
    if (!report.error.isEmpty()) {
        status_message_ = "读取材料工艺报告失败：" + report.error;
        status_->setText(status_message_);
        update();
        return;
    }
    const QJsonArray layers = report.document.object().value("layers").toArray();
    for (const QJsonValue& value : layers) {
        const QJsonObject layer = value.toObject();
        layers_.push_back(LayerStats{layer.value("layerIndex").toInt(),
                                     layer.value("rgbPrintPixels").toInt(),
                                     layer.value("whitePrintPixels").toInt(),
                                     layer.value("varnishPrintPixels").toInt(),
                                     layer.value("supportPrintPixels").toInt()});
    }
    status_message_ = QString("已加载 %1 层材料统计：%2").arg(layers_.size()).arg(QFileInfo(path).fileName());
    status_->setText(status_message_);
    update();
}

void ChannelChartPanel::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect plot = rect().adjusted(54, 88, -24, -34);
    painter.fillRect(plot, QColor(250, 250, 250));
    painter.setPen(QColor(180, 180, 180));
    painter.drawRect(plot);

    if (layers_.isEmpty()) {
        painter.setPen(Qt::darkGray);
        painter.drawText(plot, Qt::AlignCenter, status_message_.isEmpty() ? "没有可绘制数据。" : status_message_);
        return;
    }

    const int max_value = maxValue();
    painter.setPen(Qt::darkGray);
    painter.drawText(QRect(8, plot.top(), 48, 24), QString::number(max_value));
    painter.drawText(QRect(8, plot.bottom() - 18, 48, 24), "0");

    if (rgb_->isChecked()) {
        drawSeries(painter, layers_, plot, max_value, QColor(40, 120, 220), [](const LayerStats& s) { return s.rgb; });
    }
    if (white_->isChecked()) {
        drawSeries(painter, layers_, plot, max_value, QColor(20, 160, 120), [](const LayerStats& s) { return s.white; });
    }
    if (varnish_->isChecked()) {
        drawSeries(painter, layers_, plot, max_value, QColor(190, 80, 190), [](const LayerStats& s) { return s.varnish; });
    }
    if (support_->isChecked()) {
        drawSeries(painter, layers_, plot, max_value, QColor(230, 140, 20), [](const LayerStats& s) { return s.support; });
    }

    if (hover_index_ >= 0 && hover_index_ < layers_.size()) {
        const double x = plot.left() + (plot.width() * hover_index_) / static_cast<double>(qMax(1, layers_.size() - 1));
        painter.setPen(QPen(QColor(60, 60, 60), 1, Qt::DashLine));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
}

void ChannelChartPanel::mouseMoveEvent(QMouseEvent* event) {
    if (layers_.isEmpty()) {
        return;
    }
    const QRect plot = rect().adjusted(54, 88, -24, -34);
    if (!plot.contains(event->pos())) {
        hover_index_ = -1;
        status_->setText(status_message_);
        update();
        return;
    }
    const double ratio = (event->pos().x() - plot.left()) / static_cast<double>(qMax(1, plot.width()));
    hover_index_ = qBound(0, static_cast<int>(ratio * (layers_.size() - 1) + 0.5), layers_.size() - 1);
    const LayerStats& layer = layers_.at(hover_index_);
    status_->setText(QString("层 %1：RGB=%2 W=%3 V=%4 S=%5")
                         .arg(layer.layer)
                         .arg(layer.rgb)
                         .arg(layer.white)
                         .arg(layer.varnish)
                         .arg(layer.support));
    update();
}

QString ChannelChartPanel::findMaterialProcessReport(const PackageSummary& package) const {
    for (const QString& path : package.report_paths) {
        if (QFileInfo(path).fileName() == "material_process_report.json") {
            return path;
        }
    }
    return {};
}

int ChannelChartPanel::maxValue() const {
    int max_value = 1;
    for (const LayerStats& layer : layers_) {
        max_value = qMax(max_value, layer.rgb);
        max_value = qMax(max_value, layer.white);
        max_value = qMax(max_value, layer.varnish);
        max_value = qMax(max_value, layer.support);
    }
    return max_value;
}

QPointF ChannelChartPanel::pointFor(const int index, const int value, const QRect& plot, const int max_value) const {
    const double x = plot.left() + (plot.width() * index) / static_cast<double>(qMax(1, layers_.size() - 1));
    const double y = plot.bottom() - (plot.height() * value) / static_cast<double>(qMax(1, max_value));
    return QPointF(x, y);
}
