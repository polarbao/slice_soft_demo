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
    // 原实现归一化的是 model.path，但宿主 Profile 并无 model 键——真实路径位于
    // input.modelPath。结果是：它插入了一个本不存在的键，却把【工作树绝对路径】
    // 留在了哈希里，使同一份逻辑在不同工作树上得出不同哈希
    // （例如 slice_soft_demo 与 slice_soft_demo_matvol_t），
    // 于是本用例钉住的是环境而不是内容。此处按实际键名归一化。
    QJsonObject input = profile.value(QStringLiteral("input")).toObject();
    input.insert(QStringLiteral("modelPath"), QStringLiteral("/fixture/model.obj"));
    profile.insert(QStringLiteral("input"), input);
    QJsonObject output = profile.value(QStringLiteral("output")).toObject();
    output.insert(QStringLiteral("packageDir"), QStringLiteral("/fixture/package"));
    profile.insert(QStringLiteral("output"), output);
    // profileHash 由归一化【之前】的内容算出，仍随路径变化，必须一并剔除。
    profile.remove(QStringLiteral("profileHash"));
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
        {"nail_rgb_white_varnish_top1.json", "067fb78a5b644e2b0575d7f076a0f00ac3e3ce94b733341bf146a3f3a4715ddc"},
        {"nail_rgb_white_varnish_top2_regression.json", "397a4c61abdce19f060d068eb73c3e3eb83f86c3e1891c22a464d07e8260f9a5"},
        {"nail_rgb_white_varnish_top2.json", "0d2f4212e01649bbbe4ae4e5e6e1dde82fdea8da31bd63c39c661af9fc8a9a21"},
        {"nail_rgb_white_varnish_top3.json", "b4341323161fde164ff4bfbb99003892b8299766db10d6efeb8540264083376e"},
        {"nail_varnish_only.json", "e639af09651d7e249451b995ee994e36a2dd7a4f58d77ad3c3ef39a167c3dda5"},
        {"nail_white_underbase_only.json", "b2efa279494db3634220f32676dbd75aa3ca5b0243e2dee0b3d90b6d56340c26"},
        {"obj_mtl_texture_rgb_only.json", "1c6ff4b0ab0797d3e6c3062a8681910042297fa692fe31b5d1be25e9ffa71db1"},
        {"obj_mtl_texture_rgb_varnish.json", "bae4a489a43d7d66b11cc324fbd4344c790e709686fbf84100b05b86671623fa"},
        {"obj_mtl_texture_rgb_white_ondemand.json", "fbe1fbcfc3513ecd0e55f49183b145e81fc7784a1758ebc8db8dfeee0ec14324"},
        {"obj_mtl_texture_rgb_white_varnish_regression.json", "3cb9d9c4e3f22e8c9e245b5979268d03176e30f5b8aa2ea9ca60d046a731c63d"},
        {"obj_mtl_texture_rgb_white_varnish.json", "28ca280816a00ed904f6857a222a0a9c6acd9cf6d2a7dd81a74c45533271152d"},
        {"stage15_f01_xiaoma_white_carrier.json", "71f5c2952a0f9c4a166eb72c9fe8f8cdbb9400a0742a1c3de4aa9d36a947b825"},
        {"stage15_f03_four_value.json", "fb840260e851bfb8694b71319080143b1c657511bb8dec3945edfd6792c3b797"},
        {"stage15_f04_all_white.json", "299a49fb2807b63daed194a9b5acbebd6313348340663f6013e8c665aa1b506f"},
        {"three_mf_texture_rgb_white_varnish.json", "a8bee735d8948fd56d9812e7752c977b3757857a7bb9692ce86badb7d65a9ff9"}};
    const QDir directory(QDir(repositoryRoot).filePath(
        QStringLiteral("samples/configs/material_process")));
    for (const baseline& expected : baselines)
    {
        QFile file(directory.filePath(QString::fromLatin1(expected.fileName)));
        if (!file.open(QIODevice::ReadOnly)
            || QCryptographicHash::hash(
                   // 先把 CRLF 归一为 LF 再哈希。否则本用例守的是【行尾】而不是内容：
                   // 同一份文件在 CRLF 检出的工作树与 LF 检出的工作树上哈希不同，
                   // 会在一处 PASS、另一处以「旧工艺 SHA256 漂移」误报。
                   file.readAll().replace("\r\n", "\n"),
                   QCryptographicHash::Sha256)
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
    // 原为六条件复合断言、共用一句错误消息，失败时无法判断是哪一条。
    // 拆开逐条断言：诊断信息是用例的一部分，不是可选项。
    const bool builtLegacy =
        HostEffectiveProfileBuilder::Build(legacy, &legacyEffective, &error);
    const QString actualLegacyHash = NormalizedLegacyHash(legacyEffective.profile);
    const bool allLegacyChecks =
        Check(transferPresetCount == 3,
              QStringLiteral("新版 Host 传输预设数应为 3，实为 %1").arg(transferPresetCount),
              errors)
        && Check(builtLegacy,
                 QStringLiteral("旧 Profile 构建失败：%1").arg(error),
                 errors)
        && Check(!legacyEffective.profile.value(QStringLiteral("output"))
                      .toObject().contains(QStringLiteral("packageProtocol")),
                 QStringLiteral("旧 Profile 不应带 output.packageProtocol"),
                 errors)
        && Check(!legacyEffective.profile.contains(
                     QStringLiteral("transferChannelPolicy")),
                 QStringLiteral("旧 Profile 不应带 transferChannelPolicy"),
                 errors)
        && Check(actualLegacyHash
                     == QStringLiteral("sha256:322d63557dedef5952f5a7148ad7a102"
                                       "4c8fc225d081477ad069a8a0f44e502f"),
                 QStringLiteral("旧 Profile 归一化 hash 漂移，实为 %1")
                     .arg(actualLegacyHash),
                 errors)
        && Check(WorkerHash(legacyEffective.profile) == legacyEffective.profilehash,
                 QStringLiteral("旧 Profile 的 WorkerHash 与 profilehash 不一致"),
                 errors);
    if (!allLegacyChecks)
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
