#include "HostTransferProcessPresetLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStringList>

namespace
{
QString ProcessProfileDirectory()
{
    const QString overrideDirectory = qEnvironmentVariable(
        "SLICESOFT_PROCESS_PROFILE_DIR").trimmed();
    return overrideDirectory.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("configs/material_process"))
        : overrideDirectory;
}

bool IsExactTransferOrder(const QJsonArray& order)
{
    static const QStringList expected{
        QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"),
        QStringLiteral("W"), QStringLiteral("S"), QStringLiteral("V"),
        QStringLiteral("T")};
    if (order.size() != expected.size())
    {
        return false;
    }
    for (int index = 0; index < order.size(); ++index)
    {
        if (order.at(index).toString() != expected.at(index))
        {
            return false;
        }
    }
    return true;
}

bool ReadRgbValues(
    const QJsonArray& values,
    QVector<hostrgbcolor>* colours)
{
    QSet<QString> unique;
    for (const QJsonValue& value : values)
    {
        const QJsonArray channels = value.toArray();
        if (!value.isArray() || channels.size() != 3)
        {
            return false;
        }
        hostrgbcolor colour{
            channels.at(0).toInt(-1),
            channels.at(1).toInt(-1),
            channels.at(2).toInt(-1)};
        if (colour.red < 0 || colour.red > 255
            || colour.green < 0 || colour.green > 255
            || colour.blue < 0 || colour.blue > 255)
        {
            return false;
        }
        const QString key = QStringLiteral("%1,%2,%3")
            .arg(colour.red).arg(colour.green).arg(colour.blue);
        if (unique.contains(key))
        {
            return false;
        }
        unique.insert(key);
        colours->push_back(colour);
    }
    return !colours->isEmpty();
}
}

bool HostTransferProcessPresetLoader::HasAnyDeployedProfile(QString* error)
{
    const QStringList files = QDir(ProcessProfileDirectory()).entryList(
        QStringList{QStringLiteral("*_rgbwsvt.json")}, QDir::Files, QDir::Name);
    for (const QString& fileName : files)
    {
        hosttransferchannelsettings transfer;
        if (Load(fileName, &transfer, nullptr))
        {
            return true;
        }
    }
    if (error != nullptr)
    {
        *error = QStringLiteral("部署目录中没有有效的 RGBWSVT 工艺配置。");
    }
    return false;
}

bool HostTransferProcessPresetLoader::Load(
    const QString& fileName,
    hosttransferchannelsettings* transferSettings,
    QString* error)
{
    if (transferSettings == nullptr || QFileInfo(fileName).fileName() != fileName)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RGBWSVT 工艺文件名或输出目标无效。");
        }
        return false;
    }
    QFile file(QDir(ProcessProfileDirectory()).filePath(fileName));
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法读取 RGBWSVT 工艺：%1")
                .arg(QDir::toNativeSeparators(file.fileName()));
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    const QJsonObject root = document.object();
    const QJsonObject output = root.value(QStringLiteral("output")).toObject();
    const QJsonObject policy = root.value(
        QStringLiteral("transferChannelPolicy")).toObject();
    const QJsonObject topology = policy.value(
        QStringLiteral("topology")).toObject();
    hosttransferchannelsettings loaded;
    loaded.enabled = policy.value(QStringLiteral("enabled")).toBool(false);
    loaded.matchsource = policy.value(QStringLiteral("matchSource")).toString();
    loaded.missingregion = policy.value(QStringLiteral("missingRegion")).toString();
    loaded.multiplematches = policy.value(
        QStringLiteral("multipleMatches")).toString();
    loaded.value = policy.value(QStringLiteral("value")).toInt(-1);
    loaded.selfintersectionpolicy = topology.value(
        QStringLiteral("selfIntersectionPolicy")).toString();
    loaded.maxselfintersectionpairs = topology.value(
        QStringLiteral("maxSelfIntersectionPairs")).toInt(-1);
    const bool valid = document.isObject()
        && output.value(QStringLiteral("packageProtocol")).toString()
            == QStringLiteral("p0.rgbwsvt.1")
        && IsExactTransferOrder(output.value(
            QStringLiteral("channelOrder")).toArray())
        && loaded.enabled
        && loaded.matchsource == QStringLiteral("material_diffuse_rgb")
        && ReadRgbValues(policy.value(
            QStringLiteral("materialDiffuseRgbValues")).toArray(),
            &loaded.materialdiffusergbvalues)
        && (loaded.missingregion == QStringLiteral("allow_empty")
            || loaded.missingregion == QStringLiteral("fail_closed"))
        && loaded.multiplematches == QStringLiteral("fail_closed")
        && loaded.value == 0
        && (loaded.selfintersectionpolicy == QStringLiteral("reject")
            || loaded.selfintersectionpolicy
                == QStringLiteral("tolerate_closed_self_intersection"))
        && loaded.maxselfintersectionpairs > 0;
    if (!valid)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RGBWSVT 工艺协议或 T 策略无效：%1 (%2)")
                .arg(fileName, parseError.errorString());
        }
        return false;
    }
    *transferSettings = loaded;
    return true;
}
