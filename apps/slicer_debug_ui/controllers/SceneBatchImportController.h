#pragma once

#include "../models/SceneDocument.h"
#include "../services/ModelTopViewLoader.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

enum class SceneBatchImportStartErrorCode
{
    None,
    Busy,
    EmptySelection,
    SceneNotReady,
    CapacityExceeded,
    UnsupportedFile,
    LoaderUnavailable,
};

enum class SceneBatchImportItemStatus
{
    Imported,
    Failed,
    Cancelled,
};

struct SceneBatchImportStartError
{
    SceneBatchImportStartErrorCode code{
        SceneBatchImportStartErrorCode::None};
    QString path;
    QString message;
};

struct SceneBatchImportStartResult
{
    bool started{false};
    std::optional<SceneBatchImportStartError> error;

    /**
     * @brief Report whether the batch was accepted.
     * @return True when asynchronous item loading started.
     */
    bool IsValid() const;
};

struct SceneBatchImportRequest
{
    QString batchid;
    QString configpath;
    QStringList files;
    bool autolayout{true};
};

struct SceneBatchImportItemResult
{
    QString path;
    QString modelid;
    QString instanceid;
    SceneBatchImportItemStatus status{
        SceneBatchImportItemStatus::Failed};
    QString errorcode;
    QString message;
};

struct SceneBatchImportSummary
{
    QString batchid;
    int selected{0};
    int imported{0};
    int failed{0};
    int cancelled{0};
    bool autolayoutrequested{false};
    bool autolayoutapplied{false};
    QString layouterrorcode;
    QString layouterror;
    quint64 finalscenerevision{0U};
    std::vector<SceneBatchImportItemResult> items;
};

/**
 * @brief Return the stable name for a batch-import start error.
 * @param code Start error code.
 * @return Stable ASCII error name.
 */
std::string_view SceneBatchImportStartErrorCodeName(
    SceneBatchImportStartErrorCode code);

/**
 * @brief Serialize model imports through the existing single-request loader.
 */
class SceneBatchImportController final : public QObject
{
    Q_OBJECT

public:
    using LoadRequestHandler =
        std::function<quint64(const ModelTopViewLoadRequest&)>;
    using CancelHandler = std::function<void()>;

    /**
     * @brief Create a batch-import orchestrator.
     * @param document Scene document receiving imported instances.
     * @param parent QObject owner.
     */
    explicit SceneBatchImportController(
        SceneDocument* document,
        QObject* parent = nullptr);

    /**
     * @brief Bind the existing asynchronous single-model loader.
     * @param loadHandler Starts one load and returns its generation.
     * @param cancelHandler Cancels the active logical load.
     */
    void SetLoadHandlers(
        LoadRequestHandler loadHandler,
        CancelHandler cancelHandler);

    /**
     * @brief Start one ordered multi-file import batch.
     * @param request Files, config context, identity, and layout policy.
     * @return Structured start result.
     */
    SceneBatchImportStartResult Start(
        const SceneBatchImportRequest& request);

    /**
     * @brief Cancel the current and queued import items.
     */
    void Cancel();

    /**
     * @brief Accept completion of the active loader generation.
     * @param loaderGeneration Generation reported by ModelTopViewLoader.
     */
    void OnLoadFinished(quint64 loaderGeneration);

    /**
     * @brief Report whether a batch is active.
     * @return True between accepted start and terminal summary.
     */
    bool IsRunning() const;

    /**
     * @brief Return the latest batch summary.
     * @return Immutable current or terminal summary.
     */
    const SceneBatchImportSummary& Summary() const;

    /**
     * @brief Return one localized progress line.
     * @return Current batch status.
     */
    QString StatusText() const;

signals:
    /**
     * @brief Notify observers that progress or summary changed.
     */
    void SigStateChanged();

    /**
     * @brief Notify observers that the active batch reached a terminal state.
     */
    void SigFinished();

private:
    static constexpr std::size_t kMaximumInstanceCount = 22U;

    SceneBatchImportStartResult ValidateAndNormalize(
        const SceneBatchImportRequest& request,
        SceneBatchImportRequest* normalized) const;
    void StartNext();
    void RecordCurrent(
        SceneBatchImportItemStatus status,
        const QString& errorCode,
        const QString& message);
    void Finish();
    bool IdentityExists(
        const QString& modelId,
        const QString& instanceId) const;
    int NextIdentityNumber(const QString& baseName);

    SceneDocument* m_document{nullptr};
    LoadRequestHandler m_loadHandler;
    CancelHandler m_cancelHandler;
    SceneBatchImportRequest m_request;
    SceneBatchImportSummary m_summary;
    std::size_t m_nextFileIndex{0U};
    std::size_t m_currentCountBefore{0U};
    quint64 m_expectedLoaderGeneration{0U};
    QString m_currentPath;
    QString m_currentModelId;
    QString m_currentInstanceId;
    bool m_running{false};
    bool m_cancelRequested{false};
};
