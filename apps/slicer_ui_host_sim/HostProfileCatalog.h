#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

/** @brief 由宿主持有的单个可选生产 Profile 描述。 */
struct hostprofiledescriptor
{
    QString profileid;
    QString displayname;
    QString description;
    QString productionsafety;
    QStringList tags;
    QStringList requiredcapabilities;
    QString usage;
    QString defaultprocess;
    QString outputcontract;
    QString limitations;
};

/** @brief 单个宿主 Profile 的能力交集结果。 */
struct hostprofileavailability
{
    hostprofiledescriptor profile;
    bool available{false};
    QStringList missingcapabilities;
};

/** @brief 已解析的模块能力与宿主 Profile 可用性。 */
struct hostprofilecatalogresolution
{
    QStringList modulecapabilities;
    QList<hostprofileavailability> profiles;
};

/**
 * @brief 抽象的宿主侧 Profile 目录。
 *
 * 公共切片模块仅声明能力。打印宿主持有设备/材料 Profile，
 * 并通过本接口提供这些 Profile。
 */
class IHostProfileCatalog
{
public:
    /** @brief 释放宿主 Profile 提供者。 */
    virtual ~IHostProfileCatalog() = default;

    /**
     * @brief 返回宿主应用持有的 Profile。
     * @return 当前宿主会话的稳定 Profile 描述符。
     */
    virtual QList<hostprofiledescriptor> Profiles() const = 0;
};

/** @brief 独立于切片器内部实现的参考宿主 Profile fixture。 */
class ReferenceHostProfileCatalog final : public IHostProfileCatalog
{
public:
    /**
     * @brief 返回参考宿主的公共 Profile fixture。
     * @return 生产、受限与诊断 Profile 描述符。
     */
    QList<hostprofiledescriptor> Profiles() const override;
};

/** @brief 根据结构化模块信息解析宿主 Profile。 */
class HostProfileCapabilityResolver final
{
public:
    /**
     * @brief 计算宿主 Profile 要求与 pm_module_info 提供能力的交集。
     * @param catalog 由宿主持有的 Profile 目录。
     * @param moduleInfo UTF-8 slicesoft.module_info.1 载荷。
     * @param resolution 接收能力与逐 Profile 可用性。
     * @param error 接收失败即拒绝的校验原因。
     * @return 两项输入结构均有效时返回 true。
     */
    static bool Resolve(
        const IHostProfileCatalog& catalog,
        const QByteArray& moduleInfo,
        hostprofilecatalogresolution* resolution,
        QString* error);
};
