#include "HostWorkspaceMatvolState.h"

#include <QSettings>

void HostWorkspaceMatvolState::Save(
    QSettings& settings,
    const hostmaterialvolumesettings& matvol)
{
    settings.setValue(
        QStringLiteral("materialVolume/enabled"), matvol.enabled);
    settings.setValue(
        QStringLiteral("materialVolume/primaryMaterialName"),
        matvol.primarymaterialname);
    settings.setValue(
        QStringLiteral("materialVolume/primaryPriority"),
        matvol.primarypriority);
    settings.setValue(
        QStringLiteral("materialVolume/secondaryMaterialName"),
        matvol.secondarymaterialname);
    settings.setValue(
        QStringLiteral("materialVolume/secondaryPriority"),
        matvol.secondarypriority);
}

bool HostWorkspaceMatvolState::Restore(
    QSettings& settings,
    hostmaterialvolumesettings* matvol)
{
    if (matvol == nullptr)
    {
        return false;
    }
    if (!settings.contains(QStringLiteral("materialVolume/enabled")))
    {
        return false;
    }
    hostmaterialvolumesettings restored;
    restored.enabled = settings.value(
        QStringLiteral("materialVolume/enabled")).toBool();
    restored.primarymaterialname = settings.value(
        QStringLiteral("materialVolume/primaryMaterialName")).toString();
    restored.primarypriority = settings.value(
        QStringLiteral("materialVolume/primaryPriority"), -1).toInt();
    restored.secondarymaterialname = settings.value(
        QStringLiteral("materialVolume/secondaryMaterialName")).toString();
    restored.secondarypriority = settings.value(
        QStringLiteral("materialVolume/secondaryPriority"), -1).toInt();

    const bool rangesValid = restored.primarypriority >= 0
        && restored.primarypriority <= 100000
        && restored.secondarypriority >= 0
        && restored.secondarypriority <= 100000;
    /* 启用时必须两个材质名都非空、互不相同、且优先级不同级；
       未启用时不对名称与优先级组合设限，允许保留上次输入。 */
    const bool combinationValid = !restored.enabled
        || (!restored.primarymaterialname.trimmed().isEmpty()
            && !restored.secondarymaterialname.trimmed().isEmpty()
            && restored.primarymaterialname.trimmed()
                != restored.secondarymaterialname.trimmed()
            && restored.primarypriority != restored.secondarypriority);
    if (!rangesValid || !combinationValid)
    {
        return false;
    }
    *matvol = restored;
    return true;
}
