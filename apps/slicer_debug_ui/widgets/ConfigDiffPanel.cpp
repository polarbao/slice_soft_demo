#include "ConfigDiffPanel.h"

#include "../services/ConfigDiffModel.h"

#include <QHeaderView>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ConfigDiffPanel::ConfigDiffPanel(ConfigDocument* document, QWidget* parent) : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);
    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"路径", "原值", "新值"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    layout->addWidget(table_);
    connect(document_, &ConfigDocument::changed, this, &ConfigDiffPanel::refresh);
}

int ConfigDiffPanel::diffCount() const {
    return table_->rowCount();
}

void ConfigDiffPanel::refresh() {
    const QVector<ConfigDiffEntry> entries = ConfigDiffModel::diff(document_->originalDocument(), document_->document());
    table_->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        table_->setItem(row, 0, new QTableWidgetItem(entries.at(row).path));
        table_->setItem(row, 1, new QTableWidgetItem(entries.at(row).old_value));
        table_->setItem(row, 2, new QTableWidgetItem(entries.at(row).new_value));
    }
}
