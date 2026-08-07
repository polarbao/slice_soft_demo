#include "CapabilityCoverageRunner.h"

#include <QDir>
#include <QJsonDocument>

bool CapabilityCoverageRunner::FinalizeCoverage(
    const QString& runRoot,
    const QJsonObject& cancelEvidence,
    QByteArray* reportBytes,
    QString* error)
{
    int p0Passed = 0;
    int p1Passed = 0;
    int p2Recorded = 0;
    for (const QJsonValue& entryValue : m_entries)
    {
        const QJsonObject entry = entryValue.toObject();
        if (!entry.value(QStringLiteral("satisfied")).toBool())
        {
            continue;
        }
        const QString tier = entry.value(QStringLiteral("tier")).toString();
        p0Passed += tier == QStringLiteral("P0") ? 1 : 0;
        p1Passed += tier == QStringLiteral("P1") ? 1 : 0;
        p2Recorded += tier == QStringLiteral("P2") ? 1 : 0;
    }
    const bool coveragePassed = p0Passed == 5 && p1Passed == 5
        && p2Recorded == 6;
    const QJsonObject report{
        {QStringLiteral("schema"),
         QStringLiteral("slicesoft.host_capability_coverage.14e04b.1")},
        {QStringLiteral("task"), QStringLiteral("14E-04b")},
        {QStringLiteral("runRoot"), QDir::fromNativeSeparators(runRoot)},
        {QStringLiteral("moduleCallCount"),
         static_cast<qint64>(m_client.CallCount())},
        {QStringLiteral("entries"), m_entries},
        {QStringLiteral("uiM5"), cancelEvidence},
        {QStringLiteral("uiM6"), QJsonObject{
             {QStringLiteral("coveredBy"),
              QStringLiteral("slicer_stage14e02_qt_host_missing_module_test")},
             {QStringLiteral("required"), true}}},
        {QStringLiteral("summary"), QJsonObject{
             {QStringLiteral("p0Passed"), p0Passed},
             {QStringLiteral("p0Required"), 5},
             {QStringLiteral("p1Passed"), p1Passed},
             {QStringLiteral("p1Required"), 5},
             {QStringLiteral("p2Recorded"), p2Recorded},
             {QStringLiteral("p2Required"), 6},
             {QStringLiteral("passed"), coveragePassed}}}};
    if (reportBytes != nullptr)
    {
        *reportBytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    }
    if (!WriteEvidence(runRoot, report, error))
    {
        return false;
    }
    if (!coveragePassed && error != nullptr)
    {
        *error = QStringLiteral("14E-04b 能力计数未达标：P0=%1 P1=%2 P2=%3")
                     .arg(p0Passed).arg(p1Passed).arg(p2Recorded);
    }
    return coveragePassed;
}
