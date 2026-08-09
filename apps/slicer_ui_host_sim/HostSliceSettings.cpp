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
    case HostMaterialStrategy::RgbWhite:
        return HOST_MATERIAL_RGB_WHITE;
    case HostMaterialStrategy::RgbVarnish:
        return HOST_MATERIAL_RGB_VARNISH;
    case HostMaterialStrategy::RgbWhiteVarnish:
        return HOST_MATERIAL_RGB_WHITE_VARNISH;
    case HostMaterialStrategy::WhiteSolid:
        return HOST_MATERIAL_WHITE_SOLID;
    case HostMaterialStrategy::VarnishSolid:
        return HOST_MATERIAL_VARNISH_SOLID;
    }
    return static_cast<enum hostmaterialstrategy>(-1);
}

enum hostmaterialrole ToHostMaterialRole(const HostMaterialRole role)
{
    switch (role)
    {
    case HostMaterialRole::Rgb:
        return HOST_MATERIAL_ROLE_RGB;
    case HostMaterialRole::White:
        return HOST_MATERIAL_ROLE_WHITE;
    case HostMaterialRole::Varnish:
        return HOST_MATERIAL_ROLE_VARNISH;
    case HostMaterialRole::Ignore:
        return HOST_MATERIAL_ROLE_IGNORE;
    case HostMaterialRole::SupportCandidate:
        return HOST_MATERIAL_ROLE_SUPPORT_CANDIDATE;
    }
    return static_cast<enum hostmaterialrole>(-1);
}

enum hostsupportmode ToHostSupportMode(const HostSupportMode mode)
{
    switch (mode)
    {
    case HostSupportMode::None:
        return HOST_SUPPORT_NONE;
    case HostSupportMode::BottomProjection:
        return HOST_SUPPORT_BOTTOM_PROJECTION;
    case HostSupportMode::UnsupportedOnly:
        return HOST_SUPPORT_UNSUPPORTED_ONLY;
    case HostSupportMode::BottomProjectionPlusUnsupported:
        return HOST_SUPPORT_BOTTOM_PLUS_UNSUPPORTED;
    case HostSupportMode::FullVerticalProjection:
        return HOST_SUPPORT_FULL_VERTICAL_PROJECTION;
    }
    return static_cast<enum hostsupportmode>(-1);
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
            && format != QStringLiteral("3mf")
            && format != QStringLiteral("stl"))
        || model.suffix().toLower() != format)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "有效 Profile 需要已导入的 OBJ、3MF 或 STL 模型。");
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
    const hostmaterialprocesssettings& material = settings.materialprocess;
    if (MaterialRoleId(material.defaultrole) == QStringLiteral("unknown")
        || material.whiteexpandpx < 0
        || material.whiteexpandpx > 100000
        || material.whiteshrinkpx < 0
        || material.whiteshrinkpx > 100000
        || material.varnishtoplayers <= 0
        || material.varnishtoplayers > 100000
        || material.maxunexpectedoverlappixels < 0
        || material.maxunexpectedoverlappixels > 1000000)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "材料工艺参数越界：白墨扩缩 0..100000 px，光油层 1..100000，重叠像素 0..1000000。");
        }
        return false;
    }
    const hostsupportsettings& support = settings.support;
    const QString supportMode = SupportModeId(support.mode);
    if (supportMode == QStringLiteral("unknown")
        || (support.enabled && support.mode == HostSupportMode::None)
        || (!support.enabled && support.mode != HostSupportMode::None))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "支撑开关与支撑模式不一致，关闭时必须使用 none。");
        }
        return false;
    }
    if (!std::isfinite(support.offsetmm)
        || support.offsetmm < 0.0 || support.offsetmm > 10.0
        || support.minareapx < 0 || support.minareapx > 1000000
        || support.internalvoid.minareapx < 0
        || support.internalvoid.minareapx > 1000000
        || support.baseprojection.layercount < 0
        || support.baseprojection.layercount > 1000)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "支撑参数越界：外扩 0..10 mm，面积 0..1000000 px，铺底 0..1000 层。");
        }
        return false;
    }
    if (!support.enabled
        && (support.internalvoid.enabled || support.baseprojection.enabled))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "关闭支撑时不能启用内部镂空或投影铺底。");
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
        ToHostMaterialStrategy(settings.materialstrategy),
        settings.materialprocess.rolemappingenabled ? 1 : 0,
        ToHostMaterialRole(settings.materialprocess.defaultrole),
        settings.materialprocess.mapwhitenames ? 1 : 0,
        settings.materialprocess.mapvarnishnames ? 1 : 0,
        settings.materialprocess.allowinputsupportmaterial ? 1 : 0,
        settings.materialprocess.whiteexpandpx,
        settings.materialprocess.whiteshrinkpx,
        settings.materialprocess.varnishtoplayers,
        settings.materialprocess.maxunexpectedoverlappixels,
        settings.support.enabled ? 1 : 0,
        ToHostSupportMode(settings.support.mode),
        settings.support.offsetmm,
        settings.support.minareapx,
        settings.support.internalvoid.enabled ? 1 : 0,
        settings.support.internalvoid.minareapx,
        settings.support.baseprojection.enabled ? 1 : 0,
        settings.support.baseprojection.layercount};
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
    case HostMaterialStrategy::RgbWhite:
        return QStringLiteral("rgb_white");
    case HostMaterialStrategy::RgbVarnish:
        return QStringLiteral("rgb_varnish");
    case HostMaterialStrategy::RgbWhiteVarnish:
        return QStringLiteral("rgb_white_varnish");
    case HostMaterialStrategy::WhiteSolid:
        return QStringLiteral("white_solid");
    case HostMaterialStrategy::VarnishSolid:
        return QStringLiteral("varnish_solid");
    }
    return QStringLiteral("unknown");
}

QString HostEffectiveProfileBuilder::MaterialRoleId(
    const HostMaterialRole role)
{
    switch (role)
    {
    case HostMaterialRole::Rgb:
        return QStringLiteral("rgb");
    case HostMaterialRole::White:
        return QStringLiteral("white");
    case HostMaterialRole::Varnish:
        return QStringLiteral("varnish");
    case HostMaterialRole::Ignore:
        return QStringLiteral("ignore");
    case HostMaterialRole::SupportCandidate:
        return QStringLiteral("support_candidate");
    }
    return QStringLiteral("unknown");
}

QString HostEffectiveProfileBuilder::SupportModeId(
    const HostSupportMode mode)
{
    switch (mode)
    {
    case HostSupportMode::None:
        return QStringLiteral("none");
    case HostSupportMode::BottomProjection:
        return QStringLiteral("bottom_projection");
    case HostSupportMode::UnsupportedOnly:
        return QStringLiteral("unsupported_only");
    case HostSupportMode::BottomProjectionPlusUnsupported:
        return QStringLiteral("bottom_projection_plus_unsupported");
    case HostSupportMode::FullVerticalProjection:
        return QStringLiteral("full_vertical_projection");
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
