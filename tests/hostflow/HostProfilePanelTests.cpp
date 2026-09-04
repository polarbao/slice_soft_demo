#include "apps/slicer_ui_host_sim/HostProfileCatalog.h"
#include "apps/slicer_ui_host_sim/HostProfilePanel.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"

#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>
#include <utility>

namespace
{
bool Check(const bool condition, const QString& message, QTextStream& errors)
{
    if (!condition)
    {
        errors << message << Qt::endl;
    }
    return condition;
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

class StaticProfileCatalog final : public IHostProfileCatalog
{
public:
    explicit StaticProfileCatalog(QList<hostprofiledescriptor> profiles)
        : m_profiles(std::move(profiles))
    {
    }

    QList<hostprofiledescriptor> Profiles() const override
    {
        return m_profiles;
    }

private:
    QList<hostprofiledescriptor> m_profiles;
};

hostprofiledescriptor MakeProfile(
    const QString& profileId,
    const QString& safety,
    const QStringList& requirements)
{
    return hostprofiledescriptor{
        profileId,
        QStringLiteral("测试 Profile"),
        QStringLiteral("HOSTFLOW H-B-04 测试目录项"),
        safety,
        {QStringLiteral("测试")},
        requirements,
        QStringLiteral("测试用途"),
        QStringLiteral("测试默认工艺"),
        QStringLiteral("测试输出合同"),
        QStringLiteral("测试限制")};
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QTextStream errors(stderr);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    if (!Check(
            QFileInfo(modulePath).isFile(),
            QStringLiteral("slicer_module.dll 不存在。"),
            errors))
    {
        return 2;
    }

    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        errors << "模块加载失败：" << error << Qt::endl;
        return 3;
    }

    ReferenceHostProfileCatalog referenceCatalog;
    hostprofilecatalogresolution resolution;
    if (!Check(
            HostProfileCapabilityResolver::Resolve(
                referenceCatalog,
                client.ModuleInfo(),
                &resolution,
                &error),
            QStringLiteral("参考 Profile 解析失败：%1").arg(error),
            errors)
        || !Check(
            resolution.modulecapabilities.size() == 16,
            QStringLiteral("模块能力必须结构化解析为冻结的 16 项。"),
            errors)
        || !Check(
            resolution.profiles.size() == 4,
            QStringLiteral("参考宿主应提供四种 Profile。"),
            errors))
    {
        return 4;
    }
    for (const hostprofileavailability& profile : resolution.profiles)
    {
        if (!Check(
                profile.available,
                QStringLiteral("参考 Profile 应被当前模块能力满足：%1")
                    .arg(profile.profile.profileid),
                errors))
        {
            return 5;
        }
    }

    QTemporaryDir emptyProcessProfileDirectory;
    const QByteArray previousProcessProfileDirectory = qgetenv(
        "SLICESOFT_PROCESS_PROFILE_DIR");
    qputenv("SLICESOFT_PROCESS_PROFILE_DIR",
            emptyProcessProfileDirectory.path().toUtf8());
    hostprofilecatalogresolution missingProcessProfileResolution;
    const bool missingProcessProfileResolved =
        HostProfileCapabilityResolver::Resolve(
            referenceCatalog, client.ModuleInfo(),
            &missingProcessProfileResolution, &error);
    if (previousProcessProfileDirectory.isNull())
    {
        qunsetenv("SLICESOFT_PROCESS_PROFILE_DIR");
    }
    else
    {
        qputenv("SLICESOFT_PROCESS_PROFILE_DIR",
                previousProcessProfileDirectory);
    }
    const auto missingTransferProfile = std::find_if(
        missingProcessProfileResolution.profiles.cbegin(),
        missingProcessProfileResolution.profiles.cend(),
        [](const hostprofileavailability& profile)
        {
            return profile.profile.profileid
                == QStringLiteral("host-reference-transfer-channel");
        });
    if (!Check(missingProcessProfileResolved
                   && missingTransferProfile
                       != missingProcessProfileResolution.profiles.cend()
                   && !missingTransferProfile->available
                   && missingTransferProfile->missingcapabilities.contains(
                       QStringLiteral("process-profile.rgbwsvt")),
               QStringLiteral("缺少外部 RGBWSVT 工艺时新 Profile 必须显式禁用。"),
               errors))
    {
        return 5;
    }

    HostProfilePanel panel;
    QString selectedProfile;
    QObject::connect(
        &panel,
        &HostProfilePanel::SigProfileChanged,
        [&selectedProfile](const QString& profileId)
        {
            selectedProfile = profileId;
        });
    panel.SetProfiles(resolution);
    auto* profileCombo = panel.findChild<QComboBox*>(
        QStringLiteral("hostProfileCombo"));
    auto* profileDetails = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("hostProfileCapabilityView"));
    if (!Check(
            profileCombo != nullptr && profileDetails != nullptr
                && profileCombo->count() == 4,
            QStringLiteral("Profile 选择控件或目录项不完整。"),
            errors)
        || !Check(
            panel.AvailableProfileCount() == 4,
            QStringLiteral("可用 Profile 计数错误。"),
            errors)
        || !Check(
            profileCombo->itemData(0, Qt::ToolTipRole)
                    .toString().contains(QStringLiteral("默认工艺"))
                && profileDetails->toPlainText().contains(
                    QStringLiteral("输出合同"))
                && profileDetails->toPlainText().contains(
                    QStringLiteral("使用限制")),
            QStringLiteral("Profile 未提供面向操作员的完整工艺说明。"),
            errors))
    {
        return 6;
    }

    client.ResetCallCount();
    profileCombo->setCurrentIndex(1);
    QCoreApplication::processEvents();
    if (!Check(
            selectedProfile
                == QStringLiteral("host-reference-material-parity"),
            QStringLiteral("Profile 切换未更新宿主会话草稿。"),
            errors)
        || !Check(
            client.CallCount() == 0U,
            QStringLiteral("Profile 本地选择不得调用 DLL。"),
            errors))
    {
        return 7;
    }

    StaticProfileCatalog missingCatalog({MakeProfile(
        QStringLiteral("missing-capability"),
        QStringLiteral("production"),
        {QStringLiteral("profile.capability.not.present")})});
    hostprofilecatalogresolution missingResolution;
    if (!Check(
            HostProfileCapabilityResolver::Resolve(
                missingCatalog,
                client.ModuleInfo(),
                &missingResolution,
                &error),
            QStringLiteral("缺能力 Profile 应成功解析为不可用。"),
            errors)
        || !Check(
            !missingResolution.profiles.first().available
                && missingResolution.profiles.first()
                       .missingcapabilities.size() == 1,
            QStringLiteral("缺失能力未被显式标记。"),
            errors))
    {
        return 8;
    }
    panel.SetProfiles(missingResolution);
    if (!Check(
            panel.AvailableProfileCount() == 0
                && !panel.SelectProfile(
                    QStringLiteral("missing-capability")),
            QStringLiteral("能力不足的 Profile 必须禁用。"),
            errors))
    {
        return 9;
    }

    const hostprofiledescriptor duplicate = MakeProfile(
        QStringLiteral("duplicate"),
        QStringLiteral("production"),
        {QStringLiteral("slice.rgbwsv")});
    StaticProfileCatalog duplicateCatalog({duplicate, duplicate});
    hostprofilecatalogresolution invalidResolution;
    error.clear();
    if (!Check(
            !HostProfileCapabilityResolver::Resolve(
                duplicateCatalog,
                client.ModuleInfo(),
                &invalidResolution,
                &error),
            QStringLiteral("重复 Profile id 必须 fail-closed。"),
            errors))
    {
        return 10;
    }

    StaticProfileCatalog unknownSafetyCatalog({MakeProfile(
        QStringLiteral("unknown-safety"),
        QStringLiteral("unknown"),
        {QStringLiteral("slice.rgbwsv")})});
    error.clear();
    if (!Check(
            !HostProfileCapabilityResolver::Resolve(
                unknownSafetyCatalog,
                client.ModuleInfo(),
                &invalidResolution,
                &error),
            QStringLiteral("未知生产安全级别必须 fail-closed。"),
            errors))
    {
        return 11;
    }

    StaticProfileCatalog emptyCatalog({});
    error.clear();
    if (!Check(
            !HostProfileCapabilityResolver::Resolve(
                emptyCatalog,
                client.ModuleInfo(),
                &invalidResolution,
                &error),
            QStringLiteral("空 Profile 目录必须 fail-closed。"),
            errors))
    {
        return 12;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HB04_PASS profiles=" << resolution.profiles.size()
        << " capabilities=" << resolution.modulecapabilities.size()
        << Qt::endl;
    return 0;
}
