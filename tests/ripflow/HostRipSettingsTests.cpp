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
        && pass;

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
    if (pass)
    {
        std::cout << "RIPFLOW_SETTINGS_TESTS_PASS\n";
        return 0;
    }
    return 1;
}
