#include "HostSliceSettings.h"

#include "HostRequestBuilder.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>

#include <cmath>
#include <cstdlib>

namespace
{
bool IsFinitePositive(const double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool IsWritableOutput(const QString& outputDirectory)
{
    QFileInfo outputInfo(outputDirectory);
    QString candidate = outputInfo.absoluteFilePath();
    while (!candidate.isEmpty() && !QFileInfo(candidate).exists())
    {
        const QString parent = QFileInfo(candidate).absolutePath();
        if (parent == candidate)
        {
            return false;
        }
        candidate = parent;
    }
    const QFileInfo existing(candidate);
    return existing.isDir() && existing.isWritable();
}

enum hostmaterialstrategy ToHostMaterialStrategy(
    const HostMaterialStrategy strategy)
{
    switch (strategy)
    {
    case HostMaterialStrategy::RgbSolid:
        return HOST_MATERIAL_RGB_SOLID;
    case HostMaterialStrategy::WhiteSolid:
        return HOST_MATERIAL_WHITE_SOLID;
    case HostMaterialStrategy::VarnishSolid:
        return HOST_MATERIAL_VARNISH_SOLID;
    }
    return static_cast<enum hostmaterialstrategy>(-1);
}
}

bool HostEffectiveProfileBuilder::Validate(
    const hostslicesettings& settings,
    QString* error)
{
    const QFileInfo model(settings.modelpath);
    const QString format = settings.modelformat.trimmed().toLower();
    if (settings.profileid.trimmed().isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("请选择可用的宿主 Profile。");
        }
        return false;
    }
    if (!model.isFile()
        || (format != QStringLiteral("obj")
            && format != QStringLiteral("3mf"))
        || model.suffix().toLower() != format)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("有效 Profile 需要已导入的 OBJ 或 3MF 模型。");
        }
        return false;
    }
    if (settings.dpix < 72 || settings.dpix > 2400
        || settings.dpiy < 72 || settings.dpiy > 2400)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("X/Y DPI 必须位于 72..2400。");
        }
        return false;
    }
    if (!IsFinitePositive(settings.layerthicknessmm)
        || settings.layerthicknessmm > 10.0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("层厚必须大于 0 且不超过 10 mm。");
        }
        return false;
    }

    const QFileInfo outputInfo(settings.outputdirectory);
    const QString absoluteOutput = QDir::cleanPath(
        outputInfo.absoluteFilePath());
    const QString rootPath = QDir::cleanPath(
        QDir(absoluteOutput).rootPath());
    if (!outputInfo.isAbsolute() || absoluteOutput == rootPath
        || !IsWritableOutput(absoluteOutput))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "输出目录必须是非磁盘根目录的绝对可写路径。");
        }
        return false;
    }
    if (!IsFinitePositive(settings.buildvolume.widthmm)
        || !IsFinitePositive(settings.buildvolume.heightmm)
        || !IsFinitePositive(settings.buildvolume.zlimitmm)
        || settings.buildvolume.origin != QStringLiteral("lower_left")
        || settings.buildvolume.xdirection != QStringLiteral("positive")
        || settings.buildvolume.ydirection != QStringLiteral("positive"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "设备 buildVolume 必须为正数并使用 lower_left/positive 轴向。");
        }
        return false;
    }
    if (MaterialStrategyId(settings.materialstrategy)
        == QStringLiteral("unknown"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模型材料策略不受参考宿主支持。");
        }
        return false;
    }
    return true;
}

bool HostEffectiveProfileBuilder::Build(
    const hostslicesettings& settings,
    hosteffectiveprofile* effectiveProfile,
    QString* error)
{
    if (effectiveProfile == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("有效 Profile 输出目标不能为空。");
        }
        return false;
    }
    *effectiveProfile = {};
    if (!Validate(settings, error))
    {
        return false;
    }

    const QByteArray modelPath = QDir::fromNativeSeparators(
        QFileInfo(settings.modelpath).absoluteFilePath()).toUtf8();
    const QByteArray format = settings.modelformat.toLower().toUtf8();
    const QByteArray outputDirectory = QDir::fromNativeSeparators(
        QFileInfo(settings.outputdirectory).absoluteFilePath()).toUtf8();
    const QByteArray profileId = settings.profileid.toUtf8();
    const struct hosteffectiveprofilesettings requestSettings{
        modelPath.constData(),
        format.constData(),
        outputDirectory.constData(),
        profileId.constData(),
        settings.dpix,
        settings.dpiy,
        settings.layerthicknessmm,
        ToHostMaterialStrategy(settings.materialstrategy)};
    char profileHash[72] = {};
    char* profileText = HostBuildEffectiveProfile(
        &requestSettings, profileHash, sizeof(profileHash));
    if (profileText == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("宿主有效 Profile 构造失败。");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray(profileText), &parseError);
    std::free(profileText);
    if (!document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("有效 Profile JSON 无效：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }
    effectiveProfile->profile = document.object();
    effectiveProfile->profilehash = QString::fromLatin1(profileHash);
    if (effectiveProfile->profile.value(
            QStringLiteral("profileHash")).toString()
        != effectiveProfile->profilehash)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("有效 Profile hash 写回不一致。");
        }
        *effectiveProfile = {};
        return false;
    }
    return true;
}

QString HostEffectiveProfileBuilder::MaterialStrategyId(
    const HostMaterialStrategy strategy)
{
    switch (strategy)
    {
    case HostMaterialStrategy::RgbSolid:
        return QStringLiteral("rgb_solid");
    case HostMaterialStrategy::WhiteSolid:
        return QStringLiteral("white_solid");
    case HostMaterialStrategy::VarnishSolid:
        return QStringLiteral("varnish_solid");
    }
    return QStringLiteral("unknown");
}

bool HostEffectiveProfileBuilder::BuildVolumesEqual(
    const hostbuildvolume& left,
    const hostbuildvolume& right)
{
    constexpr double epsilon = 1.0e-9;
    return std::abs(left.widthmm - right.widthmm) <= epsilon
        && std::abs(left.heightmm - right.heightmm) <= epsilon
        && std::abs(left.zlimitmm - right.zlimitmm) <= epsilon
        && left.origin == right.origin
        && left.xdirection == right.xdirection
        && left.ydirection == right.ydirection;
}
