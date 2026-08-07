#pragma once

#include "ModuleClient.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

/**
 * @brief Executes the frozen Stage 14 capability checklist through ModuleClient.
 *
 * The runner is intentionally host-only. It uses public SPI requests and never
 * links or includes SliceSoft implementation types.
 */
class CapabilityCoverageRunner final
{
public:
    /**
     * @brief Creates a coverage runner for an already opened module client.
     * @param client Runtime-loaded public SPI client.
     */
    explicit CapabilityCoverageRunner(ModuleClient& client);

    /**
     * @brief Runs P0/P1 end-to-end checks, P2 recorded calls and UI-M5.
     * @param repositoryRoot SliceSoft repository root containing test fixtures.
     * @param evidenceRoot Host-owned directory for the generated package/report.
     * @param report Receives the complete machine-readable coverage report.
     * @param error Receives the first blocking failure.
     * @return True when all mandatory Stage 14E-04b gates pass.
     */
    bool Run(
        const QString& repositoryRoot,
        const QString& evidenceRoot,
        QByteArray* report,
        QString* error);

private:
    struct invocationresult
    {
        bool transportok{false};
        bool terminal{false};
        QString terminalstate;
        QString code;
        QJsonObject payload;
        qint64 elapsedms{0};
    };

    bool ExecuteJob(
        const QJsonObject& request,
        int timeoutMs,
        invocationresult* result,
        QString* error);
    bool RunCancellationGate(
        const QJsonObject& request,
        const QString& evidenceRoot,
        QJsonObject* evidence,
        QString* error);
    void Record(
        const QString& tier,
        const QString& capability,
        const QString& carrier,
        const QString& requirement,
        const invocationresult& result,
        bool satisfied);
    bool WriteEvidence(
        const QString& evidenceRoot,
        const QJsonObject& report,
        QString* error) const;

    ModuleClient& m_client;
    QJsonArray m_entries;
};
