#pragma once

#include "../services/PackageLoader.h"
#include "../services/ReportLoader.h"

#include <QCheckBox>
#include <QLabel>
#include <QVector>
#include <QWidget>

class ChannelChartPanel final : public QWidget {
    Q_OBJECT

public:
    struct LayerStats {
        int layer{0};
        int rgb{0};
        int white{0};
        int varnish{0};
        int support{0};
    };

    explicit ChannelChartPanel(QWidget* parent = nullptr);
    void loadPackage(const PackageSummary& package);
    int layerStatCount() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QString findMaterialProcessReport(const PackageSummary& package) const;
    int maxValue() const;
    QPointF pointFor(int index, int value, const QRect& plot, int max_value) const;

    ReportLoader loader_;
    QVector<LayerStats> layers_;
    QString status_message_;
    int hover_index_{-1};

    QCheckBox* rgb_{nullptr};
    QCheckBox* white_{nullptr};
    QCheckBox* varnish_{nullptr};
    QCheckBox* support_{nullptr};
    QLabel* status_{nullptr};
};
