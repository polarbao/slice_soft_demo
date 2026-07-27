#pragma once

#include "../models/SceneDocument.h"
#include "../models/SceneProjectionRequest.h"
#include "SceneModelRepository.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

struct ModelTopViewLoadRequest
{
    QString configpath;
    QString modelpath;
    QString sceneid;
    QString modelid;
    QString instanceid;
    quint64 scenerevision{0U};
    quint64 transformrevision{0U};
    slicer_core::ModelTransform transform;
    bool locked{false};
    bool appendtoscene{false};
    slicer_core::SceneViewAdmissionStatus admissionstatus{
        slicer_core::SceneViewAdmissionStatus::Unknown};
};

class ModelTopViewLoader final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create an asynchronous model importer and top-view builder.
     * @param document UI-thread scene document receiving current results.
     * @param parent QObject owner.
     */
    explicit ModelTopViewLoader(
        SceneDocument* document,
        SceneModelRepository* repository,
        QObject* parent = nullptr);
    ~ModelTopViewLoader() override;

    /**
     * @brief Load and project one model without starting the slice process.
     * @param request Config context, model path, identity, and revision.
     */
    void RequestLoad(const ModelTopViewLoadRequest& request);

    /**
     * @brief Rebuild the top view from an immutable cached source.
     * @param request Scene identity and current instance transform.
     */
    void RequestProjection(const SceneProjectionRequest& request);

    /**
     * @brief Cancel the current logical request.
     */
    void Cancel();

    /**
     * @brief Report whether a model import is active.
     * @return True while the newest request is running.
     */
    bool IsRunning() const;

    /**
     * @brief Return the current request generation.
     * @return Monotonic generation.
     */
    quint64 Generation() const;

signals:
    void SigLoadingStarted();
    void SigLoadingFinished();

private:
    struct CallbackState;
    struct WorkerResult;

    void OnWorkerCompleted(quint64 generation, WorkerResult result);

    SceneDocument* m_document{nullptr};
    SceneModelRepository* m_repository{nullptr};
    std::shared_ptr<CallbackState> m_callbackState;
    std::shared_ptr<std::atomic_bool> m_activeCancellation;
    quint64 m_generation{0U};
    bool m_running{false};
};
