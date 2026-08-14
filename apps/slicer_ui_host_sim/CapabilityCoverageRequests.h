#pragma once

#include <QJsonObject>
#include <QString>

struct capabilitycoveragefixture
{
    QString modelpath;
    QString packagedirectory;
    QString previewpath;
    QString modelid;
    QString scenehash;
    QString profilehash;
    quint64 scenerevision{0};
    quint64 scenehandle{0};
    QJsonObject committedscene;
    QJsonObject profile;
};

/** @brief 用于冻结能力请求 DTO 的宿主侧构建器。 */
class CapabilityCoverageRequests final
{
public:
    /**
     * @brief 在模型导入前初始化测试夹具路径。
     * @param repositoryRoot SliceSoft 仓库根目录。
     * @param evidenceRoot 宿主可写的证据根目录。
     * @param fixture 接收规范化后的测试夹具路径。
     * @param error 接收路径验证错误。
     * @return 当参考模型存在且输出路径有效时为 true。
     */
    static bool InitializePaths(
        const QString& repositoryRoot,
        const QString& evidenceRoot,
        capabilitycoveragefixture* fixture,
        QString* error);

    /**
     * @brief 绑定已导入模型的标识，并构建参考 Profile。
     * @param imported 成功的 model.import 响应。
     * @param fixture 输入并接收更新后的宿主测试夹具状态。
     * @param error 接收元数据格式错误或 Profile 构建失败信息。
     * @return 模型标识与默认 Profile 均已就绪时返回 true。
     */
    static bool BindImportedModel(
        const QJsonObject& imported,
        capabilitycoveragefixture* fixture,
        QString* error);

    /**
     * @brief 按指定层厚构建包含自身哈希的 Profile。
     * @param fixture 已绑定的宿主测试夹具。
     * @param layerThicknessMm 以毫米为单位的正数层厚。
     * @param profile 接收 Profile 对象。
     * @param profileHash 接收冻结的 Profile 标识。
     * @param error 接收构建器或 JSON 错误。
     * @return Profile 是有效 JSON 时返回 true。
     */
    static bool BuildProfile(
        const capabilitycoveragefixture& fixture,
        double layerThicknessMm,
        QJsonObject* profile,
        QString* profileHash,
        QString* error);
};
