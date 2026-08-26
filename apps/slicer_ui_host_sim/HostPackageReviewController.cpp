#include "HostPackageReviewController.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaObject>
#include <QTemporaryDir>

#include <exception>
#include <algorithm>
#include <iterator>
#include <utility>

namespace
{
constexpr std::array<const char*, 7> ChannelNames{
    "R", "G", "B", "W", "S", "V", "T"};

QString ResultError(const QJsonObject& response, const QString& fallback)
{
    const QJsonObject error = response.value(QStringLiteral("error")).toObject();
    const QString code = response.value(QStringLiteral("code")).toString();
    const QString message = error.value(QStringLiteral("message")).toString(
        response.value(QStringLiteral("message")).toString());
    const QString detail = error.value(QStringLiteral("detail")).toString(
        response.value(QStringLiteral("detail")).toString());
    const QString text = QStringLiteral("%1 %2 %3")
        .arg(code, message, detail).trimmed();
    return text.isEmpty() ? fallback : text;
}

bool ReadChannelCounts(
    const QJsonObject& object,
    const QStringList& channels,
    hostchannelcounts* counts)
{
    if (counts == nullptr)
    {
        return false;
    }
    for (const QString& channel : channels)
    {
        const auto found = std::find_if(
            ChannelNames.begin(),
            ChannelNames.end(),
            [&channel](const char* name)
            {
                return channel == QString::fromLatin1(name);
            });
        if (found == ChannelNames.end())
        {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(ChannelNames.begin(), found));
        const QJsonValue value = object.value(channel);
        if (!value.isDouble() || value.toDouble() < 0.0)
        {
            return false;
        }
        counts->values[index] = static_cast<quint64>(value.toDouble());
    }
    return true;
}

bool IsFrozenChannelSet(const QStringList& channels)
{
    const QStringList rgbwsv{
        QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"),
        QStringLiteral("W"), QStringLiteral("S"), QStringLiteral("V")};
    QStringList rgbwsvt = rgbwsv;
    rgbwsvt.append(QStringLiteral("T"));
    return channels == rgbwsv || channels == rgbwsvt;
}

bool IsFrozenChannelName(const QString& channel)
{
    for (const char* name : ChannelNames)
    {
        if (channel == QString::fromLatin1(name))
        {
            return true;
        }
    }
    return false;
}
}

HostPackageReviewController::HostPackageReviewController(ModuleClient& client)
    : QObject(nullptr),
      m_client(client),
      m_previewCache(std::make_unique<QTemporaryDir>(
          QDir(QDir::tempPath()).filePath(
              QStringLiteral("slicesoft-host-preview-XXXXXX"))))
{
}

HostPackageReviewController::~HostPackageReviewController()
{
    if (m_loadThread.joinable())
    {
        m_loadThread.join();
    }
}

bool HostPackageReviewController::Load(
    const QString& packageDirectory,
    QString* error)
{
    m_review = {};
    const QFileInfo packageInfo(packageDirectory);
    if (!m_client.IsOpen() || packageDirectory.trimmed().isEmpty()
        || !packageInfo.isDir())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("生产包目录不存在或模块尚未加载：%1")
                         .arg(packageDirectory);
        }
        return false;
    }
    m_review.packagedirectory = QDir::cleanPath(
        packageInfo.absoluteFilePath());
    return LoadVerification(error)
        && LoadSummary(error)
        && LoadLayers(error);
}

bool HostPackageReviewController::LoadAsync(
    const QString& packageDirectory,
    LoadCallback callback,
    QString* error)
{
    if (!callback || m_loading.exchange(true))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("结果包正在加载，不能重复提交。");
        }
        return false;
    }
    if (m_loadThread.joinable())
    {
        m_loadThread.join();
    }
    try
    {
        m_loadThread = std::thread(
            [this, packageDirectory, callback = std::move(callback)]() mutable
            {
                QString loadError;
                const bool loaded = Load(packageDirectory, &loadError);
                QMetaObject::invokeMethod(
                    this,
                    [this,
                     loaded,
                     loadError = std::move(loadError),
                     callback = std::move(callback)]() mutable
                    {
                        if (m_loadThread.joinable())
                        {
                            m_loadThread.join();
                        }
                        m_loading.store(false);
                        callback(loaded, loadError);
                    },
                    Qt::QueuedConnection);
            });
    }
    catch (const std::exception& exception)
    {
        m_loading.store(false);
        if (error != nullptr)
        {
            *error = QStringLiteral("无法启动结果包后台加载：%1")
                         .arg(QString::fromUtf8(exception.what()));
        }
        return false;
    }
    return true;
}

bool HostPackageReviewController::IsLoading() const
{
    return m_loading.load();
}

bool HostPackageReviewController::RenderPreview(
    const int layerIndex,
    const QStringList& channels,
    QString* outputPath,
    QString* error)
{
    if (outputPath == nullptr || layerIndex < 0
        || layerIndex >= m_review.layercount || channels.isEmpty()
        || !m_previewCache->isValid())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("层索引、通道或宿主预览缓存无效。");
        }
        return false;
    }
    QJsonArray channelArray;
    QStringList cacheParts;
    for (const QString& channel : channels)
    {
        if (!IsFrozenChannelName(channel)
            || !m_review.channels.contains(channel))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("不支持的生产通道：%1").arg(channel);
            }
            return false;
        }
        channelArray.append(channel);
        cacheParts.append(channel);
    }
    const QString previewPath = QDir(m_previewCache->path()).filePath(
        QStringLiteral("layer_%1_%2.png")
            .arg(layerIndex, 6, 10, QLatin1Char('0'))
            .arg(cacheParts.join(QStringLiteral("_"))));
    QJsonObject response;
    if (!ExecuteObject(
            QJsonObject{
                {QStringLiteral("capability"),
                 QStringLiteral("package.render_layer_preview")},
                {QStringLiteral("packageDir"), m_review.packagedirectory},
                {QStringLiteral("layerIndex"), layerIndex},
                {QStringLiteral("mode"), channels.size() == 1
                     ? QStringLiteral("single_channel")
                     : QStringLiteral("composite")},
                {QStringLiteral("channels"), channelArray},
                {QStringLiteral("maxWidthPx"), 1200},
                {QStringLiteral("outputPath"), previewPath}},
            &response,
            error))
    {
        return false;
    }
    const QString actualPath = response.value(
        QStringLiteral("outputPath")).toString();
    if (actualPath.isEmpty() || !QFileInfo(actualPath).isFile())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块未发布可读取的生产层预览。");
        }
        return false;
    }
    *outputPath = actualPath;
    return true;
}

bool HostPackageReviewController::ReadReport(
    const QString& reportName,
    hostpackagereport* report,
    QString* error)
{
    if (report == nullptr || reportName.trimmed().isEmpty()
        || m_review.packagedirectory.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("报告名或生产包尚未准备完成。");
        }
        return false;
    }
    QJsonObject response;
    if (!ExecuteObject(
            QJsonObject{
                {QStringLiteral("capability"),
                 QStringLiteral("package.read_report")},
                {QStringLiteral("packageDir"), m_review.packagedirectory},
                {QStringLiteral("reportName"), reportName}},
            &response,
            error))
    {
        return false;
    }
    report->name = response.value(QStringLiteral("reportName")).toString();
    report->schema = response.value(QStringLiteral("reportSchema")).toString();
    report->sourcepath = response.value(QStringLiteral("sourcePath")).toString();
    report->data = response.value(QStringLiteral("data")).toObject();
    if (report->name != reportName || report->schema.isEmpty()
        || report->data.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("命名报告响应不完整：%1").arg(reportName);
        }
        return false;
    }
    return true;
}

const hostpackagereview& HostPackageReviewController::Review() const
{
    return m_review;
}

bool HostPackageReviewController::ExecuteObject(
    const QJsonObject& request,
    QJsonObject* response,
    QString* error)
{
    if (response == nullptr)
    {
        return false;
    }
    QByteArray bytes;
    if (!m_client.Execute(
            QJsonDocument(request).toJson(QJsonDocument::Compact),
            &bytes,
            error))
    {
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (!document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("包能力返回无效 JSON：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }
    *response = document.object();
    if (!response->value(QStringLiteral("ok")).toBool())
    {
        if (error != nullptr)
        {
            *error = ResultError(
                *response, QStringLiteral("包能力调用失败。"));
        }
        return false;
    }
    return true;
}

bool HostPackageReviewController::LoadVerification(QString* error)
{
    QJsonObject response;
    if (!ExecuteObject(
            QJsonObject{
                {QStringLiteral("capability"),
                 QStringLiteral("package.verify")},
                {QStringLiteral("packageDir"), m_review.packagedirectory}},
            &response,
            error))
    {
        return false;
    }
    m_review.valid = response.value(QStringLiteral("valid")).toBool();
    const QJsonArray errors = response.value(QStringLiteral("errors")).toArray();
    for (const QJsonValue& value : errors)
    {
        const QJsonObject item = value.toObject();
        m_review.verificationerrors.append(
            QStringLiteral("%1：%2")
                .arg(item.value(QStringLiteral("code")).toString(),
                     item.value(QStringLiteral("message")).toString()));
    }
    if (!m_review.valid)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("生产包严格校验失败：%1")
                         .arg(m_review.verificationerrors.join(
                             QStringLiteral("；")));
        }
        return false;
    }
    return true;
}

bool HostPackageReviewController::LoadSummary(QString* error)
{
    QJsonObject response;
    if (!ExecuteObject(
            QJsonObject{
                {QStringLiteral("capability"),
                 QStringLiteral("package.get_summary")},
                {QStringLiteral("packageDir"), m_review.packagedirectory}},
            &response,
            error))
    {
        return false;
    }
    m_review.packageidentity = response.value(
        QStringLiteral("packageIdentity")).toString();
    m_review.schema = response.value(QStringLiteral("schema")).toString();
    m_review.layercount = response.value(QStringLiteral("layerCount")).toInt();
    m_review.bitdepth = response.value(QStringLiteral("bitDepth")).toInt();
    m_review.polarity = response.value(QStringLiteral("polarity")).toString();
    const QJsonObject grid = response.value(QStringLiteral("grid")).toObject();
    m_review.widthpx = grid.value(QStringLiteral("widthPx")).toInt();
    m_review.heightpx = grid.value(QStringLiteral("heightPx")).toInt();
    m_review.dpix = grid.value(QStringLiteral("dpiX")).toInt();
    m_review.dpiy = grid.value(QStringLiteral("dpiY")).toInt();
    const QJsonArray perInstance = response.value(
        QStringLiteral("perInstance")).toArray();
    const QJsonObject profileEcho = response.value(
        QStringLiteral("profileEcho")).toObject();
    m_review.instancecount = perInstance.size();
    m_review.profileversion = profileEcho.value(
        QStringLiteral("profileVersion")).toString();
    m_review.profilehash = profileEcho.value(
        QStringLiteral("profileHash")).toString();
    for (const QJsonValue& channel : response.value(
             QStringLiteral("channels")).toArray())
    {
        m_review.channels.append(channel.toString());
    }
    const bool protocolMatchesChannels =
        (m_review.schema == QStringLiteral("p0.rgbwsv.2")
            && m_review.channels.size() == 6)
        || (m_review.schema == QStringLiteral("p0.rgbwsvt.1")
            && m_review.channels.size() == 7);
    if (!protocolMatchesChannels
        || m_review.bitdepth != 8
        || m_review.polarity != QStringLiteral("black_is_print")
        || !IsFrozenChannelSet(m_review.channels)
        || m_review.layercount <= 0 || m_review.widthpx <= 0
        || m_review.heightpx <= 0 || m_review.instancecount <= 0
        || m_review.profileversion.isEmpty()
        || m_review.profilehash.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "生产包摘要违反冻结协议：schema=%1 bitDepth=%2 polarity=%3 channels=%4")
                         .arg(m_review.schema)
                         .arg(m_review.bitdepth)
                         .arg(m_review.polarity)
                         .arg(m_review.channels.join(QLatin1Char(',')));
        }
        return false;
    }
    return true;
}

bool HostPackageReviewController::LoadLayers(QString* error)
{
    m_review.layers.reserve(m_review.layercount);
    for (int layerIndex = 0; layerIndex < m_review.layercount; ++layerIndex)
    {
        QJsonObject response;
        if (!ExecuteObject(
                QJsonObject{
                    {QStringLiteral("capability"),
                     QStringLiteral("package.get_layer_descriptor")},
                    {QStringLiteral("packageDir"), m_review.packagedirectory},
                    {QStringLiteral("layerIndex"), layerIndex}},
                &response,
                error))
        {
            return false;
        }
        hostlayerdescriptor layer;
        layer.layerindex = response.value(QStringLiteral("layerIndex")).toInt(-1);
        layer.zmm = response.value(QStringLiteral("zMm")).toDouble();
        layer.widthpx = response.value(QStringLiteral("widthPx")).toInt();
        layer.heightpx = response.value(QStringLiteral("heightPx")).toInt();
        layer.storagemode = response.value(
            QStringLiteral("storageMode")).toString();
        layer.tiffpath = response.value(QStringLiteral("tiffPath")).toString();
        if (layer.layerindex != layerIndex || layer.widthpx != m_review.widthpx
            || layer.heightpx != m_review.heightpx
            || !ReadChannelCounts(
                response.value(QStringLiteral("printPixels")).toObject(),
                m_review.channels,
                &layer.printpixels)
            || !ReadChannelCounts(
                response.value(QStringLiteral("emptyPixels")).toObject(),
                m_review.channels,
                &layer.emptypixels))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("第 %1 层描述不完整或与摘要不一致。")
                             .arg(layerIndex);
            }
            return false;
        }
        m_review.layers.append(layer);
    }
    return true;
}
