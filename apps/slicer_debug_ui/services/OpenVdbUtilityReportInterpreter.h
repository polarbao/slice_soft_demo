#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

/**
 * @brief Result of recognizing, validating, and summarizing an OpenVDB utility report.
 */
struct OpenVdbUtilityReportInterpretation
{
    bool recognized{false};
    bool valid{false};
    QString summary;
    QStringList errors;
};

/**
 * @brief Strictly interprets the diagnostic-only OpenVDB SDF utility report schema.
 */
class OpenVdbUtilityReportInterpreter final
{
public:
    /**
     * @brief Recognize and validate an OpenVDB utility report JSON object.
     * @param object Root JSON object from a report.
     * @return Recognition state, validation result, Chinese summary, and field errors.
     */
    static OpenVdbUtilityReportInterpretation Interpret(const QJsonObject& object);
};
