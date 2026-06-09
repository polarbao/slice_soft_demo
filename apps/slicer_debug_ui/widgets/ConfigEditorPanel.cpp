#include "ConfigEditorPanel.h"

#include "MaterialPolicyEditor.h"
#include "MaterialProcessProfileEditor.h"
#include "MaterialRoleMappingEditor.h"
#include "SupportEditor.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QPushButton* button(const QString& text, QWidget* parent) {
    auto* result = new QPushButton(text, parent);
    result->setMinimumHeight(28);
    return result;
}

}  // namespace

ConfigEditorPanel::ConfigEditorPanel(ConfigDocument* document, QWidget* parent) : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);

    auto* header = new QVBoxLayout();
    path_label_ = new QLabel("配置：未加载", this);
    path_label_->setWordWrap(true);
    dirty_label_ = new QLabel("状态：未修改", this);
    header->addWidget(path_label_);
    header->addWidget(dirty_label_);
    layout->addLayout(header);

    auto* actions = new QHBoxLayout();
    auto* save_button = button("保存", this);
    auto* save_as_button = button("另存为", this);
    auto* revert_button = button("回退", this);
    auto* validate_button = button("校验", this);
    actions->addWidget(save_button);
    actions->addWidget(save_as_button);
    actions->addWidget(revert_button);
    actions->addWidget(validate_button);
    actions->addStretch(1);
    layout->addLayout(actions);

    auto* tabs = new QTabWidget(this);
    profile_editor_ = new MaterialProcessProfileEditor(document_, tabs);
    policy_editor_ = new MaterialPolicyEditor(document_, tabs);
    role_mapping_editor_ = new MaterialRoleMappingEditor(document_, tabs);
    support_editor_ = new SupportEditor(document_, tabs);
    tabs->addTab(profile_editor_, "工艺 Profile");
    tabs->addTab(policy_editor_, "材料策略");
    tabs->addTab(role_mapping_editor_, "材料角色");
    tabs->addTab(support_editor_, "支撑");
    layout->addWidget(tabs, 1);

    validation_view_ = new QPlainTextEdit(this);
    validation_view_->setReadOnly(true);
    validation_view_->setMaximumHeight(120);
    layout->addWidget(validation_view_);

    connect(save_button, &QPushButton::clicked, this, &ConfigEditorPanel::save);
    connect(save_as_button, &QPushButton::clicked, this, &ConfigEditorPanel::saveAs);
    connect(revert_button, &QPushButton::clicked, this, &ConfigEditorPanel::revert);
    connect(validate_button, &QPushButton::clicked, this, &ConfigEditorPanel::validate);
    connect(document_, &ConfigDocument::dirtyChanged, this, &ConfigEditorPanel::updateDirty);
    connect(document_, &ConfigDocument::validationChanged, this, &ConfigEditorPanel::updateValidation);
}

bool ConfigEditorPanel::loadConfig(const QString& path) {
    if (!document_->load(path)) {
        emit statusMessage("加载配置失败：" + document_->errorString());
        return false;
    }
    path_label_->setText("配置：" + document_->path());
    refreshEditors();
    emit configPathChanged(document_->path());
    emit statusMessage("已加载配置：" + document_->path());
    return true;
}

QString ConfigEditorPanel::configPath() const {
    return document_->path();
}

void ConfigEditorPanel::save() {
    if (!document_->save(this)) {
        emit statusMessage("保存失败：" + document_->errorString());
        return;
    }
    path_label_->setText("配置：" + document_->path());
    emit configPathChanged(document_->path());
    emit statusMessage("配置已保存。");
}

void ConfigEditorPanel::saveAs() {
    const QString initial = document_->path().isEmpty() ? QString() : document_->path();
    const QString path = QFileDialog::getSaveFileName(this, "另存配置", initial, "JSON (*.json)");
    if (path.isEmpty()) {
        return;
    }
    if (!document_->saveAs(path, this)) {
        emit statusMessage("另存失败：" + document_->errorString());
        return;
    }
    path_label_->setText("配置：" + document_->path());
    emit configPathChanged(document_->path());
    emit statusMessage("配置已另存为：" + document_->path());
}

void ConfigEditorPanel::revert() {
    if (!document_->revert()) {
        emit statusMessage("回退失败：" + document_->errorString());
        return;
    }
    path_label_->setText("配置：" + document_->path());
    refreshEditors();
    emit statusMessage("配置已回退到磁盘版本。");
}

void ConfigEditorPanel::validate() {
    const ConfigValidationResult result = document_->validate();
    updateValidation(result.warnings, result.errors);
    emit statusMessage(result.isValid() ? "配置校验通过。" : "配置校验失败。");
}

void ConfigEditorPanel::updateDirty(const bool dirty) {
    dirty_label_->setText(dirty ? "状态：已修改，尚未保存" : "状态：未修改");
}

void ConfigEditorPanel::updateValidation(const QStringList& warnings, const QStringList& errors) {
    QStringList lines;
    if (errors.isEmpty() && warnings.isEmpty()) {
        lines.push_back("配置校验通过。");
    }
    for (const QString& error : errors) {
        lines.push_back("错误：" + error);
    }
    for (const QString& warning : warnings) {
        lines.push_back("警告：" + warning);
    }
    validation_view_->setPlainText(lines.join('\n'));
}

void ConfigEditorPanel::refreshEditors() {
    profile_editor_->loadFromDocument();
    policy_editor_->loadFromDocument();
    role_mapping_editor_->loadFromDocument();
    support_editor_->loadFromDocument();
    validate();
}
