#pragma once

#include "../models/SceneDocument.h"
#include "SceneModelRepository.h"

#include "slicer_core/preflight/TransformedModelPreflight.h"

#include <QObject>

#include <atomic>
#include <memory>

/**
 * @brief Asynchronously audit the current effective model instance.
 */
class TransformedModelPreflightLoader final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create the transformed preflight loader.
     * @param document UI scene state receiving latest results.
     * @param repository Immutable source model repository.
     * @param parent QObject owner.
     */
    explicit TransformedModelPreflightLoader(
        SceneDocument* document,
        SceneModelRepository* repository,
        QObject* parent = nullptr);
    ~TransformedModelPreflightLoader() override;

    /**
     * @brief Audit the document's current instance and revisions.
     * @return True when a complete request was started.
     */
    bool RequestCurrent();

    /**
     * @brief Cancel the current logical request.
     */
    void Cancel();

    /**
     * @brief Report whether a preflight worker is active.
     * @return True while the newest request is running.
     */
    bool IsRunning() const;

signals:
    void SigPreflightStarted();
    void SigPreflightFinished();

private:
    struct CallbackState;

    void OnWorkerCompleted(
        quint64 generation,
        slicer_core::TransformedModelPreflightExecution execution);

    SceneDocument* m_document{nullptr};
    SceneModelRepository* m_repository{nullptr};
    std::shared_ptr<
        slicer_core::TransformedModelPreflightService>
        m_service;
    std::shared_ptr<CallbackState> m_callbackState;
    std::shared_ptr<std::atomic_bool> m_activeCancellation;
    quint64 m_generation{0U};
    bool m_running{false};
};
