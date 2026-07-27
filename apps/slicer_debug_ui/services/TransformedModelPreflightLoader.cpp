#include "TransformedModelPreflightLoader.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include <functional>
#include <stdexcept>
#include <utility>

namespace
{

class FunctionRunnable final : public QRunnable
{
public:
    explicit FunctionRunnable(std::function<void()> task)
        : m_task(std::move(task))
    {
    }

    void run() override
    {
        m_task();
    }

private:
    std::function<void()> m_task;
};

}  // namespace

struct TransformedModelPreflightLoader::CallbackState
{
    QMutex mutex;
    QPointer<TransformedModelPreflightLoader> loader;
};

TransformedModelPreflightLoader::TransformedModelPreflightLoader(
    SceneDocument* document,
    SceneModelRepository* repository,
    QObject* parent)
    : QObject(parent),
      m_document(document),
      m_repository(repository),
      m_service(std::make_shared<
                slicer_core::TransformedModelPreflightService>()),
      m_callbackState(std::make_shared<CallbackState>())
{
    if (m_document == nullptr || m_repository == nullptr)
    {
        throw std::invalid_argument(
            "TransformedModelPreflightLoader requires document "
            "and repository");
    }
    m_callbackState->loader = this;
}

TransformedModelPreflightLoader::~TransformedModelPreflightLoader()
{
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    QMutexLocker lock(&m_callbackState->mutex);
    m_callbackState->loader = nullptr;
}

bool TransformedModelPreflightLoader::RequestCurrent()
{
    if (!m_document->Instance().has_value()
        || m_document->SourceCacheKey().isEmpty())
    {
        return false;
    }
    const std::optional<SceneModelRepositoryEntry> sourceEntry =
        m_repository->Find(m_document->SourceCacheKey());
    if (!sourceEntry.has_value() || sourceEntry->model == nullptr)
    {
        return false;
    }

    ++m_generation;
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    m_activeCancellation = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancellation =
        m_activeCancellation;
    const std::shared_ptr<CallbackState> callbackState =
        m_callbackState;
    const std::shared_ptr<
        slicer_core::TransformedModelPreflightService>
        service = m_service;
    const quint64 generation = m_generation;
    const quint64 sceneRevision = m_document->SceneRevision();
    const quint64 transformRevision =
        m_document->Instance()->transformrevision;
    if (!m_document->SetTransformedPreflightRunning(
            generation,
            sceneRevision,
            transformRevision))
    {
        return false;
    }
    m_running = true;
    emit SigPreflightStarted();

    slicer_core::TransformedModelPreflightRequest request;
    request.source = sourceEntry->model.get();
    request.instance = m_document->Instance().value();
    request.sourcehash = sourceEntry->sourcehash.toStdString();
    request.resourcehash =
        sourceEntry->resourcehash.toStdString();
    request.sceneid = m_document->SceneId().toStdString();
    request.scenerevision = sceneRevision;
    request.expectedscenerevision = sceneRevision;
    request.expectedtransformrevision = transformRevision;
    request.generation = generation;
    request.admissioncontext.global_backend_available = true;
    request.cancellationrequested = [cancellation]()
    {
        return cancellation->load();
    };

    auto* runnable = new FunctionRunnable(
        [callbackState,
         sourceEntry,
         service,
         generation,
         request = std::move(request)]() mutable
        {
            Q_UNUSED(sourceEntry);
            slicer_core::TransformedModelPreflightExecution execution =
                service->Run(request);
            QMutexLocker lock(&callbackState->mutex);
            TransformedModelPreflightLoader* loader =
                callbackState->loader.data();
            if (loader == nullptr)
            {
                return;
            }
            QMetaObject::invokeMethod(
                loader,
                [loader,
                 generation,
                 execution = std::move(execution)]() mutable
                {
                    loader->OnWorkerCompleted(
                        generation,
                        std::move(execution));
                },
                Qt::QueuedConnection);
        });
    QThreadPool::globalInstance()->start(runnable);
    return true;
}

void TransformedModelPreflightLoader::Cancel()
{
    ++m_generation;
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    m_running = false;
    m_document->SetTransformedPreflightCancelled(
        m_generation - 1U);
    emit SigPreflightFinished();
}

bool TransformedModelPreflightLoader::IsRunning() const
{
    return m_running;
}

void TransformedModelPreflightLoader::OnWorkerCompleted(
    const quint64 generation,
    slicer_core::TransformedModelPreflightExecution execution)
{
    if (generation != m_generation)
    {
        return;
    }
    m_running = false;
    if (execution.cancelled)
    {
        m_document->SetTransformedPreflightCancelled(generation);
    }
    else if (execution.stale)
    {
        m_document->SetTransformedPreflightFailure(
            generation,
            QStringLiteral("MODEL_PREFLIGHT_REVISION_STALE"));
    }
    else if (!m_document->SetTransformedPreflightResult(
                 generation,
                 std::move(execution)))
    {
        m_document->SetTransformedPreflightFailure(
            generation,
            QStringLiteral("MODEL_PREFLIGHT_RESULT_STALE"));
    }
    emit SigPreflightFinished();
}
