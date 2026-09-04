#include "HostRipJobController.h"

#include "rip_integration/RipArtifactPublisher.h"
#include "rip_integration/RipCommandBuilder.h"
#include "rip_integration/RipInputValidator.h"
#include "rip_integration/RipOutputValidator.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>

namespace
{
constexpr int kMaximumCapturedLogBytes = 1024 * 1024;

std::filesystem::path FsPath(const QString& path)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

QString QtPath(const std::filesystem::path& path)
{
#ifdef Q_OS_WIN
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

QString FileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
    {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool IsContainedPath(
    const QString& root,
    const QString& candidate,
    const bool allowRoot = false)
{
    QString normalizedRoot = QDir::cleanPath(
        QFileInfo(root).absoluteFilePath());
    QString normalizedCandidate = QDir::cleanPath(
        QFileInfo(candidate).absoluteFilePath());
#ifdef Q_OS_WIN
    normalizedRoot = normalizedRoot.toCaseFolded();
    normalizedCandidate = normalizedCandidate.toCaseFolded();
#endif
    const QString prefix = normalizedRoot.endsWith('/')
        ? normalizedRoot : normalizedRoot + '/';
    return (allowRoot && normalizedCandidate == normalizedRoot)
        || normalizedCandidate.startsWith(prefix);
}

bool ReadJsonObject(
    const QString& path,
    QJsonObject* object,
    QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法读取 JSON：%1").arg(path);
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    if (!document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("JSON 对象无效：%1 · %2")
                         .arg(path, parseError.errorString());
        }
        return false;
    }
    *object = document.object();
    return true;
}

void AppendCapped(QByteArray* destination, const QByteArray& value)
{
    if (destination->size() >= kMaximumCapturedLogBytes)
    {
        return;
    }
    destination->append(value.left(
        kMaximumCapturedLogBytes - destination->size()));
}

QString RipMessage(const slicesoft::rip::RipStatus& status)
{
    return QString::fromStdString(status.message);
}

QString RipCode(const slicesoft::rip::RipStatus& status)
{
    return QString::fromStdString(status.code);
}

struct InputValidationState
{
    slicesoft::rip::RipStatus status;
    QVector<hostripsourcefileidentity> sourceIdentity;
    QString identityError;
};

struct OutputValidationState
{
    slicesoft::rip::RipOutputValidationResult validation;
    bool sourceIdentityValid{false};
    QString identityError;
};
}

HostRipJobController::HostRipJobController(QObject* parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    m_killTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &HostRipJobController::OnTimeout);
    connect(&m_killTimer, &QTimer::timeout, this, [this]()
    {
        if (m_process != nullptr && m_process->state() != QProcess::NotRunning)
        {
            m_process->kill();
        }
    });
}

HostRipJobController::~HostRipJobController()
{
    m_timeoutTimer.stop();
    m_killTimer.stop();
    if (m_cancelToken != nullptr)
    {
        m_cancelToken->store(true, std::memory_order_relaxed);
    }
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        (void)m_process->waitForFinished(3000);
    }
    if (m_validationThread != nullptr)
    {
        QThread* thread = m_validationThread;
        m_validationThread = nullptr;
        thread->disconnect(this);
        (void)thread->wait();
        delete thread;
    }
    QString ignored;
    (void)CleanupOwnedStaging(&ignored);
}

QString HostRipJobController::DefaultModuleDirectory()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("modules/rip"));
}

bool HostRipJobController::CheckRuntime(
    const QString& moduleDirectory,
    QString* error) const
{
    RuntimeMetadata ignored;
    return InspectRuntime(moduleDirectory, &ignored, error);
}

bool HostRipJobController::CheckRequest(
    const QString& packageDirectory,
    const QString& moduleDirectory,
    const hostripsettings& settings,
    QString* error) const
{
    QString validationError;
    RuntimeMetadata runtime;
    PackageMetadata package;
    const bool valid = HostRipSettingsStore::Validate(
            settings, &validationError)
        && InspectRuntime(moduleDirectory, &runtime, &validationError)
        && InspectPackage(
            packageDirectory, &package, &validationError);
    if (!valid && error != nullptr)
    {
        *error = validationError;
    }
    return valid;
}

bool HostRipJobController::InspectRuntime(
    const QString& moduleDirectory,
    RuntimeMetadata* metadata,
    QString* error) const
{
    if (metadata == nullptr)
    {
        return false;
    }
    *metadata = {};
    const QString root = QDir::cleanPath(
        QFileInfo(moduleDirectory).absoluteFilePath());
    const QString manifestPath = QDir(root).filePath(
        QStringLiteral("rip_module.json"));
    if (!QFileInfo(root).isDir())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 模块目录不存在：%1").arg(root);
        }
        return false;
    }
    QJsonObject manifest;
    if (!ReadJsonObject(manifestPath, &manifest, error)
        || manifest.value(QStringLiteral("schema")).toString()
            != QStringLiteral("slicesoft.rip.module.1")
        || manifest.value(QStringLiteral("moduleId")).toString()
            != QStringLiteral("slicesoft.external_rip")
        || manifest.value(QStringLiteral("version")).toString()
            != QStringLiteral("1.1.0")
        || manifest.value(QStringLiteral("status")).toString()
            != QStringLiteral("LOCAL_ENGINEERING_ONLY")
        || manifest.value(QStringLiteral("externalValidation")).toString()
            != QStringLiteral("EXTERNAL_VALIDATION_DEFERRED"))
    {
        if (error != nullptr && error->isEmpty())
        {
            *error = QStringLiteral("RIP 模块 manifest 身份或安全状态无效。");
        }
        return false;
    }
    const QJsonArray files = manifest.value(QStringLiteral("files")).toArray();
    if (files.size() < 11)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 模块文件清单不完整。");
        }
        return false;
    }
    for (const QJsonValue& value : files)
    {
        const QJsonObject item = value.toObject();
        const QString relativePath = item.value(QStringLiteral("path")).toString();
        const QString absolutePath = QDir(root).filePath(relativePath);
        const QFileInfo info(absolutePath);
        const QString expectedHash = item.value(QStringLiteral("sha256")).toString();
        if (relativePath.isEmpty() || !IsContainedPath(root, absolutePath)
            || !info.isFile()
            || item.value(QStringLiteral("size")).toVariant().toLongLong()
                != info.size()
            || expectedHash.size() != 64
            || FileSha256(absolutePath) != expectedHash)
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("RIP 模块文件缺失或 hash 不一致：%1")
                             .arg(relativePath);
            }
            return false;
        }
    }
    const QString cliPath = QDir(root).filePath(
        manifest.value(QStringLiteral("entrypoint")).toString());
    const QString dllPath = QDir(root).filePath(
        manifest.value(QStringLiteral("library")).toString());
    const QString resourceDirectory = QDir(root).filePath(
        manifest.value(QStringLiteral("resourceDirectory")).toString());
    if (!IsContainedPath(root, cliPath)
        || !IsContainedPath(root, dllPath)
        || !IsContainedPath(root, resourceDirectory)
        || !QFileInfo(cliPath).isFile()
        || !QFileInfo(dllPath).isFile()
        || !QFileInfo(resourceDirectory).isDir())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 模块入口、DLL 或资源路径无效。");
        }
        return false;
    }
    metadata->moduleDirectory = root;
    metadata->manifestPath = manifestPath;
    metadata->manifestSha256 = FileSha256(manifestPath);
    metadata->version = manifest.value(QStringLiteral("version")).toString();
    metadata->cliPath = cliPath;
    metadata->dllPath = dllPath;
    metadata->resourceDirectory = resourceDirectory;
    return true;
}

bool HostRipJobController::InspectPackage(
    const QString& packageDirectory,
    PackageMetadata* metadata,
    QString* error) const
{
    if (metadata == nullptr)
    {
        return false;
    }
    *metadata = {};
    const QString package = QDir::cleanPath(
        QFileInfo(packageDirectory).absoluteFilePath());
    const QString manifestPath = QDir(package).filePath(
        QStringLiteral("manifest.json"));
    QJsonObject manifest;
    if (!QFileInfo(package).isDir()
        || !ReadJsonObject(manifestPath, &manifest, error))
    {
        return false;
    }
    const QJsonObject grid = manifest.value(QStringLiteral("grid")).toObject();
    const QJsonObject tiff = manifest.value(QStringLiteral("tiff")).toObject();
    const QJsonArray layers = manifest.value(QStringLiteral("layers")).toArray();
    const QJsonArray channelOrder = tiff.value(
        QStringLiteral("channelOrder")).toArray();
    const QStringList expectedChannels{
        QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"),
        QStringLiteral("W"), QStringLiteral("S"), QStringLiteral("V")};
    QStringList actualChannels;
    for (const QJsonValue& channel : channelOrder)
    {
        actualChannels.push_back(channel.toString());
    }
    const int width = grid.value(QStringLiteral("widthPx")).toInt();
    const int height = grid.value(QStringLiteral("heightPx")).toInt();
    const int layerCount = grid.value(QStringLiteral("layerCount")).toInt();
    if (manifest.value(QStringLiteral("schema")).toString()
            != QStringLiteral("p0.rgbwsv.2")
        || !manifest.value(
            QStringLiteral("productionOutputWritten")).toBool(false)
        || manifest.value(QStringLiteral("fallbackApplied")).toBool(true)
        || tiff.value(QStringLiteral("bitDepth")).toInt() != 8
        || tiff.value(QStringLiteral("channelCount")).toInt() != 6
        || actualChannels != expectedChannels
        || tiff.value(QStringLiteral("planarConfig")).toString()
            != QStringLiteral("contiguous")
        || tiff.value(QStringLiteral("polarity")).toString()
            != QStringLiteral("black_is_print")
        || tiff.value(QStringLiteral("storageMode")).toString()
            != QStringLiteral("stripped")
        || tiff.value(QStringLiteral("tiled")).toBool(true)
        || width <= 0 || height <= 0 || layerCount <= 0
        || layers.size() != layerCount)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "RIP 输入包必须是有效的 stripped、8bit、contiguous RGBWSV 包。");
        }
        return false;
    }
    const QString inputDirectory = QDir(package).filePath(
        QStringLiteral("layers"));
    if (!QFileInfo(inputDirectory).isDir())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 输入 layers 目录不存在。");
        }
        return false;
    }
    for (int index = 0; index < layers.size(); ++index)
    {
        const QJsonObject layer = layers.at(index).toObject();
        const QString relativePath = layer.value(QStringLiteral("path")).toString();
        const QString absolutePath = QDir(package).filePath(relativePath);
        if (layer.value(QStringLiteral("index")).toInt(-1) != index
            || layer.value(QStringLiteral("widthPx")).toInt() != width
            || layer.value(QStringLiteral("heightPx")).toInt() != height
            || !relativePath.startsWith(QStringLiteral("layers/"))
            || !IsContainedPath(inputDirectory, absolutePath)
            || !QFileInfo(absolutePath).isFile())
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("RIP 输入层列表、尺寸或路径不闭合。");
            }
            return false;
        }
        metadata->layerPaths.push_back(absolutePath);
    }

    metadata->packageDirectory = package;
    metadata->inputDirectory = inputDirectory;
    metadata->sourceRoot = package;
    metadata->stagingRoot = package;
    metadata->manifestPath = manifestPath;
    metadata->manifestSha256 = FileSha256(manifestPath);
    metadata->layerCount = layerCount;
    metadata->widthPx = static_cast<std::uint32_t>(width);
    metadata->heightPx = static_cast<std::uint32_t>(height);
    return true;
}

bool HostRipJobController::InspectManualRequest(
    const QString& inputDirectory,
    const QString& outputDirectory,
    PackageMetadata* metadata,
    QString* error) const
{
    if (metadata == nullptr)
    {
        return false;
    }
    *metadata = {};
    if (inputDirectory.isEmpty() || outputDirectory.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("请先选择切片文件夹与 RIP 输出文件夹。");
        }
        return false;
    }
    const QString input = QDir::cleanPath(
        QFileInfo(inputDirectory).absoluteFilePath());
    const QString output = QDir::cleanPath(
        QFileInfo(outputDirectory).absoluteFilePath());
    if (!QFileInfo(input).isDir())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("切片文件夹不存在：%1").arg(input);
        }
        return false;
    }
    const QString sourceRoot = QDir::cleanPath(
        QFileInfo(input).absolutePath());
    const QString stagingRoot = QDir::cleanPath(
        QFileInfo(output).absolutePath());
    if (sourceRoot.isEmpty() || sourceRoot == input)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "切片文件夹不能是盘符根目录，请选择其下一级目录。");
        }
        return false;
    }
    if (stagingRoot.isEmpty() || stagingRoot == output
        || !QFileInfo(stagingRoot).isDir())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "RIP 输出文件夹的上级目录必须已存在，且输出不能是盘符根目录。");
        }
        return false;
    }
    if (QFileInfo::exists(output))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 输出目录已存在，不会覆盖：%1")
                         .arg(output);
        }
        return false;
    }
    if (IsContainedPath(input, output, true)
        || IsContainedPath(output, input, true))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "RIP 输出文件夹不能与切片文件夹相同或互相嵌套。");
        }
        return false;
    }

    QDir directory(input);
    directory.setFilter(QDir::Files | QDir::NoSymLinks);
    directory.setNameFilters(
        QStringList{QStringLiteral("*.tif"), QStringLiteral("*.tiff")});
    directory.setSorting(QDir::Name | QDir::IgnoreCase);
    const QStringList names = directory.entryList();
    if (names.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("切片文件夹里没有 *.tif / *.tiff：%1")
                         .arg(input);
        }
        return false;
    }
    if (names.size() > 1000000)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("切片文件夹的层数超过上限 1000000。");
        }
        return false;
    }
    for (const QString& name : names)
    {
        metadata->layerPaths.push_back(directory.absoluteFilePath(name));
    }

    slicesoft::rip::RipInputGeometry geometry;
    const slicesoft::rip::RipStatus probe =
        slicesoft::rip::ProbeRipInputGeometry(
            FsPath(metadata->layerPaths.constFirst()), &geometry);
    if (!probe.ok)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("首层切片不符合 RIP 输入要求：%1 · %2")
                         .arg(RipCode(probe), RipMessage(probe));
        }
        metadata->layerPaths.clear();
        return false;
    }

    metadata->inputDirectory = input;
    metadata->sourceRoot = sourceRoot;
    metadata->stagingRoot = stagingRoot;
    metadata->outputDirectory = output;
    metadata->layerCount = static_cast<int>(metadata->layerPaths.size());
    metadata->widthPx = geometry.width_px;
    metadata->heightPx = geometry.height_px;
    return true;
}

bool HostRipJobController::CheckManualRequest(
    const QString& inputDirectory,
    const QString& outputDirectory,
    const QString& moduleDirectory,
    const hostripsettings& settings,
    QString* error) const
{
    QString validationError;
    RuntimeMetadata runtime;
    PackageMetadata manual;
    const bool valid = HostRipSettingsStore::Validate(
            settings, &validationError)
        && InspectRuntime(moduleDirectory, &runtime, &validationError)
        && InspectManualRequest(
            inputDirectory, outputDirectory, &manual, &validationError);
    if (!valid && error != nullptr)
    {
        *error = validationError;
    }
    return valid;
}

bool HostRipJobController::StartManual(
    const QString& inputDirectory,
    const QString& outputDirectory,
    const QString& moduleDirectory,
    const hostripsettings& settings,
    QString* error)
{
    if (IsActive())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("已有 RIP 作业正在运行。");
        }
        return false;
    }
    if (m_process != nullptr || m_validationThread != nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("上一 RIP 作业仍在收口。");
        }
        return false;
    }
    QString validationError;
    if (!HostRipSettingsStore::Validate(settings, &validationError)
        || !InspectRuntime(moduleDirectory, &m_runtime, &validationError)
        || !InspectManualRequest(
            inputDirectory, outputDirectory, &m_package, &validationError))
    {
        if (error != nullptr)
        {
            *error = validationError;
        }
        return false;
    }
    m_scope = JobScope::Manual;
    m_settings = settings;
    m_sourceIdentity.clear();
    m_stagingDirectory.clear();
    m_stdout.clear();
    m_stderr.clear();
    m_cancelRequested = false;
    m_timedOut = false;
    m_cancelToken = std::make_shared<std::atomic_bool>(false);
    ++m_jobGeneration;
    m_elapsed.start();
    m_phase = Phase::ValidatingInput;
    PublishState(
        QStringLiteral("validating_input"),
        QStringLiteral("正在后台检查人工指定切片文件夹的层与文件身份"));
    StartInputValidation();
    return true;
}

bool HostRipJobController::Start(
    const QString& packageDirectory,
    const QString& moduleDirectory,
    const hostripsettings& settings,
    QString* error)
{
    if (IsActive())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("已有 RIP 作业正在运行。");
        }
        return false;
    }
    if (m_process != nullptr || m_validationThread != nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("上一 RIP 作业仍在收口。");
        }
        return false;
    }
    QString validationError;
    if (!HostRipSettingsStore::Validate(settings, &validationError)
        || !InspectRuntime(moduleDirectory, &m_runtime, &validationError)
        || !InspectPackage(packageDirectory, &m_package, &validationError))
    {
        if (error != nullptr)
        {
            *error = validationError;
        }
        return false;
    }
    const QString outputDirectoryName =
        HostRipSettingsStore::EffectiveOutputDirectoryName(settings);
    if (QFileInfo(QDir(m_package.packageDirectory).filePath(
            outputDirectoryName)).exists())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 输出目录已存在，不会覆盖：%1/%2")
                         .arg(m_package.packageDirectory, outputDirectoryName);
        }
        return false;
    }
    m_scope = JobScope::Package;
    m_settings = settings;
    m_sourceIdentity.clear();
    m_stagingDirectory.clear();
    m_stdout.clear();
    m_stderr.clear();
    m_cancelRequested = false;
    m_timedOut = false;
    m_cancelToken = std::make_shared<std::atomic_bool>(false);
    ++m_jobGeneration;
    m_elapsed.start();
    m_phase = Phase::ValidatingInput;
    PublishState(
        QStringLiteral("validating_input"),
        QStringLiteral("正在后台检查 S1 切片层与源文件身份"));
    StartInputValidation();
    return true;
}

void HostRipJobController::StartInputValidation()
{
    std::vector<std::filesystem::path> inputLayers;
    inputLayers.reserve(static_cast<std::size_t>(m_package.layerPaths.size()));
    for (const QString& layerPath : m_package.layerPaths)
    {
        inputLayers.push_back(FsPath(layerPath));
    }
    const auto cancelToken = m_cancelToken;
    const slicesoft::rip::RipInputValidationRequest request{
        FsPath(m_package.sourceRoot),
        FsPath(m_package.inputDirectory),
        std::move(inputLayers),
        m_package.widthPx,
        m_package.heightPx,
        [cancelToken]()
        {
            return cancelToken->load(std::memory_order_relaxed);
        }};
    QStringList sourcePaths;
    if (m_scope == JobScope::Package)
    {
        sourcePaths.push_back(m_package.manifestPath);
    }
    sourcePaths.append(m_package.layerPaths);
    const QString packageDirectory = m_package.sourceRoot;
    const auto state = std::make_shared<InputValidationState>();
    const quint64 generation = m_jobGeneration;
    QThread* thread = QThread::create(
        [request, sourcePaths, packageDirectory, cancelToken, state]()
        {
            for (const QString& sourcePath : sourcePaths)
            {
                if (cancelToken->load(std::memory_order_relaxed))
                {
                    return;
                }
                QVector<hostripsourcefileidentity> item;
                if (!HostRipSafety::CaptureSourceIdentity(
                        packageDirectory,
                        QStringList{sourcePath},
                        &item,
                        &state->identityError))
                {
                    return;
                }
                state->sourceIdentity.push_back(item.constFirst());
            }
            for (const std::filesystem::path& layerPath : request.layer_paths)
            {
                if (cancelToken->load(std::memory_order_relaxed))
                {
                    return;
                }
                slicesoft::rip::RipInputValidationRequest layerRequest{
                    request.package_directory,
                    request.input_directory,
                    {layerPath},
                    request.expected_width_px,
                    request.expected_height_px,
                    request.is_cancelled};
                state->status = slicesoft::rip::ValidateRipInput(layerRequest);
                if (!state->status.ok)
                {
                    return;
                }
            }
        });
    thread->setParent(this);
    m_validationThread = thread;
    connect(thread, &QThread::finished, this,
        [this, thread, state, generation]()
        {
            if (m_validationThread == thread)
            {
                m_validationThread = nullptr;
            }
            thread->deleteLater();
            if (generation != m_jobGeneration
                || m_phase != Phase::ValidatingInput)
            {
                return;
            }
            if (m_cancelRequested
                || (m_cancelToken != nullptr
                    && m_cancelToken->load(std::memory_order_relaxed)))
            {
                FinishCancelled(QStringLiteral("验证期间取消"));
                return;
            }
            if (!state->identityError.isEmpty())
            {
                FinishFailure(
                    QStringLiteral("RIP_SOURCE_IDENTITY_CAPTURE_FAILED"),
                    state->identityError);
                return;
            }
            if (!state->status.ok)
            {
                FinishFailure(RipCode(state->status), RipMessage(state->status));
                return;
            }
            if (state->sourceIdentity.isEmpty()
                || (m_scope == JobScope::Package
                    && QString::fromLatin1(
                           state->sourceIdentity.constFirst().sha256.toHex())
                        != m_package.manifestSha256))
            {
                FinishFailure(
                    QStringLiteral("RIP_SOURCE_PACKAGE_CHANGED"),
                    m_scope == JobScope::Package
                        ? QStringLiteral("RIP 前置检查期间 manifest 身份发生变化。")
                        : QStringLiteral("RIP 前置检查期间切片文件身份发生变化。"));
                return;
            }
            m_sourceIdentity = state->sourceIdentity;
            StartExternalProcess();
        });
    thread->start();
}

void HostRipJobController::StartExternalProcess()
{
    if (m_cancelRequested)
    {
        FinishCancelled(QStringLiteral("进程启动前取消"));
        return;
    }
    m_stagingDirectory = QDir(m_package.stagingRoot).filePath(
        QStringLiteral(".rip.staging.%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(m_stagingDirectory))
    {
        FinishFailure(
            QStringLiteral("RIP_STAGING_CREATE_FAILED"),
            QStringLiteral("无法创建 RIP staging 目录。"));
        return;
    }

    slicesoft::rip::RipSettings coreSettings;
    coreSettings.auto_run_after_slice = m_settings.autoafterslice;
    coreSettings.intent = m_settings.renderintent;
    coreSettings.transparent_mode = m_settings.transparentmode;
    coreSettings.color_mode = m_settings.colormode;
    coreSettings.continue_on_layer_error = m_settings.continueonerror;
    coreSettings.gray_bits = m_settings.devicegraybits;
    coreSettings.input_icc_path = FsPath(
        QDir(m_runtime.moduleDirectory).filePath(m_settings.inputicc));
    coreSettings.output_icc_path = FsPath(
        QDir(m_runtime.moduleDirectory).filePath(m_settings.outputicc));
    coreSettings.output_directory_name = "rip";
    slicesoft::rip::RipCommand command;
    const slicesoft::rip::RipStatus commandStatus =
        slicesoft::rip::BuildRipCommand(
            slicesoft::rip::RipCommandRequest{
                {FsPath(m_runtime.moduleDirectory), FsPath(m_runtime.cliPath),
                 FsPath(m_runtime.dllPath), FsPath(m_runtime.resourceDirectory)},
                FsPath(m_package.stagingRoot),
                FsPath(m_package.inputDirectory),
                FsPath(m_stagingDirectory),
                coreSettings,
                m_scope == JobScope::Package
                    ? slicesoft::rip::RipCommandScope::PackageBound
                    : slicesoft::rip::RipCommandScope::ManualUnbound},
            &command);
    if (!commandStatus.ok)
    {
        FinishFailure(RipCode(commandStatus), RipMessage(commandStatus));
        return;
    }

    m_process = new QProcess(this);
    QStringList arguments;
    for (const std::string& argument : command.arguments)
    {
        arguments.push_back(QString::fromUtf8(
            argument.data(), static_cast<int>(argument.size())));
    }
    m_process->setProgram(QtPath(command.program));
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(QtPath(command.working_directory));
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("PATH"),
        m_runtime.moduleDirectory + QDir::listSeparator()
            + environment.value(QStringLiteral("PATH")));
    m_process->setProcessEnvironment(environment);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &HostRipJobController::OnReadyStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &HostRipJobController::OnReadyStandardError);
    connect(
        m_process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &HostRipJobController::OnProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &HostRipJobController::OnProcessError);
    m_phase = Phase::RunningProcess;
    PublishState(QStringLiteral("starting"), QStringLiteral("正在启动 RIP 进程"));
    m_process->start();
    m_timeoutTimer.start(m_settings.timeoutseconds * 1000);
}

bool HostRipJobController::Cancel(QString* error)
{
    if (!IsActive())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("当前没有 RIP 作业。");
        }
        return false;
    }
    if (m_cancelRequested)
    {
        return true;
    }
    m_cancelRequested = true;
    if (m_cancelToken != nullptr)
    {
        m_cancelToken->store(true, std::memory_order_relaxed);
    }
    PublishState(QStringLiteral("cancelling"), QStringLiteral("正在停止 RIP 进程"));
    if (m_phase == Phase::RunningProcess && m_process != nullptr)
    {
        m_process->terminate();
        m_killTimer.start(2000);
    }
    return true;
}

bool HostRipJobController::IsActive() const
{
    return m_phase != Phase::Idle;
}

void HostRipJobController::OnReadyStandardOutput()
{
    if (m_process != nullptr)
    {
        AppendCapped(&m_stdout, m_process->readAllStandardOutput());
        PublishState(QStringLiteral("running"), QStringLiteral("RIP 正在处理切片层"));
    }
}

void HostRipJobController::OnReadyStandardError()
{
    if (m_process != nullptr)
    {
        AppendCapped(&m_stderr, m_process->readAllStandardError());
    }
}

void HostRipJobController::OnProcessFinished(
    const int exitCode,
    const QProcess::ExitStatus exitStatus)
{
    if (m_process == nullptr || m_phase != Phase::RunningProcess)
    {
        return;
    }
    OnReadyStandardOutput();
    OnReadyStandardError();
    m_timeoutTimer.stop();
    m_killTimer.stop();
    if (m_cancelRequested)
    {
        FinishCancelled(
            m_timedOut ? QStringLiteral("RIP 超时并已终止")
                       : QStringLiteral("操作员取消"));
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0)
    {
        FinishFailure(
            exitStatus != QProcess::NormalExit
                ? QStringLiteral("RIP_PROCESS_CRASHED")
                : QStringLiteral("RIP_PROCESS_EXIT_FAILED"),
            QStringLiteral("RIP 进程退出异常，exitCode=%1 · %2")
                .arg(exitCode)
                .arg(QString::fromUtf8(m_stderr).trimmed()));
        return;
    }
    StartOutputValidation(exitCode);
}

void HostRipJobController::OnProcessError(const QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart
        && m_phase == Phase::RunningProcess)
    {
        FinishFailure(
            QStringLiteral("RIP_PROCESS_START_FAILED"),
            QStringLiteral("RIP 进程无法启动：%1")
                .arg(m_process != nullptr ? m_process->errorString() : QString{}));
    }
}

void HostRipJobController::OnTimeout()
{
    if (!IsActive())
    {
        return;
    }
    m_timedOut = true;
    QString ignored;
    (void)Cancel(&ignored);
}

void HostRipJobController::StartOutputValidation(const int exitCode)
{
    m_timeoutTimer.stop();
    m_killTimer.stop();
    ResetProcess();
    m_phase = Phase::ValidatingOutput;
    PublishState(
        QStringLiteral("validating_output"),
        QStringLiteral("正在后台检查真实 RIP TIFF 与源文件身份"));
    const auto cancelToken = m_cancelToken;
    const slicesoft::rip::RipOutputValidationRequest request{
        FsPath(m_package.stagingRoot),
        FsPath(m_stagingDirectory),
        static_cast<std::size_t>(m_package.layerCount),
        m_package.widthPx,
        m_package.heightPx,
        m_settings.devicegraybits,
        HostRipSettingsStore::IsDiagnosticMode(m_settings)
            ? slicesoft::rip::RipOutputValidationMode::DiagnosticUnvalidated
            : slicesoft::rip::RipOutputValidationMode::StrictS2,
        [cancelToken]()
        {
            return cancelToken->load(std::memory_order_relaxed);
        }};
    const QString packageDirectory = m_package.sourceRoot;
    const QVector<hostripsourcefileidentity> sourceIdentity = m_sourceIdentity;
    const auto state = std::make_shared<OutputValidationState>();
    const quint64 generation = m_jobGeneration;
    QThread* thread = QThread::create(
        [request, packageDirectory, sourceIdentity, cancelToken, state]()
        {
            if (cancelToken->load(std::memory_order_relaxed))
            {
                return;
            }
            state->validation =
                slicesoft::rip::ValidateAndNormalizeRipOutput(request);
            if (!state->validation.status.ok
                || cancelToken->load(std::memory_order_relaxed))
            {
                return;
            }
            state->sourceIdentityValid = true;
            for (const hostripsourcefileidentity& item : sourceIdentity)
            {
                if (cancelToken->load(std::memory_order_relaxed))
                {
                    state->sourceIdentityValid = false;
                    return;
                }
                if (!HostRipSafety::VerifySourceIdentity(
                        packageDirectory,
                        QVector<hostripsourcefileidentity>{item},
                        &state->identityError))
                {
                    state->sourceIdentityValid = false;
                    return;
                }
            }
        });
    thread->setParent(this);
    m_validationThread = thread;
    connect(thread, &QThread::finished, this,
        [this, thread, state, generation, exitCode]()
        {
            if (m_validationThread == thread)
            {
                m_validationThread = nullptr;
            }
            thread->deleteLater();
            if (generation != m_jobGeneration
                || m_phase != Phase::ValidatingOutput)
            {
                return;
            }
            if (m_cancelRequested
                || (m_cancelToken != nullptr
                    && m_cancelToken->load(std::memory_order_relaxed)))
            {
                FinishCancelled(QStringLiteral("输出验证期间取消"));
                return;
            }
            if (!state->validation.status.ok)
            {
                FinishFailure(
                    RipCode(state->validation.status),
                    RipMessage(state->validation.status));
                return;
            }
            if (!state->sourceIdentityValid)
            {
                FinishFailure(
                    QStringLiteral("RIP_SOURCE_PACKAGE_CHANGED"),
                    state->identityError.isEmpty()
                        ? (m_scope == JobScope::Package
                               ? QStringLiteral("RIP 运行期间源切片包发生变化。")
                               : QStringLiteral("RIP 运行期间源切片文件发生变化。"))
                        : state->identityError);
                return;
            }
            FinalizeValidatedOutput(exitCode, state->validation);
        });
    thread->start();
}

void HostRipJobController::FinalizeValidatedOutput(
    const int exitCode,
    const slicesoft::rip::RipOutputValidationResult& validation)
{
    m_phase = Phase::Publishing;
    const bool diagnostic =
        HostRipSettingsStore::IsDiagnosticMode(m_settings);
    if (!diagnostic && !validation.s2_drop_limits_passed)
    {
        FinishFailure(
            QStringLiteral("RIP_OUTPUT_STRICT_STATE_INVALID"),
            QStringLiteral("严格 S2 验证产生了未通过的内部状态。"));
        return;
    }

    std::array<int, 3> minima{255, 255, 255};
    std::array<int, 3> maxima{0, 0, 0};
    for (const auto& layer : validation.layers)
    {
        minima[0] = (std::min)(minima[0], static_cast<int>(layer.minimum_white));
        minima[1] = (std::min)(minima[1], static_cast<int>(layer.minimum_support));
        minima[2] = (std::min)(minima[2], static_cast<int>(layer.minimum_varnish));
        maxima[0] = (std::max)(maxima[0], static_cast<int>(layer.maximum_white));
        maxima[1] = (std::max)(maxima[1], static_cast<int>(layer.maximum_support));
        maxima[2] = (std::max)(maxima[2], static_cast<int>(layer.maximum_varnish));
    }
    const QJsonObject settingsObject{
        {QStringLiteral("schema"), QStringLiteral("slicesoft.rip.settings.2")},
        {QStringLiteral("autoAfterSlice"), m_settings.autoafterslice},
        {QStringLiteral("renderIntent"), m_settings.renderintent},
        {QStringLiteral("transparentMode"), m_settings.transparentmode},
        {QStringLiteral("colorMode"), m_settings.colormode},
        {QStringLiteral("inputIcc"), m_settings.inputicc},
        {QStringLiteral("outputIcc"), m_settings.outputicc},
        {QStringLiteral("continueOnError"), m_settings.continueonerror},
        {QStringLiteral("deviceGrayBits"), m_settings.devicegraybits},
        {QStringLiteral("timeoutSeconds"), m_settings.timeoutseconds},
        {QStringLiteral("outputValidationMode"),
         m_settings.outputvalidationmode},
        {QStringLiteral("outputDirectoryName"), QStringLiteral("rip")},
        {QStringLiteral("existingOutputPolicy"), QStringLiteral("fail_closed")}};
    const QJsonObject moduleObject{
        {QStringLiteral("moduleId"), QStringLiteral("slicesoft.external_rip")},
        {QStringLiteral("version"), m_runtime.version},
        {QStringLiteral("manifestSha256"), m_runtime.manifestSha256}};
    const QJsonObject processObject{
        {QStringLiteral("exitCode"), exitCode},
        {QStringLiteral("elapsedMs"), m_elapsed.elapsed()},
        {QStringLiteral("stdout"), QString::fromUtf8(m_stdout)},
        {QStringLiteral("stderr"), QString::fromUtf8(m_stderr)}};
    QJsonObject outputObject{
        {QStringLiteral("directory"), m_scope == JobScope::Manual
             ? m_package.outputDirectory
             : (diagnostic ? QStringLiteral("rip_diagnostic")
                           : QStringLiteral("rip"))},
        {QStringLiteral("layerCount"), static_cast<int>(validation.layers.size())},
        {QStringLiteral("filePattern"), QStringLiteral("rip_%06d.tif")},
        {QStringLiteral("minimum"), QJsonObject{
             {QStringLiteral("W"), minima[0]},
             {QStringLiteral("S"), minima[1]},
             {QStringLiteral("V"), minima[2]}}},
        {QStringLiteral("maximum"), QJsonObject{
             {QStringLiteral("W"), maxima[0]},
             {QStringLiteral("S"), maxima[1]},
             {QStringLiteral("V"), maxima[2]}}}};
    if (diagnostic)
    {
        const std::array<int, 3> limits = m_settings.devicegraybits == 1
            ? std::array<int, 3>{2, 3, 3}
            : std::array<int, 3>{6, 9, 9};
        outputObject.insert(
            QStringLiteral("s2DropLimitsPassed"),
            validation.s2_drop_limits_passed);
        outputObject.insert(
            QStringLiteral("s2PublicationEligible"), false);
        outputObject.insert(
            QStringLiteral("referenceDropLimit"), QJsonObject{
                {QStringLiteral("W"), limits[0]},
                {QStringLiteral("S"), limits[1]},
                {QStringLiteral("V"), limits[2]}});
        outputObject.insert(
            QStringLiteral("samplesExceedingLimit"), QJsonObject{
                {QStringLiteral("W"), static_cast<qint64>(
                    validation.samples_exceeding_drop_limit[0])},
                {QStringLiteral("S"), static_cast<qint64>(
                    validation.samples_exceeding_drop_limit[1])},
                {QStringLiteral("V"), static_cast<qint64>(
                    validation.samples_exceeding_drop_limit[2])}});
        if (validation.first_drop_violation.has_value())
        {
            const auto& violation = *validation.first_drop_violation;
            outputObject.insert(
                QStringLiteral("firstExceedance"), QJsonObject{
                    {QStringLiteral("layerIndex"), static_cast<qint64>(
                        violation.layer_index)},
                    {QStringLiteral("channel"), QString::fromStdString(
                        violation.channel)},
                    {QStringLiteral("value"), violation.value},
                    {QStringLiteral("limit"), violation.limit},
                    {QStringLiteral("x"), static_cast<qint64>(violation.x)},
                    {QStringLiteral("y"), static_cast<qint64>(violation.y)}});
        }
    }
    const QJsonObject result{
        {QStringLiteral("schema"), diagnostic
             ? QStringLiteral("slicesoft.rip.diagnostic.2")
             : QStringLiteral("slicesoft.rip.result.2")},
        {QStringLiteral("status"), diagnostic
             ? QStringLiteral("diagnostic_unvalidated")
             : QStringLiteral("succeeded")},
        {QStringLiteral("externalValidation"), QStringLiteral("EXTERNAL_VALIDATION_DEFERRED")},
        {QStringLiteral("sourceBinding"), m_scope == JobScope::Package
             ? QStringLiteral("package_bound")
             : QStringLiteral("manual_unbound")},
        {QStringLiteral("sourcePackage"), m_package.packageDirectory},
        {QStringLiteral("sourceInputDirectory"), m_package.inputDirectory},
        {QStringLiteral("sourceLayerCount"), m_package.layerCount},
        {QStringLiteral("sourceManifestSha256"), m_package.manifestSha256},
        {QStringLiteral("module"), moduleObject},
        {QStringLiteral("settings"), settingsObject},
        {QStringLiteral("process"), processObject},
        {QStringLiteral("output"), outputObject}};
    QFile resultFile(QDir(m_stagingDirectory).filePath(
        diagnostic
            ? QStringLiteral("rip_diagnostic_result.json")
            : QStringLiteral("rip_result.json")));
    if (!resultFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || resultFile.write(QJsonDocument(result).toJson(QJsonDocument::Indented)) < 0
        || !resultFile.flush())
    {
        FinishFailure(
            QStringLiteral("RIP_RESULT_WRITE_FAILED"),
            QStringLiteral("无法写入 RIP 结果报告。"));
        return;
    }
    resultFile.close();
    const QString outputDirectoryName =
        HostRipSettingsStore::EffectiveOutputDirectoryName(m_settings);
    PublishState(
        QStringLiteral("publishing"),
        diagnostic ? QStringLiteral("正在保存不可打印的 RIP 诊断数据")
                   : QStringLiteral("正在发布严格 S2 RIP 输出"));
    const auto published = m_scope == JobScope::Package
        ? slicesoft::rip::PublishRipArtifact(
            slicesoft::rip::RipArtifactPublishRequest{
                FsPath(m_package.packageDirectory),
                FsPath(m_stagingDirectory),
                outputDirectoryName.toStdString()})
        : slicesoft::rip::PublishManualRipArtifact(
            slicesoft::rip::RipManualArtifactPublishRequest{
                FsPath(m_stagingDirectory),
                FsPath(m_package.outputDirectory)});
    if (!published.status.ok)
    {
        FinishFailure(RipCode(published.status), RipMessage(published.status));
        return;
    }
    const QString outputDirectory = QtPath(published.output_directory);
    m_stagingDirectory.clear();
    const qint64 elapsedMs = m_elapsed.elapsed();
    ResetProcess();
    m_cancelToken.reset();
    m_sourceIdentity.clear();
    m_phase = Phase::Idle;
    emit SigCompleted(
        true,
        false,
        diagnostic ? QStringLiteral("RIP_DIAGNOSTIC_SAVED")
                   : QStringLiteral("RIP_SUCCEEDED"),
        diagnostic
            ? QStringLiteral("%1 层已完成结构检查并保存；未按 S2 发布，不可打印")
                  .arg(static_cast<qulonglong>(validation.layers.size()))
            : QStringLiteral("%1 层已校验并发布；外部验收仍延期")
                  .arg(static_cast<qulonglong>(validation.layers.size())),
        outputDirectory,
        elapsedMs);
}

void HostRipJobController::FinishFailure(
    const QString& code,
    const QString& message)
{
    if (m_phase == Phase::Idle)
    {
        return;
    }
    m_timeoutTimer.stop();
    m_killTimer.stop();
    QString cleanupError;
    const bool cleaned = CleanupOwnedStaging(&cleanupError);
    const qint64 elapsedMs = m_elapsed.isValid() ? m_elapsed.elapsed() : 0;
    ResetProcess();
    m_cancelToken.reset();
    m_sourceIdentity.clear();
    m_phase = Phase::Idle;
    emit SigCompleted(
        false,
        false,
        cleaned ? code : QStringLiteral("RIP_STAGING_CLEANUP_REFUSED"),
        cleaned ? message
                : QStringLiteral("%1 · %2").arg(message, cleanupError),
        QString{},
        elapsedMs);
}

void HostRipJobController::FinishCancelled(const QString& message)
{
    m_timeoutTimer.stop();
    m_killTimer.stop();
    QString cleanupError;
    const bool cleaned = CleanupOwnedStaging(&cleanupError);
    const qint64 elapsedMs = m_elapsed.isValid() ? m_elapsed.elapsed() : 0;
    ResetProcess();
    m_cancelToken.reset();
    m_sourceIdentity.clear();
    m_phase = Phase::Idle;
    emit SigCompleted(
        false,
        true,
        cleaned
            ? (m_timedOut ? QStringLiteral("RIP_TIMEOUT")
                          : QStringLiteral("RIP_CANCELLED"))
            : QStringLiteral("RIP_STAGING_CLEANUP_REFUSED"),
        cleaned ? message
                : QStringLiteral("%1 · %2").arg(message, cleanupError),
        QString{},
        elapsedMs);
}

bool HostRipJobController::CleanupOwnedStaging(QString* error)
{
    if (m_stagingDirectory.isEmpty())
    {
        return true;
    }
    if ((m_process != nullptr
            && m_process->state() != QProcess::NotRunning)
        || (m_validationThread != nullptr
            && m_validationThread->isRunning()))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "RIP 进程或验证线程尚未停止，拒绝清理 staging。");
        }
        return false;
    }
    if (!HostRipSafety::RemoveOwnedStaging(
            m_package.stagingRoot, m_stagingDirectory, error))
    {
        return false;
    }
    m_stagingDirectory.clear();
    return true;
}

void HostRipJobController::ResetProcess()
{
    if (m_process != nullptr)
    {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void HostRipJobController::PublishState(
    const QString& state,
    const QString& message)
{
    emit SigStateChanged(state, message);
}
