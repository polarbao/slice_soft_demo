#pragma once

#include "HostSliceSettings.h"
#include "ModuleClient.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QJsonObject;
class QJsonArray;

/** @brief 同步模型预检返回的单个问题。 */
struct hostpreflightissue
{
    QString code;
    QString severity;
    quint64 count{0};
    QString detail;
};

/** @brief 宿主持有的单个已导入模型实例展示数据。 */
struct hostmodelimportresult
{
    QString sourcepath;
    QString modelid;
    QString instanceid;
    QString admission;
    quint64 trianglecount{0};
    quint64 vertexcount{0};
    bool hasuv{false};
    bool hasnormals{false};
    double widthmm{0.0};
    double heightmm{0.0};
    double depthmm{0.0};
    QString appearancestatus;
    bool singlematerialonly{false};
    QString appearancedetail;
    QStringList texturepaths;
    QList<hostpreflightissue> issues;
};

/** @brief 提交给选定场景实例的增量变换值。 */
struct hosttransformrequest
{
    double deltaxmm{0.0};
    double deltaymm{0.0};
    double deltazmm{0.0};
    double rotatexdegrees{0.0};
    double rotateydegrees{0.0};
    double rotatezdegrees{0.0};
    double uniformscalefactor{1.0};
    bool mirrorx{false};
    bool mirrory{false};
    bool landonbuildplate{true};
};

/** @brief 宿主 UI 拥有的确定性网格布局值。 */
struct hostgridlayoutrequest
{
    int maxcolumns{11};
    int maxrows{2};
    double columngapmm{10.0};
    double rowgapmm{10.0};
};

/** @brief 单次场景提交返回的权威摘要。 */
struct hostsceneeditresult
{
    quint64 scenerevision{0};
    QString scenehash;
    QString viewdataidentity;
    int collisioncount{0};
    int outofboundscount{0};
};

/**
 * @brief 运行公共 SPI 模型导入、场景准入和快速预检流程。
 *
 * 工作流仅持有宿主会话状态，不包含实现层类型，
 * 也不构造规范生产场景 JSON。
 */
class HostModelImportWorkflow final
{
public:
    /**
     * @brief 创建绑定到已加载模块客户端的工作流。
     * @param client 连接至持有场景的模块会话的公共 ABI 客户端。
     */
    explicit HostModelImportWorkflow(ModuleClient& client);

    /**
     * @brief 导入 OBJ、3MF 或 STL 模型并添加一个场景实例。
     * @param modelPath 操作员选择的现有模型路径。
     * @param result 接收模型元数据、实例标识与预检数据。
     * @param error 接收用户可读的失败即拒绝原因。
     * @return 当导入、addInstance 和快速预检全部完成时为 true。
     */
    bool ImportModel(
        const QString& modelPath,
        hostmodelimportresult* result,
        QString* error);

    /**
     * @brief 通过一个原子场景提交导入并接纳多个模型。
     * @param modelPaths 按操作员顺序排列的现有 OBJ、3MF 或 STL 路径。
     * @param results 按路径接收对应的元数据与预检结果。
     * @param error 接收失败即拒绝原因；失败时不会添加任何实例。
     * @return 当每个资源都通过预检并且所有实例都提交时为 true。
     */
    bool ImportModels(
        const QStringList& modelPaths,
        QList<hostmodelimportresult>* results,
        QString* error);

    /**
     * @brief 以原子方式删除现有场景实例。
     * @param instanceIds 宿主选择的稳定实例标识。
     * @param error 接收用户可读的失败即拒绝原因。
     * @return 当模块提交完整的删除集时为 true。
     */
    bool RemoveInstances(
        const QStringList& instanceIds,
        QString* error);

    /**
     * @brief 以原子方式将增量变换应用于选定实例。
     * @param instanceIds 宿主选择的稳定实例标识。
     * @param request 有限平移、旋转、缩放和镜像命令。
     * @param result 接收权威提交摘要。
     * @param error 接收用户可读的失败即拒绝原因。
     * @return 当所有请求的操作在一个修订版中提交时为 true。
     */
    bool ApplyTransforms(
        const QStringList& instanceIds,
        const hosttransformrequest& request,
        hostsceneeditresult* result,
        QString* error);

    /** @brief 将选中实例的主体落点贴到构建平台 Z=0。 */
    bool LandOnBuildPlate(
        const QStringList& instanceIds,
        hostsceneeditresult* result,
        QString* error);

    /**
     * @brief 通过公共 SPI 应用权威的 11x2 网格布局。
     * @param request 由宿主持有的有效行、列和间距值。
     * @param result 接收权威提交摘要。
     * @param error 接收用户可读的失败即拒绝原因。
     * @return 当 applyGridLayout 推进场景一次时为 true。
     */
    bool ApplyGridLayout(
        const hostgridlayoutrequest& request,
        hostsceneeditresult* result,
        QString* error);

    /**
     * @brief 第一次导入后返回模块拥有的场景句柄。
     * @return 创建场景前返回零。
     */
    /// @brief 解绑当前场景，释放全部已导入模型，使 Profile/buildVolume 可重新选择。
    ///
    /// 场景在首次提交时把 Profile 与 buildVolume 钉死，此后 PrepareSceneContext
    /// 会以「当前场景已绑定 Profile/buildVolume；请新建场景后修改」拒绝变更。
    /// 但此前【没有任何接口能新建场景】，且 RemoveInstances 只删实例、不清 m_sceneHandle，
    /// 于是删光模型也退不出该状态——软件要求的操作在 UI 上不存在。本方法即该出口。
    void ResetScene();

    [[nodiscard]] quint64 SceneHandle() const;

    /**
     * @brief 返回最新提交的场景修订号。
     * @return 单调递增的场景修订号，初始值为零。
     */
    [[nodiscard]] quint64 SceneRevision() const;

    /** @brief 返回宿主跟踪的已导入实例数量。 */
    [[nodiscard]] int InstanceCount() const;

    /**
     * @brief 设置首次场景提交使用的宿主上下文。
     * @param profileId 选定的宿主 Profile 标识。
     * @param buildVolume 由设备持有的构建体积。
     * @param error 接收不可变场景或验证原因。
     * @return 当待处理的上下文被接受时为 true。
     */
    bool SetPendingSceneContext(
        const QString& profileId,
        const hostbuildvolume& buildVolume,
        QString* error);

    /** @brief 返回绑定到当前场景的 Profile 标识。 */
    [[nodiscard]] QString SceneProfileId() const;

    /** @brief 返回绑定到当前场景的设备构建体积。 */
    [[nodiscard]] hostbuildvolume SceneBuildVolume() const;

    /**
     * @brief 返回可作为有效 Profile 输入的稳定已导入模型路径。
     * @return 返回当前源路径中按字典序最先的一项；无可用路径时返回空字符串。
     */
    [[nodiscard]] QString ReferenceModelPath() const;

    /**
     * @brief 返回当前场景实例使用的不同源纹理。
     * @return 返回规范化后并按确定性字典序排列的纹理路径。
     */
    [[nodiscard]] QStringList TexturePaths() const;

    /**
     * @brief 判断当前场景是否包含只能采用单材料工艺的模型。
     * @return 任一实例的彩色外观资源不完整时返回 true。
     */
    [[nodiscard]] bool RequiresSingleMaterialProcess() const;

    /**
     * @brief 返回当前场景单材料限制的确定性诊断摘要。
     * @return 无限制时返回空字符串，否则返回去重并排序后的原因。
     */
    [[nodiscard]] QString SingleMaterialRestrictionSummary() const;

    /**
     * @brief 采用另一个宿主控制器提交的场景修订。
     * @param sceneHandle 现有模块拥有的场景句柄。
     * @param sceneRevision 提交或恢复后的新权威修订号。
     * @param error 接收标识不一致或修订号非单调递增的原因。
     * @return 工作流成功采用同一权威修订号时返回 true。
     */
    bool AdoptSceneState(
        quint64 sceneHandle,
        quint64 sceneRevision,
        QString* error);

private:
    bool ExecuteObject(
        const QJsonObject& request,
        QJsonObject* response,
        QString* error);
    bool ImportResource(
        const QString& modelPath,
        hostmodelimportresult* result,
        QString* error);
    bool CommitImportedInstances(
        QList<hostmodelimportresult>* results,
        QString* error);
    bool RunFastPreflight(
        const QString& modelId,
        hostmodelimportresult* result,
        QString* error);
    bool CommitSceneOperations(
        const QJsonArray& operations,
        hostsceneeditresult* result,
        QString* error);
    void ReleaseImportedModels(
        const QList<hostmodelimportresult>& results);

    ModuleClient& m_client;
    QHash<QString, QString> m_instanceModels;
    QHash<QString, QString> m_instanceSources;
    QHash<QString, QStringList> m_instanceTexturePaths;
    QHash<QString, QString> m_instanceMaterialRestrictions;
    QString m_pendingProfileId{QStringLiteral("host-reference-default")};
    hostbuildvolume m_pendingBuildVolume;
    QString m_sceneProfileId;
    hostbuildvolume m_sceneBuildVolume;
    quint64 m_sceneHandle{0};
    quint64 m_sceneRevision{0};
};
