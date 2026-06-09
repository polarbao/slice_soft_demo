#pragma once

#include <QJsonObject>
#include <QStringList>

struct ConfigValidationResult {
    QStringList warnings;
    QStringList errors;

    bool isValid() const { return errors.isEmpty(); }
};

class ConfigValidator {
public:
    static ConfigValidationResult validate(const QJsonObject& root);
};
