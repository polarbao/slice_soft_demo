#include "ConfigDocument.h"

#include <QFile>
#include <QJsonObject>

namespace {

bool setNested(QJsonObject& object, const QStringList& path, const int index, const QJsonValue& value) {
    if (index >= path.size()) {
        return false;
    }
    const QString key = path.at(index);
    if (index == path.size() - 1) {
        object.insert(key, value);
        return true;
    }
    QJsonObject child = object.value(key).toObject();
    if (!setNested(child, path, index + 1, value)) {
        return false;
    }
    object.insert(key, child);
    return true;
}

}  // namespace

ConfigDocument::ConfigDocument(QObject* parent) : QObject(parent) {}

bool ConfigDocument::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error_ = "无法打开配置文件：" + path;
        return false;
    }
    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        error_ = "配置 JSON 解析失败：" + parse_error.errorString();
        return false;
    }
    document_ = document;
    path_ = path;
    error_.clear();
    setDirty(false);
    emit changed();
    publishValidation(validate());
    return true;
}

bool ConfigDocument::save(QWidget* parent) {
    Q_UNUSED(parent);
    if (path_.isEmpty()) {
        error_ = "当前配置没有保存路径，请使用另存为。";
        return false;
    }
    return saveAs(path_, parent);
}

bool ConfigDocument::saveAs(const QString& path, QWidget* parent) {
    Q_UNUSED(parent);
    const ConfigValidationResult result = validate();
    publishValidation(result);
    if (!result.isValid()) {
        error_ = "配置校验失败，禁止保存。";
        return false;
    }
    if (!writeToPath(path)) {
        return false;
    }
    path_ = path;
    setDirty(false);
    emit changed();
    return true;
}

bool ConfigDocument::revert() {
    if (path_.isEmpty()) {
        error_ = "当前配置没有可回退路径。";
        return false;
    }
    return load(path_);
}

QJsonValue ConfigDocument::value(const QStringList& path) const {
    QJsonValue current = document_.object();
    for (const QString& key : path) {
        if (!current.isObject()) {
            return {};
        }
        current = current.toObject().value(key);
    }
    return current;
}

void ConfigDocument::setValue(const QStringList& path, const QJsonValue& value) {
    if (path.isEmpty() || !document_.isObject()) {
        return;
    }
    QJsonObject root = document_.object();
    if (!setNested(root, path, 0, value)) {
        return;
    }
    document_.setObject(root);
    setDirty(true);
    emit changed();
    publishValidation(validate());
}

bool ConfigDocument::isDirty() const {
    return dirty_;
}

QString ConfigDocument::path() const {
    return path_;
}

QString ConfigDocument::errorString() const {
    return error_;
}

QJsonDocument ConfigDocument::document() const {
    return document_;
}

ConfigValidationResult ConfigDocument::validate() const {
    if (!document_.isObject()) {
        ConfigValidationResult result;
        result.errors.push_back("配置文档为空或不是 JSON object。");
        return result;
    }
    return ConfigValidator::validate(document_.object());
}

void ConfigDocument::setDirty(const bool dirty) {
    if (dirty_ == dirty) {
        return;
    }
    dirty_ = dirty;
    emit dirtyChanged(dirty_);
}

void ConfigDocument::publishValidation(const ConfigValidationResult& result) {
    emit validationChanged(result.warnings, result.errors);
}

bool ConfigDocument::writeToPath(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        error_ = "无法写入配置文件：" + path;
        return false;
    }
    file.write(document_.toJson(QJsonDocument::Indented));
    error_.clear();
    return true;
}
