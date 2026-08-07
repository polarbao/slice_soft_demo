#pragma once

#include "ModuleClient.h"

#include <QList>
#include <QString>

class QJsonObject;

/** @brief One issue returned by the synchronous model preflight. */
struct hostpreflightissue
{
    QString code;
    QString severity;
    quint64 count{0};
    QString detail;
};

/** @brief Host-owned presentation data for one imported model instance. */
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
    QList<hostpreflightissue> issues;
};

/**
 * @brief Runs the public-SPI model import, scene admission and fast preflight flow.
 *
 * The workflow owns only host session state. It never includes implementation
 * types and never constructs the canonical production scene JSON.
 */
class HostModelImportWorkflow final
{
public:
    /**
     * @brief Creates a workflow bound to one loaded module client.
     * @param client Public ABI client whose module session owns the scene.
     */
    explicit HostModelImportWorkflow(ModuleClient& client);

    /**
     * @brief Imports an OBJ or 3MF model and adds one instance to the scene.
     * @param modelPath Existing model path selected by the operator.
     * @param result Receives model metadata, instance identity and preflight data.
     * @param error Receives a user-readable fail-closed reason.
     * @return True when import, addInstance and fast preflight all complete.
     */
    bool ImportModel(
        const QString& modelPath,
        hostmodelimportresult* result,
        QString* error);

    /**
     * @brief Returns the module-owned scene handle after the first import.
     * @return Zero before a scene has been created.
     */
    [[nodiscard]] quint64 SceneHandle() const;

    /**
     * @brief Returns the latest committed scene revision.
     * @return Monotonic scene revision, initially zero.
     */
    [[nodiscard]] quint64 SceneRevision() const;

private:
    bool ExecuteObject(
        const QJsonObject& request,
        QJsonObject* response,
        QString* error);
    bool AddInstance(
        const QString& modelId,
        QString* instanceId,
        QString* error);
    bool RunFastPreflight(
        const QString& modelId,
        hostmodelimportresult* result,
        QString* error);
    void RollbackImport(
        const QString& modelId,
        const QString& instanceId);

    ModuleClient& m_client;
    quint64 m_sceneHandle{0};
    quint64 m_sceneRevision{0};
};
