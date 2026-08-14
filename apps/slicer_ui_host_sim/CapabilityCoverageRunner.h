#pragma once

#include "ModuleClient.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

/**
 * @brief 通过 ModuleClient 执行冻结的 Stage 14 能力检查清单。
 *
 * 本执行器仅属于宿主侧。它只使用公共 SPI 请求，
 * 不链接或包含 SliceSoft 实现类型。
 */
class CapabilityCoverageRunner final
{
public:
    /**
     * @brief 为已打开的模块客户端创建能力覆盖执行器。
     * @param client 运行时加载的公共 SPI 客户端。
     */
    explicit CapabilityCoverageRunner(ModuleClient& client);

    /**
     * @brief 运行 P0/P1 端到端检查、P2 调用记录检查和 UI-M5。
     * @param repositoryRoot 包含测试夹具的 SliceSoft 仓库根目录。
     * @param evidenceRoot 由宿主持有的 Package 与报告生成目录。
     * @param report 接收完整的机器可读覆盖率报告。
     * @param error 接收第一个阻塞失败。
     * @return 全部强制 14E-04b 门槛通过时返回 true。
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
    bool FinalizeCoverage(
        const QString& runRoot,
        const QJsonObject& cancelEvidence,
        QByteArray* reportBytes,
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
