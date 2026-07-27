#pragma once

#include "slicer_core/scene/SceneViewGeometry.h"

#include <QObject>
#include <QString>

#include <optional>

enum class SceneDocumentState
{
    Unloaded,
    Loading,
    Ready,
    Blocked,
    Failed,
    Cancelled,
};

class SceneDocument final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create the UI scene document.
     * @param parent QObject owner.
     */
    explicit SceneDocument(QObject* parent = nullptr);

    /**
     * @brief Reset the document to its unloaded state.
     */
    void Reset();

    /**
     * @brief Begin one asynchronous model-view generation.
     * @param generation Monotonic request generation.
     * @param modelPath Model path shown by the UI.
     */
    void SetLoading(quint64 generation, const QString& modelPath);

    /**
     * @brief Publish geometry for the current generation.
     * @param generation Generation that produced the geometry.
     * @param geometry Immutable projected scene geometry.
     * @return True when the generation was current and the result was accepted.
     */
    bool SetGeometry(
        quint64 generation,
        slicer_core::SceneViewGeometry geometry);

    /**
     * @brief Publish a loading failure for the current generation.
     * @param generation Generation that failed.
     * @param error User-facing failure detail.
     * @return True when the generation was current and the result was accepted.
     */
    bool SetFailure(quint64 generation, const QString& error);

    /**
     * @brief Mark the current generation cancelled.
     * @param generation Current generation.
     * @return True when the generation was current.
     */
    bool SetCancelled(quint64 generation);

    /**
     * @brief Return the current lifecycle state.
     * @return Scene document state.
     */
    SceneDocumentState State() const;

    /**
     * @brief Return the active request generation.
     * @return Monotonic generation.
     */
    quint64 Generation() const;

    /**
     * @brief Return the selected model path.
     * @return Model path shown by the UI.
     */
    QString ModelPath() const;

    /**
     * @brief Return the latest failure detail.
     * @return Empty text when no failure is active.
     */
    QString Error() const;

    /**
     * @brief Return the current projected geometry.
     * @return Geometry when a ready or blocked model is available.
     */
    const std::optional<slicer_core::SceneViewGeometry>& Geometry() const;

signals:
    void SigChanged();

private:
    void PublishState(SceneDocumentState state);

    SceneDocumentState m_state{SceneDocumentState::Unloaded};
    quint64 m_generation{0U};
    QString m_modelPath;
    QString m_error;
    std::optional<slicer_core::SceneViewGeometry> m_geometry;
};
