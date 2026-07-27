#include "ModelTopViewLoader.h"

#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/scene/ModelInstance.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include <filesystem>
#include <functional>
#include <optional>
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

std::string Utf8(const QString& value)
{
    return value.toUtf8().toStdString();
}

}  // namespace

struct ModelTopViewLoader::CallbackState
{
    QMutex mutex;
    QPointer<ModelTopViewLoader> loader;
};

struct ModelTopViewLoader::WorkerResult
{
    std::optional<slicer_core::SceneViewGeometry> geometry;
    QString error;
    bool cancelled{false};
};

ModelTopViewLoader::ModelTopViewLoader(
    SceneDocument* document,
    QObject* parent)
    : QObject(parent),
      m_document(document),
      m_callbackState(std::make_shared<CallbackState>())
{
    if (m_document == nullptr)
    {
        throw std::invalid_argument(
            "ModelTopViewLoader requires a SceneDocument");
    }
    m_callbackState->loader = this;
}

ModelTopViewLoader::~ModelTopViewLoader()
{
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    QMutexLocker lock(&m_callbackState->mutex);
    m_callbackState->loader = nullptr;
}

void ModelTopViewLoader::RequestLoad(
    const ModelTopViewLoadRequest& request)
{
    ++m_generation;
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    m_activeCancellation = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancellation =
        m_activeCancellation;
    const std::shared_ptr<CallbackState> callbackState = m_callbackState;
    const quint64 generation = m_generation;
    m_running = true;
    m_document->SetLoading(generation, request.modelpath);
    emit SigLoadingStarted();

    auto* runnable = new FunctionRunnable(
        [callbackState, cancellation, generation, request]()
        {
            WorkerResult result;
            try
            {
                if (cancellation->load())
                {
                    result.cancelled = true;
                }
                else
                {
                    slicer_core::SliceConfig config;
                    std::filesystem::path configDirectory =
                        std::filesystem::current_path();
                    const QFileInfo configInfo(request.configpath);
                    if (!request.configpath.trimmed().isEmpty()
                        && configInfo.exists()
                        && configInfo.isFile())
                    {
                        const std::filesystem::path configPath(
                            configInfo.absoluteFilePath().toStdWString());
                        config =
                            slicer_core::load_slice_config(configPath);
                        configDirectory = configPath.parent_path();
                    }

                    const QFileInfo modelInfo(request.modelpath);
                    if (!modelInfo.exists() || !modelInfo.isFile())
                    {
                        throw std::runtime_error(
                            "selected model file does not exist");
                    }
                    config.input.model_path =
                        modelInfo.absoluteFilePath().toStdWString();
                    config.input.format = "auto";

                    const slicer_core::SceneModel source =
                        slicer_core::load_model_report(
                            config,
                            configDirectory);
                    if (cancellation->load())
                    {
                        result.cancelled = true;
                    }
                    else
                    {
                        slicer_core::ModelInstance instance;
                        instance.instanceid = Utf8(request.instanceid);
                        instance.modelid = Utf8(request.modelid);
                        instance.sourcetransformidentity =
                            Utf8(modelInfo.absoluteFilePath());
                        instance.transformrevision =
                            request.transformrevision;
                        instance.sourcebboxmm = source.bbox_mm;
                        instance.effectivebboxmm = source.bbox_mm;

                        slicer_core::SceneViewGeometryRequest coreRequest;
                        coreRequest.sceneid = Utf8(request.sceneid);
                        coreRequest.scenerevision =
                            request.scenerevision;
                        coreRequest.expectedscenerevision =
                            request.scenerevision;
                        coreRequest.expectedtransformrevision =
                            request.transformrevision;
                        coreRequest.instance = std::move(instance);
                        coreRequest.admissionstatus =
                            request.admissionstatus;
                        slicer_core::SceneViewGeometryResult coreResult =
                            slicer_core::BuildSceneViewGeometry(
                                source,
                                coreRequest);
                        if (!coreResult.IsValid())
                        {
                            result.error = QStringLiteral("%1: %2")
                                               .arg(QString::fromLatin1(
                                                   slicer_core::
                                                       SceneViewGeometryErrorCodeName(
                                                           coreResult.error
                                                               ->code)
                                                           .data()))
                                               .arg(QString::fromUtf8(
                                                   coreResult.error->message
                                                       .c_str()));
                        }
                        else
                        {
                            result.geometry =
                                std::move(coreResult.geometry);
                        }
                    }
                }
            }
            catch (const std::exception& error)
            {
                result.error = QString::fromUtf8(error.what());
            }

            QMutexLocker lock(&callbackState->mutex);
            ModelTopViewLoader* loader =
                callbackState->loader.data();
            if (loader == nullptr)
            {
                return;
            }
            QMetaObject::invokeMethod(
                loader,
                [loader,
                 generation,
                 result = std::move(result)]() mutable
                {
                    loader->OnWorkerCompleted(
                        generation,
                        std::move(result));
                },
                Qt::QueuedConnection);
        });
    QThreadPool::globalInstance()->start(runnable);
}

void ModelTopViewLoader::Cancel()
{
    ++m_generation;
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    m_running = false;
    m_document->SetLoading(m_generation, m_document->ModelPath());
    m_document->SetCancelled(m_generation);
    emit SigLoadingFinished();
}

bool ModelTopViewLoader::IsRunning() const
{
    return m_running;
}

quint64 ModelTopViewLoader::Generation() const
{
    return m_generation;
}

void ModelTopViewLoader::OnWorkerCompleted(
    const quint64 generation,
    WorkerResult result)
{
    if (generation != m_generation)
    {
        return;
    }

    m_running = false;
    if (result.cancelled)
    {
        m_document->SetCancelled(generation);
    }
    else if (!result.geometry.has_value())
    {
        m_document->SetFailure(generation, result.error);
    }
    else
    {
        m_document->SetGeometry(
            generation,
            std::move(result.geometry.value()));
    }
    emit SigLoadingFinished();
}
