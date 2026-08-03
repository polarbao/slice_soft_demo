#pragma once

#include "ConfigValidator.h"

#include <QJsonDocument>
#include <QObject>
#include <QStringList>

class QWidget;

struct SaveOptions {
    bool allowOverwriteWithoutPrompt{false};
};

class ConfigDocument final : public QObject {
    Q_OBJECT

public:
    explicit ConfigDocument(QObject* parent = nullptr);

    bool load(const QString& path);
    bool save(QWidget* parent = nullptr, SaveOptions options = {});
    bool saveAs(const QString& path, QWidget* parent = nullptr, SaveOptions options = {});
    bool revert();
    QJsonValue value(const QStringList& path) const;
    void setValue(const QStringList& path, const QJsonValue& value);

    /**
     * @brief Atomically replace the editable root object.
     * @param root Complete configuration root.
     *
     * One changed and one validation notification are emitted after the
     * complete field group has been installed. This prevents observers from
     * seeing transiently inconsistent material or texture settings.
     */
    void ReplaceObject(const QJsonObject& root);

    bool isDirty() const;
    QString path() const;
    QString errorString() const;
    QJsonDocument document() const;
    QJsonDocument originalDocument() const;
    ConfigValidationResult validate() const;

signals:
    void changed();
    void dirtyChanged(bool dirty);
    void validationChanged(QStringList warnings, QStringList errors);

private:
    void setDirty(bool dirty);
    void publishValidation(const ConfigValidationResult& result);
    bool writeToPath(const QString& path);

    QJsonDocument document_;
    QJsonDocument original_document_;
    QString path_;
    QString error_;
    bool dirty_{false};
};
