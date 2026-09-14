#pragma once
#include "HostMainWindow.h"
#include "ViewWorkspaceWidget.h"
#include "ThreeDCanvasWidget.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTextStream>
#include <QThread>

inline int RunHostUxSceneSmoke(const QString& module,const QString& model,const QString& output)
{
    if(model.isEmpty() || output.isEmpty()) return 2;
    QDir().mkpath(output);
    HostMainWindow window(module); window.resize(1600,960); window.show();
    window.ImportModelPaths({model});
    if(window.m_importWorkflow->InstanceCount()!=1) return 3;
    window.m_workspace->SetMode(HostViewMode::ThreeD);
    QApplication::processEvents();
    auto* canvas=window.m_workspace->ThreeDCanvas();
    const auto selected=canvas->grab().toImage();
    selected.save(output+"/selected.png");
    QRect outline;
    for(int y=0;y<selected.height();++y) for(int x=0;x<selected.width();++x)
        if(selected.pixelColor(x,y)==QColor(32,144,255)) outline|=QRect(x,y,1,1);
    if(outline.isEmpty()) return 8;
    canvas->ZoomAtCursor(6,2.0F*outline.center().x()/selected.width()-1.0F,
        1.0F-2.0F*outline.center().y()/selected.height());
    QApplication::processEvents();
    canvas->grab().save(output+"/selected_detail.png");
    for(const auto* profile:{"host-reference-material-parity","host-reference-transfer-channel","host-reference-default"})
    {
        window.m_profilePanel->SelectProfile(QString::fromLatin1(profile));
        QApplication::processEvents();
        QTextStream(stdout)<<profile<<" ready="<<window.m_sliceSettingsPanel->IsReady()
            <<" scene="<<window.m_importWorkflow->SceneProfileId()<<Qt::endl;
        if(window.m_importWorkflow->SceneProfileId()!=QString::fromLatin1(profile)) return 4;
    }
    auto* presets=window.m_sliceSettingsPanel->findChild<QComboBox*>(QStringLiteral("hostProcessPresetCombo"));
    if(!presets) return 5;
    const bool transfer=model.contains(QStringLiteral("08-04"));
    if(!transfer) presets->setCurrentIndex(presets->findData(QStringLiteral("multilayer_transparent_varnish_lower_support")));
    auto settings=window.m_sliceSettingsPanel->Settings();
    settings.dpix=100;settings.dpiy=100;settings.layerthicknessmm=0.15;
    settings.outputdirectory=QDir(output).absoluteFilePath(QStringLiteral("package"));
    window.m_sliceSettingsPanel->SetPersistentSettings(settings);
    window.OnSliceSettingsChanged();
    window.m_profilePanel->SelectProfile(transfer ? QStringLiteral("host-reference-transfer-channel")
        : QStringLiteral("host-reference-material-parity"));
    QTextStream(stdout)<<"PRE_SLICE ready="<<window.m_sliceSettingsPanel->IsReady()
        <<" hash="<<window.m_sliceSettingsPanel->EffectiveProfile().profilehash
        <<" T="<<window.m_sliceSettingsPanel->Settings().transferchannel.enabled<<Qt::endl;
    window.OnStartSlice();
    QElapsedTimer timer;timer.start();
    while(window.m_sliceJobController->IsActive() && timer.elapsed()<120000)
    {
        QApplication::processEvents();QThread::msleep(10);
    }
    if(window.m_sliceJobController->IsActive()) { window.OnCancelSlice(); return 6; }
    const auto completion=window.m_sliceJobController->Completion();
    QTextStream(stdout)<<"SCENE_UI_SLICE success="<<completion.success<<" code="<<completion.code
        <<" message="<<completion.message<<Qt::endl;
    while(window.m_resultLoadActive && timer.elapsed()<130000) { QApplication::processEvents(); QThread::msleep(10); }
    return completion.success ? 0 : 7;
}
