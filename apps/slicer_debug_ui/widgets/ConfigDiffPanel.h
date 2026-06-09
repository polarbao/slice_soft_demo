#pragma once

#include "../services/ConfigDocument.h"

#include <QTableWidget>
#include <QWidget>

class ConfigDiffPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ConfigDiffPanel(ConfigDocument* document, QWidget* parent = nullptr);
    int diffCount() const;

public slots:
    void refresh();

private:
    ConfigDocument* document_{nullptr};
    QTableWidget* table_{nullptr};
};
