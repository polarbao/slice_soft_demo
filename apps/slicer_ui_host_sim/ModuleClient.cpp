#include "ModuleClient.h"

#include <array>
#include <functional>

namespace
{
constexpr std::array<const char*, 15> RequiredCapabilities{
    "model.import",
    "model.get_metadata",
    "model.release",
    "scene.apply_operation",
    "scene.get_snapshot",
    "scene.get_viewdata",
    "geometry.collision",
    "geometry.preflight",
    "geometry.repair",
    "slice.rgbwsv",
    "package.verify",
    "package.get_summary",
    "package.get_layer_descriptor",
    "package.render_layer_preview",
    "package.read_report"};

bool ReadTextBuffer(
    const std::function<int(char*, int, int*)>& reader,
    QByteArray* output,
    QString* error)
{
    if (output == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("输出缓冲不能为空。");
        }
        return false;
    }

    int required = 0;
    const int probeCode = reader(nullptr, 0, &required);
    if (probeCode != PM_ERR_BUFFER_SMALL || required < 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块未遵守缓冲区三态协议。");
        }
        return false;
    }

    QByteArray buffer(required + 1, '\0');
    int written = 0;
    const int readCode = reader(buffer.data(), buffer.size(), &written);
    if (readCode < 0 || written != required)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块返回文本读取失败。");
        }
        return false;
    }

    buffer.resize(written);
    *output = buffer;
    return true;
}

QString WindowsErrorText(DWORD errorCode)
{
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&message),
        0,
        nullptr);
    if (length == 0 || message == nullptr)
    {
        return QStringLiteral("Windows 错误 %1").arg(errorCode);
    }

    const QString result = QString::fromWCharArray(message).trimmed();
    LocalFree(message);
    return result;
}
}

ModuleClient::ModuleClient() = default;

ModuleClient::~ModuleClient()
{
    Close();
}

bool ModuleClient::Open(
    const QString& modulePath,
    const QByteArray& optionsJson,
    QString* error)
{
    Close();
    ResetCallCount();

    m_library = LoadLibraryW(
        reinterpret_cast<const wchar_t*>(modulePath.utf16()));
    if (m_library == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法加载切片能力模块：%1")
                         .arg(WindowsErrorText(GetLastError()));
        }
        return false;
    }

    if (!ResolveExports(error))
    {
        Close();
        return false;
    }

    RecordCall();
    if (m_spiVersion() != PM_SPI_VERSION)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("SPI 版本不兼容，宿主要求 v%1。")
                         .arg(PM_SPI_VERSION);
        }
        Close();
        return false;
    }

    QByteArray moduleInfo;
    if (!ReadModuleInfo(&moduleInfo, error))
    {
        Close();
        return false;
    }
    for (const char* capability : RequiredCapabilities)
    {
        if (!moduleInfo.contains(capability))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("模块缺少冻结能力：%1")
                             .arg(QString::fromUtf8(capability));
            }
            Close();
            return false;
        }
    }

    RecordCall();
    m_module = m_create(optionsJson.constData());
    if (m_module == nullptr)
    {
        const QString detail = LastErrorText(
            QStringLiteral("pm_create 拒绝创建模块实例。"));
        if (error != nullptr)
        {
            *error = detail;
        }
        Close();
        return false;
    }

    m_moduleInfo = moduleInfo;
    return true;
}

void ModuleClient::Close()
{
    if (m_module != nullptr && m_destroy != nullptr)
    {
        RecordCall();
        m_destroy(m_module);
    }
    m_module = nullptr;
    m_moduleInfo.clear();

    if (m_library != nullptr)
    {
        FreeLibrary(m_library);
    }
    m_library = nullptr;
    ResetFunctions();
}

bool ModuleClient::IsOpen() const
{
    return m_library != nullptr && m_module != nullptr;
}

QByteArray ModuleClient::ModuleInfo() const
{
    return m_moduleInfo;
}

bool ModuleClient::SelfTest(QByteArray* report, QString* error)
{
    if (!IsOpen())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块尚未加载。");
        }
        return false;
    }

    const bool success = ReadTextBuffer(
        [this](char* output, int capacity, int* required)
        {
            RecordCall();
            return m_selfTest(m_module, output, capacity, required);
        },
        report,
        error);
    if (!success && error != nullptr)
    {
        *error = LastErrorText(*error);
    }
    return success;
}

pm_job_t* ModuleClient::Submit(
    const QByteArray& requestJson,
    QString* error)
{
    if (!IsOpen())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块尚未加载。");
        }
        return nullptr;
    }

    RecordCall();
    pm_job_t* job = m_submit(m_module, requestJson.constData());
    if (job == nullptr && error != nullptr)
    {
        *error = LastErrorText(QStringLiteral("模块拒绝能力请求。"));
    }
    return job;
}

bool ModuleClient::Poll(
    pm_job_t* job,
    QByteArray* progress,
    QString* error)
{
    if (job == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("作业句柄不能为空。");
        }
        return false;
    }

    return ReadTextBuffer(
        [this, job](char* output, int capacity, int* required)
        {
            RecordCall();
            return m_poll(job, output, capacity, required);
        },
        progress,
        error);
}

bool ModuleClient::Result(
    pm_job_t* job,
    QByteArray* result,
    QString* error)
{
    if (job == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("作业句柄不能为空。");
        }
        return false;
    }

    const bool success = ReadTextBuffer(
        [this, job](char* output, int capacity, int* required)
        {
            RecordCall();
            return m_result(job, output, capacity, required);
        },
        result,
        error);
    if (!success && error != nullptr)
    {
        *error = LastErrorText(*error);
    }
    return success;
}

bool ModuleClient::Cancel(pm_job_t* job, QString* error)
{
    if (job == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("作业句柄不能为空。");
        }
        return false;
    }

    RecordCall();
    if (m_cancel(job) != PM_OK)
    {
        if (error != nullptr)
        {
            *error = LastErrorText(QStringLiteral("取消请求被模块拒绝。"));
        }
        return false;
    }
    return true;
}

void ModuleClient::Release(pm_job_t* job)
{
    if (m_release != nullptr)
    {
        RecordCall();
        m_release(job);
    }
}

quint64 ModuleClient::CallCount() const
{
    return m_callCount.load(std::memory_order_relaxed);
}

void ModuleClient::ResetCallCount()
{
    m_callCount.store(0, std::memory_order_relaxed);
}

bool ModuleClient::ResolveExports(QString* error)
{
#define RESOLVE_EXPORT(member, name, type)                                    \
    member = reinterpret_cast<type>(GetProcAddress(m_library, name));         \
    if (member == nullptr)                                                     \
    {                                                                          \
        if (error != nullptr)                                                  \
        {                                                                      \
            *error = QStringLiteral("模块缺少冻结导出：%1")                  \
                         .arg(QString::fromLatin1(name));                       \
        }                                                                      \
        return false;                                                          \
    }

    RESOLVE_EXPORT(m_spiVersion, "pm_spi_version", SpiVersionFunction)
    RESOLVE_EXPORT(m_moduleInfoFunction, "pm_module_info", ModuleInfoFunction)
    RESOLVE_EXPORT(m_create, "pm_create", CreateFunction)
    RESOLVE_EXPORT(m_destroy, "pm_destroy", DestroyFunction)
    RESOLVE_EXPORT(m_submit, "pm_submit", SubmitFunction)
    RESOLVE_EXPORT(m_poll, "pm_poll", PollFunction)
    RESOLVE_EXPORT(m_cancel, "pm_cancel", CancelFunction)
    RESOLVE_EXPORT(m_result, "pm_result", ResultFunction)
    RESOLVE_EXPORT(m_release, "pm_release", ReleaseFunction)
    RESOLVE_EXPORT(m_selfTest, "pm_self_test", SelfTestFunction)
    RESOLVE_EXPORT(m_lastError, "pm_last_error", LastErrorFunction)

#undef RESOLVE_EXPORT
    return true;
}

bool ModuleClient::ReadModuleInfo(QByteArray* output, QString* error)
{
    return ReadTextBuffer(
        [this](char* destination, int capacity, int* required)
        {
            RecordCall();
            return m_moduleInfoFunction(destination, capacity, required);
        },
        output,
        error);
}

QString ModuleClient::LastErrorText(const QString& fallback)
{
    if (m_lastError == nullptr)
    {
        return fallback;
    }

    QByteArray detail;
    QString readError;
    if (!ReadTextBuffer(
            [this](char* output, int capacity, int* required)
            {
                RecordCall();
                return m_lastError(output, capacity, required);
            },
            &detail,
            &readError))
    {
        return fallback;
    }
    return QString::fromUtf8(detail);
}

void ModuleClient::ResetFunctions()
{
    m_spiVersion = nullptr;
    m_moduleInfoFunction = nullptr;
    m_create = nullptr;
    m_destroy = nullptr;
    m_submit = nullptr;
    m_poll = nullptr;
    m_cancel = nullptr;
    m_result = nullptr;
    m_release = nullptr;
    m_selfTest = nullptr;
    m_lastError = nullptr;
}

void ModuleClient::RecordCall()
{
    m_callCount.fetch_add(1, std::memory_order_relaxed);
}
