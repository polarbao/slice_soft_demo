#pragma once
#include "apps/slicer_ui_host_sim/HostWorkspaceMatvolState.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

inline bool CheckMatvolPersistence()
{
    QTemporaryDir temporary;
    if(!temporary.isValid()) return false;
    const QString file=temporary.filePath(QStringLiteral("material.ini"));
    hostmaterialvolumesettings expected;
    expected.enabled=true; expected.overlapautobyname=true;
    expected.opacityvarnishenabled=true; expected.opacityvarnishmax=0.02;
    expected.degenerateareaepsilonmm2=1e-24;
    {
        QSettings saved(file,QSettings::IniFormat);
        HostWorkspaceMatvolState::Save(saved,expected);
    }
    QSettings settings(file,QSettings::IniFormat);
    hostmaterialvolumesettings actual;
    if(!HostWorkspaceMatvolState::Restore(settings,&actual)
        || !actual.enabled || !actual.overlapautobyname || !actual.opacityvarnishenabled
        || actual.opacityvarnishmax!=0.02 || actual.degenerateareaepsilonmm2!=1e-24)
    {
        QTextStream(stderr)<<"MATVOL_PERSISTENCE_LOST_MULTILAYER_SETTINGS"<<Qt::endl;
        return false;
    }
    for(const auto* key:{"overlapAutoByName","opacityVarnishEnabled","opacityVarnishMax","degenerateAreaEpsilonMm2"})
        settings.remove(QStringLiteral("materialVolume/")+QString::fromLatin1(key));
    settings.setValue(QStringLiteral("processPresetId"),QStringLiteral("multilayer_transparent_varnish_lower_support"));
    if(!HostWorkspaceMatvolState::Restore(settings,&actual) || !actual.overlapautobyname
        || !actual.opacityvarnishenabled || actual.opacityvarnishmax!=0.001
        || actual.degenerateareaepsilonmm2!=1e-24) return false;
    settings.setValue(QStringLiteral("processPresetId"),QStringLiteral("custom"));
    if(HostWorkspaceMatvolState::Restore(settings,&actual)) return false;
    settings.setValue(QStringLiteral("materialVolume/primaryMaterialName"),QStringLiteral("01"));
    settings.setValue(QStringLiteral("materialVolume/secondaryMaterialName"),QStringLiteral("02"));
    if(!HostWorkspaceMatvolState::Restore(settings,&actual) || actual.overlapautobyname
        || actual.opacityvarnishenabled) return false;
    HostWorkspaceMatvolState::Save(settings,expected);
    settings.setValue(QStringLiteral("materialVolume/opacityVarnishMax"),1.0);
    if(HostWorkspaceMatvolState::Restore(settings,&actual)) return false;
    HostWorkspaceMatvolState::Save(settings,expected);
    settings.remove(QStringLiteral("materialVolume/opacityVarnishEnabled"));
    if(HostWorkspaceMatvolState::Restore(settings,&actual)) return false;
    HostWorkspaceMatvolState::Save(settings,expected);
    settings.setValue(QStringLiteral("materialVolume/degenerateAreaEpsilonMm2"),QStringLiteral("invalid"));
    if(HostWorkspaceMatvolState::Restore(settings,&actual)) return false;
    return true;
}
