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
 * @brief 用于冻结 SliceSoft 公共 C SPI 的运行时加载客户端。
 *
 * 客户端有意不依赖任何切片实现类型。11 个 pm_* 符号全部在运行时解析，
 * 使参考宿主无需链接模块导入库即可加载；能力 DLL 缺失时也能明确报告错误。
 */
class ModuleClient final
{
public:
    /** @brief 构造尚未加载模块的客户端。 */
    ModuleClient();

    /** @brief 释放模块实例并卸载 DLL。 */
    ~ModuleClient();

    ModuleClient(const ModuleClient&) = delete;
    ModuleClient& operator=(const ModuleClient&) = delete;

    /**
     * @brief 加载公共模块并创建一个独立的模块实例。
     * @param modulePath slicer_module.dll 的绝对或相对路径。
     * @param optionsJson UTF-8 模块选项，通常是一个空的 JSON 对象。
     * @param error 接收用户可读的失败原因。
     * @return SPI 版本、能力和实例创建均通过时返回 true。
     */
    bool Open(
        const QString& modulePath,
        const QByteArray& optionsJson,
        QString* error);

    /**
     * @brief 销毁模块实例并卸载运行时 DLL。
     * @return 该函数不返回值。
     */
    void Close();

    /**
     * @brief 报告模块实例是否已准备好接收请求。
     * @return 当 DLL 和模块实例都有效时为 true。
     */
    bool IsOpen() const;

    /**
     * @brief 返回加载时捕获的不可变 pm_module_info 响应。
     * @return UTF-8 模块信息 JSON；未加载时返回空值。
     */
    QByteArray ModuleInfo() const;

    /**
     * @brief 运行公共无副作用模块运行状况检查。
     * @param report 接收 UTF-8 健康报告 JSON。
     * @param error 接收用户可读的失败原因。
     * @return pm_self_test 成功时返回 true。
     */
    bool SelfTest(QByteArray* report, QString* error);

    /**
     * @brief 执行一项能力请求，直至获取最终字节结果。
     * @param requestJson 符合冻结能力 DTO 的 UTF-8 请求。
     * @param result 接收 JSON 文本或二进制 blob 块。
     * @param error 接收用户可读的失败原因。
     * @return 当提交、轮询和结果检索均成功时为 true。
     */
    bool Execute(
        const QByteArray& requestJson,
        QByteArray* result,
        QString* error);

    /**
     * @brief 通过 pm_submit 提交能力请求。
     * @param requestJson 符合冻结能力 DTO 的 UTF-8 请求。
     * @param error 请求被拒绝时，接收线程局部的公共错误信息。
     * @return 不透明的公共作业句柄，或失败时为 nullptr。
     */
    pm_job_t* Submit(const QByteArray& requestJson, QString* error);

    /**
     * @brief 读取作业的最新进度快照。
     * @param job 提交返回的不透明公共作业句柄。
     * @param progress 接收 UTF-8 进度 JSON。
     * @param error 接收用户可读的失败原因。
     * @return 成功读取进度快照时返回 true。
     */
    bool Poll(pm_job_t* job, QByteArray* progress, QString* error);

    /**
     * @brief 读取作业的最终结果。
     * @param job 提交返回的不透明公共作业句柄。
     * @param result 接收 UTF-8 结果 JSON。
     * @param error 接收用户可读的失败原因。
     * @return 成功读取最终结果时返回 true。
     */
    bool Result(pm_job_t* job, QByteArray* result, QString* error);

    /**
     * @brief 请求以协作方式取消公共作业。
     * @param job 提交返回的不透明公共作业句柄。
     * @param error 接收用户可读的失败原因。
     * @return 幂等取消请求被接受时返回 true。
     */
    bool Cancel(pm_job_t* job, QString* error);

    /**
     * @brief 通过其所属的 DLL 释放公共作业句柄。
     * @param job 不透明的公共作业句柄；允许为 nullptr。
     * @return 该函数不返回值。
     */
    void Release(pm_job_t* job);

    /**
     * @brief 返回此客户端进行的公共 ABI 调用的数量。
     * @return 供后续 UI 零调用断言使用的单调递增调用计数。
     */
    quint64 CallCount() const;

    /**
     * @brief 将公共 ABI 调用计数器重置为零。
     * @return 该函数不返回值。
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
