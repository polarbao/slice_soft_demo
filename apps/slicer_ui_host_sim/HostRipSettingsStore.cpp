#include "HostRipSettingsStore.h"

#include <QSettings>
#include <QStringList>

namespace
{
const QString kGroup = QStringLiteral("hostflow/rip");
const QString kSchema = QStringLiteral("slicesoft.rip.settings.2");
const QString kLegacySchema = QStringLiteral("slicesoft.rip.settings.1");
constexpr int kVersion = 2;

bool IsModuleRelativePath(const QString& value)
{
    const QString normalized = value.trimmed().replace('\\', '/');
    return !normalized.isEmpty()
        && !normalized.startsWith('/')
        && !(normalized.size() > 1 && normalized.at(1) == ':')
        && !normalized.split('/', Qt::SkipEmptyParts).contains(
            QStringLiteral(".."));
}
}

hostripsettings HostRipSettingsStore::Defaults()
{
    return {};
}

bool HostRipSettingsStore::Validate(
    const hostripsettings& settings,
    QString* error)
{
    const bool validValidationMode = settings.outputvalidationmode
            == QStringLiteral("strict_s2")
        || settings.outputvalidationmode
            == QStringLiteral("diagnostic_unvalidated");
    if (settings.renderintent < 0 || settings.renderintent > 3
        || settings.transparentmode < 0 || settings.transparentmode > 4
        || !validValidationMode || settings.colormode != 0
        || !IsModuleRelativePath(settings.inputicc)
        || !IsModuleRelativePath(settings.outputicc)
        || (settings.devicegraybits != 1 && settings.devicegraybits != 2)
        || settings.timeoutseconds < 1
        || settings.timeoutseconds > 86400
        || settings.outputdirectoryname != QStringLiteral("rip")
        || settings.existingoutputpolicy != QStringLiteral("fail_closed"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "RIP 设置无效：intent 仅允许 0..3，RIP 颜色模式仅允许 0..4，"
                "纹理/浮雕模式仅允许 0，"
                "grayBits 仅允许 1/2，输出验证仅允许严格或诊断模式。");
        }
        return false;
    }
    return true;
}

bool HostRipSettingsStore::Load(
    QSettings& settings,
    hostripsettings* value,
    QString* error)
{
    if (value == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 设置接收对象不能为空。");
        }
        return false;
    }
    *value = Defaults();
    settings.beginGroup(kGroup);
    if (!settings.contains(QStringLiteral("schema")))
    {
        settings.endGroup();
        return true;
    }
    const QString schema = settings.value(QStringLiteral("schema")).toString();
    const int version = settings.value(
        QStringLiteral("schemaVersion"), -1).toInt();
    hostripsettings loaded;
    loaded.autoafterslice = settings.value(
        QStringLiteral("autoAfterSlice"), false).toBool();
    loaded.renderintent = settings.value(
        QStringLiteral("renderIntent"), -1).toInt();
    const QVariant storedTransparentMode = settings.value(
        QStringLiteral("transparentMode"));
    bool transparentModeIsInteger{false};
    loaded.transparentmode = storedTransparentMode.toInt(
        &transparentModeIsInteger);
    loaded.colormode = settings.value(
        QStringLiteral("colorMode"), -1).toInt();
    loaded.inputicc = settings.value(QStringLiteral("inputIcc")).toString();
    loaded.outputicc = settings.value(QStringLiteral("outputIcc")).toString();
    loaded.continueonerror = settings.value(
        QStringLiteral("continueOnError"), false).toBool();
    loaded.devicegraybits = settings.value(
        QStringLiteral("deviceGrayBits"), -1).toInt();
    loaded.timeoutseconds = settings.value(
        QStringLiteral("timeoutSeconds"), -1).toInt();
    loaded.outputvalidationmode = settings.value(
        QStringLiteral("outputValidationMode"),
        QStringLiteral("strict_s2")).toString();
    loaded.outputdirectoryname = settings.value(
        QStringLiteral("outputDirectoryName")).toString();
    loaded.existingoutputpolicy = settings.value(
        QStringLiteral("existingOutputPolicy")).toString();
    settings.endGroup();
    if (schema == kLegacySchema && version == 1)
    {
        const QString legacyMode = storedTransparentMode.toString();
        if (legacyMode == QStringLiteral("explicit_transparent"))
        {
            loaded.transparentmode = 0;
        }
        else if (legacyMode == QStringLiteral("explicit_opaque"))
        {
            loaded.transparentmode = 1;
        }
        else if (legacyMode == QStringLiteral("follow_manifest"))
        {
            loaded.transparentmode = 0;
            loaded.autoafterslice = false;
        }
        else
        {
            loaded.transparentmode = -1;
        }
    }
    QString validationError;
    const bool currentVersion = schema == kSchema && version == kVersion;
    const bool migratedLegacy = schema == kLegacySchema && version == 1;
    if ((!currentVersion && !migratedLegacy)
        || (currentVersion && !transparentModeIsInteger)
        || !Validate(loaded, &validationError))
    {
        value->autoafterslice = false;
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "已保存的 RIP 设置版本未知或内容损坏；自动 RIP 已关闭。%1")
                         .arg(validationError);
        }
        return false;
    }
    *value = loaded;
    return true;
}

bool HostRipSettingsStore::Save(
    QSettings& settings,
    const hostripsettings& value,
    QString* error)
{
    if (!Validate(value, error))
    {
        return false;
    }
    settings.beginGroup(kGroup);
    settings.remove(QString{});
    settings.setValue(QStringLiteral("schema"), kSchema);
    settings.setValue(QStringLiteral("schemaVersion"), kVersion);
    settings.setValue(
        QStringLiteral("autoAfterSlice"), value.autoafterslice);
    settings.setValue(QStringLiteral("renderIntent"), value.renderintent);
    settings.setValue(
        QStringLiteral("transparentMode"), value.transparentmode);
    settings.setValue(QStringLiteral("colorMode"), value.colormode);
    settings.setValue(QStringLiteral("inputIcc"), value.inputicc);
    settings.setValue(QStringLiteral("outputIcc"), value.outputicc);
    settings.setValue(
        QStringLiteral("continueOnError"), value.continueonerror);
    settings.setValue(
        QStringLiteral("deviceGrayBits"), value.devicegraybits);
    settings.setValue(
        QStringLiteral("timeoutSeconds"), value.timeoutseconds);
    settings.setValue(
        QStringLiteral("outputValidationMode"),
        value.outputvalidationmode);
    settings.setValue(
        QStringLiteral("outputDirectoryName"), value.outputdirectoryname);
    settings.setValue(
        QStringLiteral("existingOutputPolicy"), value.existingoutputpolicy);
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 设置无法写入系统设置存储。");
        }
        return false;
    }
    return true;
}

bool HostRipSettingsStore::IsDiagnosticMode(
    const hostripsettings& settings)
{
    return settings.outputvalidationmode
        == QStringLiteral("diagnostic_unvalidated");
}

QString HostRipSettingsStore::EffectiveOutputDirectoryName(
    const hostripsettings& settings)
{
    return IsDiagnosticMode(settings)
        ? QStringLiteral("rip_diagnostic")
        : QStringLiteral("rip");
}
