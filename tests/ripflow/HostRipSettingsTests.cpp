#include "HostRipSettingsStore.h"

#include <QSettings>
#include <QTemporaryDir>

#include <iostream>

namespace
{
bool Expect(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}
}

int main()
{
    QTemporaryDir temporary;
    if (!Expect(temporary.isValid(), "temporary settings directory exists"))
    {
        return 1;
    }
    const QString path = temporary.filePath(QStringLiteral("rip.ini"));
    QSettings storage(path, QSettings::IniFormat);
    hostripsettings defaults = HostRipSettingsStore::Defaults();
    bool pass = Expect(!defaults.autoafterslice, "automatic RIP defaults off")
        && Expect(
            defaults.outputvalidationmode == QStringLiteral("strict_s2"),
            "strict S2 publication is the default")
        && Expect(
            defaults.outputdirectoryname == QStringLiteral("rip"),
            "output directory is frozen")
        && Expect(
            HostRipSettingsStore::Save(storage, defaults),
            "valid settings save");

    hostripsettings restored;
    pass = Expect(
        HostRipSettingsStore::Load(storage, &restored),
        "valid settings restore")
        && Expect(!restored.autoafterslice, "restored auto remains off")
        && Expect(restored.renderintent == 0, "intent round trips")
        && Expect(restored.devicegraybits == 2, "grayBits round trips")
        && Expect(
            restored.outputvalidationmode == QStringLiteral("strict_s2"),
            "validation mode round trips")
        && pass;

    hostripsettings diagnostic = defaults;
    diagnostic.outputvalidationmode =
        QStringLiteral("diagnostic_unvalidated");
    pass = Expect(
        HostRipSettingsStore::Validate(diagnostic),
        "explicit diagnostic mode is valid")
        && Expect(
            HostRipSettingsStore::EffectiveOutputDirectoryName(diagnostic)
                == QStringLiteral("rip_diagnostic"),
            "diagnostic mode uses an isolated output directory")
        && pass;

    // 诊断模式必须真正存盘再读回：Load 在键缺失时回落 strict_s2，
    // 因此只往返默认值的断言即使 Save 完全不写该键也会通过。
    const QString diagnosticPath =
        temporary.filePath(QStringLiteral("rip_diagnostic.ini"));
    {
        QSettings diagnosticStorage(diagnosticPath, QSettings::IniFormat);
        pass = Expect(
            HostRipSettingsStore::Save(diagnosticStorage, diagnostic),
            "diagnostic settings save")
            && pass;
    }
    {
        QSettings diagnosticStorage(diagnosticPath, QSettings::IniFormat);
        hostripsettings restoredDiagnostic;
        pass = Expect(
            HostRipSettingsStore::Load(
                diagnosticStorage, &restoredDiagnostic),
            "diagnostic settings restore")
            && pass;
        pass = Expect(
            restoredDiagnostic.outputvalidationmode
                == QStringLiteral("diagnostic_unvalidated"),
            "diagnostic validation mode survives a save and load")
            && pass;
        pass = Expect(
            HostRipSettingsStore::EffectiveOutputDirectoryName(
                restoredDiagnostic) == QStringLiteral("rip_diagnostic"),
            "restored diagnostic mode still isolates the output directory")
            && pass;
    }

    storage.beginGroup(QStringLiteral("hostflow/rip"));
    storage.setValue(QStringLiteral("schemaVersion"), 999);
    storage.setValue(QStringLiteral("autoAfterSlice"), true);
    storage.endGroup();
    storage.sync();
    restored.autoafterslice = true;
    QString error;
    pass = Expect(
        !HostRipSettingsStore::Load(storage, &restored, &error),
        "unknown schema version fails")
        && Expect(
            !restored.autoafterslice,
            "unknown schema version disables automatic RIP")
        && Expect(!error.isEmpty(), "corruption failure is explained")
        && pass;

    hostripsettings invalid = defaults;
    invalid.inputicc = QStringLiteral("../escape.icc");
    pass = Expect(
        !HostRipSettingsStore::Validate(invalid),
        "module-relative path escape fails")
        && pass;
    invalid = defaults;
    invalid.outputvalidationmode = QStringLiteral("ignore_s2");
    pass = Expect(
        !HostRipSettingsStore::Validate(invalid),
        "unknown output validation mode fails")
        && pass;
    if (pass)
    {
        std::cout << "RIPFLOW_SETTINGS_TESTS_PASS\n";
        return 0;
    }
    return 1;
}
