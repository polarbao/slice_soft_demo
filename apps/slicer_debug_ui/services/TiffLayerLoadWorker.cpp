#include "TiffLayerLoadWorker.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include <functional>
#include <utility>

namespace
{

class FunctionRunnable final : public QRunnable
{
public:
    explicit FunctionRunnable(std::function<void()> action)
        : m_action(std::move(action))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        m_action();
    }

private:
    std::function<void()> m_action;
};

QString ErrorCode(const slicer_core::TiffLayerError& error)
{
    return QString::fromStdString(
        slicer_core::TiffLayerErrorCodeString(error.Code()));
}

}  // namespace

struct TiffLayerLoadWorker::CallbackState
{
    QMutex mutex;
    QPointer<TiffLayerLoadWorker> worker;
};

TiffLayerLoadWorker::TiffLayerLoadWorker(
    std::shared_ptr<slicer_core::TiffLayerSource> source,
    QObject* parent)
    : QObject(parent),
      m_source(std::move(source)),
      m_callbackState(std::make_shared<CallbackState>())
{
    if (m_source == nullptr)
    {
        m_source =
            std::make_shared<slicer_core::TiffLayerSource>();
    }
    m_callbackState->worker = this;
    qRegisterMetaType<TiffLayerBufferPtr>("TiffLayerBufferPtr");
}

TiffLayerLoadWorker::~TiffLayerLoadWorker()
{
    Cancel();
    QMutexLocker lock(&m_callbackState->mutex);
    m_callbackState->worker.clear();
}

bool TiffLayerLoadWorker::IndexPackage(
    const QString& manifestPath)
{
    Cancel();
    try
    {
        static_cast<void>(m_source->IndexPackage(
            manifestPath.toStdWString()));
        return true;
    }
    catch (const slicer_core::TiffLayerError& error)
    {
        EmitFailure(
            m_generation,
            QStringLiteral("package-index"),
            -1,
            ErrorCode(error),
            QString::fromUtf8(error.what()));
    }
    catch (const std::exception& error)
    {
        EmitFailure(
            m_generation,
            QStringLiteral("package-index"),
            -1,
            QStringLiteral("TIFF_LAYER_MANIFEST_INVALID"),
            QString::fromUtf8(error.what()));
    }
    return false;
}

quint64 TiffLayerLoadWorker::RequestLayer(
    const int layerIndex,
    const QString& consumerId)
{
    ++m_generation;
    if (m_activeCancellation != nullptr)
    {
        m_activeCancellation->store(true);
    }
    m_activeCancellation =
        std::make_shared<std::atomic_bool>(false);

    const quint64 generation = m_generation;
    const auto layer = m_source->FindLayer(layerIndex);
    if (!layer.has_value())
    {
        EmitFailure(
            generation,
            consumerId,
            layerIndex,
            QStringLiteral("TIFF_LAYER_NOT_LISTED"),
            QStringLiteral("当前 package 的 manifest 未列出该层。"));
        return generation;
    }

    const std::shared_ptr<std::atomic_bool> cancellation =
        m_activeCancellation;
    const std::shared_ptr<CallbackState> callbackState =
        m_callbackState;
    const std::shared_ptr<slicer_core::TiffLayerSource> source =
        m_source;
    auto* runnable = new FunctionRunnable(
        [source,
         callbackState,
         cancellation,
         layer = *layer,
         generation,
         consumerId]()
        {
            try
            {
                slicer_core::TiffLayerLoadControl control;
                control.requestGeneration = generation;
                control.cancellationRequested =
                    [cancellation]()
                    {
                        return cancellation->load();
                    };
                control.generationCurrent =
                    [cancellation](const std::uint64_t)
                    {
                        return !cancellation->load();
                    };
                const slicer_core::TiffLayerLoadResult result =
                    source->LoadLayer(layer, control);

                QMutexLocker lock(&callbackState->mutex);
                TiffLayerLoadWorker* worker =
                    callbackState->worker.data();
                if (worker == nullptr || cancellation->load())
                {
                    return;
                }
                QMetaObject::invokeMethod(
                    worker,
                    [worker,
                     generation,
                     consumerId,
                     layerIndex = layer.layerIndex,
                     buffer = result.buffer,
                     cacheHit = result.cacheHit]()
                    {
                        if (worker->Generation() != generation)
                        {
                            return;
                        }
                        emit worker->SigLayerLoaded(
                            generation,
                            consumerId,
                            layerIndex,
                            buffer,
                            cacheHit);
                    },
                    Qt::QueuedConnection);
            }
            catch (const slicer_core::TiffLayerError& error)
            {
                if (error.Code()
                        == slicer_core::TiffLayerErrorCode::Cancelled
                    || error.Code()
                        == slicer_core::TiffLayerErrorCode::StaleResult)
                {
                    return;
                }
                QMutexLocker lock(&callbackState->mutex);
                TiffLayerLoadWorker* worker =
                    callbackState->worker.data();
                if (worker == nullptr || cancellation->load())
                {
                    return;
                }
                const QString code = ErrorCode(error);
                const QString message =
                    QString::fromUtf8(error.what());
                QMetaObject::invokeMethod(
                    worker,
                    [worker,
                     generation,
                     consumerId,
                     layerIndex = layer.layerIndex,
                     code,
                     message]()
                    {
                        if (worker->Generation() != generation)
                        {
                            return;
                        }
                        worker->EmitFailure(
                            generation,
                            consumerId,
                            layerIndex,
                            code,
                            message);
                    },
                    Qt::QueuedConnection);
            }
            catch (const std::exception& error)
            {
                QMutexLocker lock(&callbackState->mutex);
                TiffLayerLoadWorker* worker =
                    callbackState->worker.data();
                if (worker == nullptr || cancellation->load())
                {
                    return;
                }
                const QString message =
                    QString::fromUtf8(error.what());
                QMetaObject::invokeMethod(
                    worker,
                    [worker,
                     generation,
                     consumerId,
                     layerIndex = layer.layerIndex,
                     message]()
                    {
                        if (worker->Generation() != generation)
                        {
                            return;
                        }
                        worker->EmitFailure(
                            generation,
                            consumerId,
                            layerIndex,
                            QStringLiteral(
                                "TIFF_LAYER_READ_FAILED"),
                            message);
                    },
                    Qt::QueuedConnection);
            }
        });
    QThreadPool::globalInstance()->start(runnable);
    return generation;
}

void TiffLayerLoadWorker::Cancel()
{
    ++m_generation;
    if (m_activeCancellation != nullptr)
    {
        m_activeCancellation->store(true);
    }
}

quint64 TiffLayerLoadWorker::Generation() const
{
    return m_generation;
}

void TiffLayerLoadWorker::EmitFailure(
    const quint64 generation,
    const QString& consumerId,
    const int layerIndex,
    const QString& errorCode,
    const QString& message)
{
    emit SigLayerLoadFailed(
        generation,
        consumerId,
        layerIndex,
        errorCode,
        message);
}
