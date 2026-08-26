#include "apps/slicer_ui_host_sim/HostProcessPresetCatalog.h"
#include "apps/slicer_ui_host_sim/HostProfileCatalog.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/json_value.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

#include <sstream>

namespace
{
QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

QString WorkerHash(const QJsonObject& profile)
{
    const QByteArray serialized = QJsonDocument(profile).toJson(
        QJsonDocument::Compact);
    std::istringstream input(serialized.toStdString());
    return QString::fromStdString(
        slicer_core::api::ComputeProfileDocumentHash(
            slicer_core::Json::parse(input)));
}

QString NormalizedLegacyHash(QJsonObject profile)
{
    QJsonObject model = profile.value(QStringLiteral("model")).toObject();
    model.insert(QStringLiteral("path"), QStringLiteral("/fixture/model.obj"));
    profile.insert(QStringLiteral("model"), model);
    QJsonObject output = profile.value(QStringLiteral("output")).toObject();
    output.insert(QStringLiteral("packageDir"), QStringLiteral("/fixture/package"));
    profile.insert(QStringLiteral("output"), output);
    return WorkerHash(profile);
}

bool Check(const bool condition, const QString& message, QTextStream& errors)
{
    if (!condition)
    {
        errors << message << Qt::endl;
    }
    return condition;
}

bool VerifyLegacyProcessProfileHashes(
    const QString& repositoryRoot,
    QTextStream& errors)
{
    struct baseline
    {
        const char* fileName;
        const char* sha256;
    };
    static const baseline baselines[]{
        {"nail_rgb_white_varnish_top1.json", "6621d8ed691876321eed80c0ec4972162d7a3fddad82eda54b3133ab8482ebad"},
        {"nail_rgb_white_varnish_top2_regression.json", "d66defe66a224d38f5a37212c8c201091cc08aa0fae8fffe483439b62453be10"},
        {"nail_rgb_white_varnish_top2.json", "eac41f3488849db6a16cbe6c32708142353a3b2b8f984d814e04dd003cc84658"},
        {"nail_rgb_white_varnish_top3.json", "9f671b6ad328d1e30791b00ebe1f50b9ae2f9e9f0fe630142d478e601261c2a7"},
        {"nail_varnish_only.json", "97b1e9b67e6fa5cd508a45353f5840342386562ed3c7224e71e7a0ff2b8d9bea"},
        {"nail_white_underbase_only.json", "eb73d826d5af4b96d95ae3fc91ee993f2668f69816f8915d2bfd6b28999e2804"},
        {"obj_mtl_texture_rgb_only.json", "5e1ac3725aebd3d6564de0137e6496fc7da143f3dc746fe9a4b500ec29698595"},
        {"obj_mtl_texture_rgb_varnish.json", "ae8ede31c52d8bfc184732e35df16862955e341b5299cb0246d94034116133f8"},
        {"obj_mtl_texture_rgb_white_ondemand.json", "1c7fa36e1983b4881cb6bf55e15c52ef2c44f4732189e7f379b9a88e3d79547f"},
        {"obj_mtl_texture_rgb_white_varnish_regression.json", "595165c00a404867e989814dee7071556a8beb590b80da7e1417ac34f3757f5e"},
        {"obj_mtl_texture_rgb_white_varnish.json", "024076e66483b44b3e265e65b0b1243d3bf776fbce7e52831af7e678c8ead0d2"},
        {"stage15_f01_xiaoma_white_carrier.json", "888502a6f7c106a44ca6c0e4d908a58572a0944feabefa00383157b392d332f8"},
        {"stage15_f03_four_value.json", "4308277d6c38fe06cb19630b91638603d36c99dbfef4f8c18c1c1abf159b3c08"},
        {"stage15_f04_all_white.json", "0f7060258f157538d89d95af08e55e672d88e58d041887bb31a5bd9c4c8ac25d"},
        {"three_mf_texture_rgb_white_varnish.json", "49941594f210cc3ab53f4c12d085b262da289e0762d2d1e376d24daba6fc072b"}};
    const QDir directory(QDir(repositoryRoot).filePath(
        QStringLiteral("samples/configs/material_process")));
    for (const baseline& expected : baselines)
    {
        QFile file(directory.filePath(QString::fromLatin1(expected.fileName)));
        if (!file.open(QIODevice::ReadOnly)
            || QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256)
                    .toHex() != QByteArray(expected.sha256))
        {
            return Check(false, QStringLiteral("旧工艺 SHA256 漂移：%1")
                                    .arg(QString::fromLatin1(expected.fileName)),
                         errors);
        }
    }
    return true;
}

bool VerifyTransferProfileOutputDirectories(
    const QString& repositoryRoot,
    QTextStream& errors)
{
    const QDir directory(QDir(repositoryRoot).filePath(
        QStringLiteral("samples/configs/matvol_t/process_profiles")));
    const QStringList files = directory.entryList(
        QStringList{QStringLiteral("*_rgbwsvt.json")}, QDir::Files, QDir::Name);
    if (!Check(files.size() == 10,
               QStringLiteral("RGBWSVT 工艺全集数量必须为 10。"), errors))
    {
        return false;
    }
    for (const QString& fileName : files)
    {
        QFile file(directory.filePath(fileName));
        const QJsonObject output = file.open(QIODevice::ReadOnly)
            ? QJsonDocument::fromJson(file.readAll()).object()
                  .value(QStringLiteral("output")).toObject()
            : QJsonObject{};
        const QString packageDirectory = output.value(
            QStringLiteral("packageDir")).toString();
        if (!Check(!packageDirectory.isEmpty()
                       && !QDir::isAbsolutePath(packageDirectory)
                       && !packageDirectory.contains(QStringLiteral("..")),
                   QStringLiteral("RGBWSVT 工艺输出目录必须安全且相对：%1")
                       .arg(fileName), errors))
        {
            return false;
        }
    }
    return true;
}

hostslicesettings MakeSettings(
    const QString& modelPath,
    const QString& outputDirectory)
{
    hostslicesettings settings;
    settings.profileid = QStringLiteral("host-reference-transfer-channel");
    settings.modelpath = modelPath;
    settings.modelformat = QStringLiteral("obj");
    settings.outputdirectory = outputDirectory;
    return settings;
}

bool VerifyTransferProfileProductionSafety(QTextStream& errors)
{
    const QList<hostprofiledescriptor> profiles =
        ReferenceHostProfileCatalog{}.Profiles();
    for (const hostprofiledescriptor& profile : profiles)
    {
        if (profile.profileid
            == QStringLiteral("host-reference-transfer-channel"))
        {
            return Check(
                profile.productionsafety == QStringLiteral("production"),
                QStringLiteral("RGBWSVT 显式 Profile 必须标记为 production。"),
                errors);
        }
    }
    return Check(
        false,
        QStringLiteral("RGBWSVT 显式 Profile 不存在。"),
        errors);
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTextStream errors(stderr);
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    const QString modelPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/openvdb/surface_shell_cube_no_uv.obj"));
    QTemporaryDir outputRoot;
    if (!VerifyLegacyProcessProfileHashes(repositoryRoot, errors)
        || !VerifyTransferProfileOutputDirectories(repositoryRoot, errors)
        || !VerifyTransferProfileProductionSafety(errors)
        || !Check(QFileInfo(modelPath).isFile(),
               QStringLiteral("MATVOL-T Host 模型 fixture 不存在。"), errors)
        || !Check(outputRoot.isValid(),
                  QStringLiteral("MATVOL-T Host 临时目录不可用。"), errors))
    {
        return 2;
    }

    int transferPresetCount = 0;
    for (const hostprocesspreset& preset : HostProcessPresetCatalog::Presets())
    {
        if (preset.packageprotocol != HostPackageProtocol::Rgbwsvt)
        {
            continue;
        }
        ++transferPresetCount;
        hostslicesettings settings = MakeSettings(
            modelPath, QDir(outputRoot.path()).filePath(preset.id));
        settings.materialstrategy = preset.materialstrategy;
        settings.materialprocess = preset.materialprocess;
        settings.texture = preset.texture;
        settings.support = preset.support;
        settings.materialvolume = preset.materialvolume;
        settings.packageprotocol = preset.packageprotocol;
        settings.transferchannel = preset.transferchannel;
        hosteffectiveprofile effective;
        QString error;
        if (!Check(HostEffectiveProfileBuilder::Build(
                       settings, &effective, &error),
                   QStringLiteral("RGBWSVT 预设 %1 构造失败：%2")
                       .arg(preset.id, error),
                   errors))
        {
            return 3;
        }
        const QJsonObject output = effective.profile.value(
            QStringLiteral("output")).toObject();
        const QJsonObject transfer = effective.profile.value(
            QStringLiteral("transferChannelPolicy")).toObject();
        if (!Check(
                output.value(QStringLiteral("packageProtocol")).toString()
                        == QStringLiteral("p0.rgbwsvt.1")
                    && output.value(QStringLiteral("channelOrder")).toArray()
                        == QJsonArray{
                            QStringLiteral("R"), QStringLiteral("G"),
                            QStringLiteral("B"), QStringLiteral("W"),
                            QStringLiteral("S"), QStringLiteral("V"),
                            QStringLiteral("T")}
                    && transfer.value(QStringLiteral("enabled")).toBool()
                    && transfer.value(QStringLiteral(
                        "materialDiffuseRgbValues")).toArray().size()
                        == preset.transferchannel.materialdiffusergbvalues.size()
                    && WorkerHash(effective.profile) == effective.profilehash,
                QStringLiteral("RGBWSVT 预设 %1 协议、策略或 hash 未闭合。")
                    .arg(preset.id),
                errors))
        {
            return 4;
        }
    }

    hostslicesettings legacy;
    legacy.profileid = QStringLiteral("host-reference-default");
    legacy.modelpath = modelPath;
    legacy.modelformat = QStringLiteral("obj");
    legacy.outputdirectory = QDir(outputRoot.path()).filePath(
        QStringLiteral("legacy"));
    hosteffectiveprofile legacyEffective;
    QString error;
    if (!Check(
            transferPresetCount == 3
                && HostEffectiveProfileBuilder::Build(
                    legacy, &legacyEffective, &error)
                && !legacyEffective.profile.value(
                    QStringLiteral("output")).toObject().contains(
                        QStringLiteral("packageProtocol"))
                && !legacyEffective.profile.contains(
                    QStringLiteral("transferChannelPolicy"))
                && NormalizedLegacyHash(legacyEffective.profile)
                    == QStringLiteral(
                        "sha256:f184100f2cd6dd4a3dc4bfcd9e22ee67c6fc056a03133cfe153752acea5b7f32")
                && WorkerHash(legacyEffective.profile) == legacyEffective.profilehash,
            QStringLiteral("旧 Profile/hash 漂移或新版 Host 预设不完整。"),
            errors))
    {
        return 5;
    }
    QTextStream(stdout)
        << "MATVOL_T_HOST_PROFILE_PASS legacy=unchanged transferPresets="
        << transferPresetCount << " legacyHash="
        << legacyEffective.profilehash << " normalizedLegacyHash="
        << NormalizedLegacyHash(legacyEffective.profile) << Qt::endl;
    return 0;
}
