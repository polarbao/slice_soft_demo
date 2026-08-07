#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define PM_MODULE_STATIC
#include "contracts/print_module_spi.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <atomic>

/**
 * @brief Runtime-loaded client for the frozen SliceSoft public C SPI.
 *
 * The client deliberately owns no slicer implementation types. It resolves
 * all eleven pm_* exports at runtime so the reference host remains loadable
 * when the capability DLL is absent.
 */
class ModuleClient final
{
public:
    /** @brief Constructs an unloaded client. */
    ModuleClient();

    /** @brief Releases the module instance and unloads the DLL. */
    ~ModuleClient();

    ModuleClient(const ModuleClient&) = delete;
    ModuleClient& operator=(const ModuleClient&) = delete;

    /**
     * @brief Loads the public module and creates one isolated module instance.
     * @param modulePath Absolute or relative path to slicer_module.dll.
     * @param optionsJson UTF-8 module options, normally an empty JSON object.
     * @param error Receives a user-readable failure reason.
     * @return True when SPI version, capabilities and instance creation pass.
     */
    bool Open(
        const QString& modulePath,
        const QByteArray& optionsJson,
        QString* error);

    /**
     * @brief Destroys the module instance and unloads the runtime DLL.
     * @return This function does not return a value.
     */
    void Close();

    /**
     * @brief Reports whether a module instance is ready for submissions.
     * @return True when both DLL and module instance are valid.
     */
    bool IsOpen() const;

    /**
     * @brief Returns the immutable pm_module_info response captured at load.
     * @return UTF-8 module information JSON, or an empty value when unloaded.
     */
    QByteArray ModuleInfo() const;

    /**
     * @brief Runs the public no-side-effect module health check.
     * @param report Receives the UTF-8 health report JSON.
     * @param error Receives a user-readable failure reason.
     * @return True when pm_self_test succeeds.
     */
    bool SelfTest(QByteArray* report, QString* error);

    /**
     * @brief Executes one capability request to its terminal byte result.
     * @param requestJson UTF-8 request matching a frozen capability DTO.
     * @param result Receives JSON text or a binary blob chunk.
     * @param error Receives a user-readable failure reason.
     * @return True when submit, poll and result retrieval all succeed.
     */
    bool Execute(
        const QByteArray& requestJson,
        QByteArray* result,
        QString* error);

    /**
     * @brief Submits a capability request through pm_submit.
     * @param requestJson UTF-8 request matching a frozen capability DTO.
     * @param error Receives the thread-local public error on rejection.
     * @return Opaque public job handle, or nullptr on failure.
     */
    pm_job_t* Submit(const QByteArray& requestJson, QString* error);

    /**
     * @brief Reads the latest progress snapshot for a job.
     * @param job Opaque public job handle returned by Submit.
     * @param progress Receives UTF-8 progress JSON.
     * @param error Receives a user-readable failure reason.
     * @return True when the progress snapshot was read.
     */
    bool Poll(pm_job_t* job, QByteArray* progress, QString* error);

    /**
     * @brief Reads a terminal job result.
     * @param job Opaque public job handle returned by Submit.
     * @param result Receives UTF-8 result JSON.
     * @param error Receives a user-readable failure reason.
     * @return True when the terminal result was read.
     */
    bool Result(pm_job_t* job, QByteArray* result, QString* error);

    /**
     * @brief Requests cooperative cancellation of a public job.
     * @param job Opaque public job handle returned by Submit.
     * @param error Receives a user-readable failure reason.
     * @return True when the idempotent cancellation request was accepted.
     */
    bool Cancel(pm_job_t* job, QString* error);

    /**
     * @brief Releases a public job handle through its owning DLL.
     * @param job Opaque public job handle; nullptr is accepted.
     * @return This function does not return a value.
     */
    void Release(pm_job_t* job);

    /**
     * @brief Returns the number of public ABI calls made by this client.
     * @return Monotonic call count used by later UI zero-call assertions.
     */
    quint64 CallCount() const;

    /**
     * @brief Resets the public ABI call counter to zero.
     * @return This function does not return a value.
     */
    void ResetCallCount();

private:
    using SpiVersionFunction = int (PM_CALL*)();
    using ModuleInfoFunction = int (PM_CALL*)(char*, int, int*);
    using CreateFunction = pm_module_t* (PM_CALL*)(const char*);
    using DestroyFunction = void (PM_CALL*)(pm_module_t*);
    using SubmitFunction = pm_job_t* (PM_CALL*)(pm_module_t*, const char*);
    using PollFunction = int (PM_CALL*)(pm_job_t*, char*, int, int*);
    using CancelFunction = int (PM_CALL*)(pm_job_t*);
    using ResultFunction = int (PM_CALL*)(pm_job_t*, char*, int, int*);
    using ReleaseFunction = void (PM_CALL*)(pm_job_t*);
    using SelfTestFunction = int (PM_CALL*)(pm_module_t*, char*, int, int*);
    using LastErrorFunction = int (PM_CALL*)(char*, int, int*);

    bool ResolveExports(QString* error);
    bool ReadModuleInfo(QByteArray* output, QString* error);
    QString LastErrorText(const QString& fallback);
    void ResetFunctions();
    void RecordCall();

    HMODULE m_library{nullptr};
    pm_module_t* m_module{nullptr};
    QByteArray m_moduleInfo;
    std::atomic<quint64> m_callCount{0};
    SpiVersionFunction m_spiVersion{nullptr};
    ModuleInfoFunction m_moduleInfoFunction{nullptr};
    CreateFunction m_create{nullptr};
    DestroyFunction m_destroy{nullptr};
    SubmitFunction m_submit{nullptr};
    PollFunction m_poll{nullptr};
    CancelFunction m_cancel{nullptr};
    ResultFunction m_result{nullptr};
    ReleaseFunction m_release{nullptr};
    SelfTestFunction m_selfTest{nullptr};
    LastErrorFunction m_lastError{nullptr};
};
