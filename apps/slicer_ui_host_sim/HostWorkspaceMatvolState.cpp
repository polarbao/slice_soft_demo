#include "HostWorkspaceMatvolState.h"

#include <QSettings>
#include <cmath>

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
    settings.setValue(QStringLiteral("materialVolume/overlapAutoByName"),matvol.overlapautobyname);
    settings.setValue(QStringLiteral("materialVolume/opacityVarnishEnabled"),matvol.opacityvarnishenabled);
    settings.setValue(QStringLiteral("materialVolume/opacityVarnishMax"),matvol.opacityvarnishmax);
    settings.setValue(QStringLiteral("materialVolume/degenerateAreaEpsilonMm2"),matvol.degenerateareaepsilonmm2);
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

    int fields=0;
    for(const auto* key:{"overlapAutoByName","opacityVarnishEnabled","opacityVarnishMax","degenerateAreaEpsilonMm2"})
        fields+=settings.contains(QStringLiteral("materialVolume/")+QString::fromLatin1(key)) ? 1 : 0;
    if(fields!=0 && fields!=4) return false;
    if(fields==4)
    {
        for(const auto* key:{"overlapAutoByName","opacityVarnishEnabled"})
        {
            const auto value=settings.value(QStringLiteral("materialVolume/")+QString::fromLatin1(key)).toString();
            if(value!=QStringLiteral("true") && value!=QStringLiteral("false")
                && value!=QStringLiteral("1") && value!=QStringLiteral("0")) return false;
        }
        restored.overlapautobyname=settings.value(QStringLiteral("materialVolume/overlapAutoByName")).toBool();
        restored.opacityvarnishenabled=settings.value(QStringLiteral("materialVolume/opacityVarnishEnabled")).toBool();
        bool opacityValid=false,epsilonValid=false;
        restored.opacityvarnishmax=settings.value(QStringLiteral("materialVolume/opacityVarnishMax")).toDouble(&opacityValid);
        restored.degenerateareaepsilonmm2=settings.value(QStringLiteral("materialVolume/degenerateAreaEpsilonMm2")).toDouble(&epsilonValid);
        if(!opacityValid || !epsilonValid || !std::isfinite(restored.opacityvarnishmax)
            || restored.opacityvarnishmax<=0 || restored.opacityvarnishmax>=1
            || !std::isfinite(restored.degenerateareaepsilonmm2)) return false;
    }
    else if(restored.enabled && settings.value(QStringLiteral("processPresetId")).toString()
        ==QStringLiteral("multilayer_transparent_varnish_lower_support"))
    {
        // Recover only the four omitted fields of the known historical preset.
        // Custom/manual settings retain their previous validation contract.
        restored.overlapautobyname=true;
        restored.opacityvarnishenabled=true;
        restored.opacityvarnishmax=0.001;
        restored.degenerateareaepsilonmm2=1e-24;
    }

    const bool rangesValid = restored.primarypriority >= 0
        && restored.primarypriority <= 100000
        && restored.secondarypriority >= 0
        && restored.secondarypriority <= 100000;
    // Auto-by-name does not use the two manual priority slots.
    const bool combinationValid = !restored.enabled
        || restored.overlapautobyname
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
