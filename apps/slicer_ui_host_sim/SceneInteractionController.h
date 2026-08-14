#pragma once

#include "ModuleClient.h"
#include "TransformCommitPolicy.h"

#include <QJsonObject>
#include <QString>

/** @brief 一次用户变换提交的最终结果。 */
enum class CommitOutcome
{
    Committed,
    StaleRecovered,
    Failed
};

/**
 * @brief 协调本地瞬态变换和权威场景提交。
 */
class SceneInteractionController final
{
public:
    /**
     * @brief 在已加载的公共模块客户端上创建控制器。
     * @param client 运行时加载的公共 ABI 客户端。
     */
    explicit SceneInteractionController(ModuleClient& client);

    /**
     * @brief 引导模块场景并捕获其权威快照。
     * @param scene 冻结的多模型场景 JSON。
     * @param error 接收用户可读的失败原因。
     * @return 场景句柄、修订号和哈希均建立时返回 true。
     */
    bool Initialize(const QJsonObject& scene, QString* error);

    /**
     * @brief 将瞬态交互附加到现有的宿主持有场景。
     * @param sceneHandle 模块拥有的场景句柄。
     * @param sceneRevision 宿主已接受的最新权威修订号。
     * @return 当现有场景标识有效时为 true。
     */
    bool Attach(quint64 sceneHandle, quint64 sceneRevision);

    /**
     * @brief 启动实例的本地瞬态反馈。
     * @param instanceId 稳定的场景实例标识。
     * @return 瞬态状态初始化成功时返回 true。
     */
    bool BeginTransient(const QString& instanceId);

    /**
     * @brief 更新仅存在于本地的平移预览，不调用模块。
     * @param deltaXMm 沿设备 X 轴的平移量，单位为毫米。
     * @param deltaYMm 沿设备 Y 轴的平移量，单位为毫米。
     * @param deltaZMm 沿设备 Z 轴的平移量，单位为毫米。
     * @return 当活动瞬态预览已更新时为 true。
     */
    bool UpdateTransientTranslation(
        double deltaXMm,
        double deltaYMm,
        double deltaZMm);

    /**
     * @brief 以原子方式提交活动转换或执行过时恢复。
     * @param error 接收公共错误或恢复诊断。
     * @return 提交结果；发生 stale 恢复时绝不自动重试已变更载荷。
     */
    CommitOutcome CommitTransient(QString* error);

    /** @brief 丢弃本地瞬态而不进行模块调用。 */
    void DiscardTransient();

    /**
     * @brief 报告本地瞬态是否处于活动状态。
     * @return BeginTransient 之后、Commit 或 Discard 之前返回 true。
     */
    bool HasTransient() const;

    /**
     * @brief 返回最新的权威场景修订版。
     * @return 宿主已接受的当前模块修订号。
     */
    quint64 SceneRevision() const;

    /**
     * @brief 返回模块拥有的场景句柄。
     * @return 成功初始化后的非零场景句柄。
     */
    quint64 SceneHandle() const;

    /**
     * @brief 返回最新的权威场景哈希。
     * @return 已提交场景的公开生产标识。
     */
    QString SceneHash() const;

    /**
     * @brief 返回最新提交中采用的 ViewData 标识。
     * @return 快照刷新后为空；提交成功后为对应的 ViewData 标识。
     */
    QString ViewDataIdentity() const;

    /**
     * @brief 返回显式快照恢复/刷新读取的数量。
     * @return 快照计数；正常提交不得增加它。
     */
    quint64 SnapshotReadCount() const;

    /** @brief 返回来自最新提交的权威冲突计数。 */
    [[nodiscard]] int CollisionCount() const;

    /** @brief 返回最新提交的权威越界计数。 */
    [[nodiscard]] int OutOfBoundsCount() const;

private:
    bool Bootstrap(const QJsonObject& scene, QJsonObject* result, QString* error);
    bool RefreshSnapshot(QString* error);
    bool ExecuteSync(
        const QJsonObject& request,
        QJsonObject* result,
        QString* error);
    bool AdoptCommit(const QJsonObject& result, QString* error);
    bool AdoptSnapshot(const QJsonObject& result, QString* error);

    ModuleClient& m_client;
    TransformCommitPolicy m_transformPolicy;
    QString m_externalSceneId;
    QString m_sceneHash;
    QString m_viewDataIdentity;
    quint64 m_sceneHandle{0};
    quint64 m_sceneRevision{0};
    quint64 m_snapshotReadCount{0};
    int m_collisionCount{0};
    int m_outOfBoundsCount{0};
};
