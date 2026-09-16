#pragma once
#include "HostMainWindow.h"
#include "ViewWorkspaceWidget.h"
#include "ThreeDCanvasWidget.h"
#include "HostWorkspaceState.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTextStream>
#include <QThread>
#include <QUuid>

inline int RunHostUxSceneSmoke(const QString& module,const QString& model,const QString& evidenceRoot)
{
    if(model.isEmpty() || evidenceRoot.isEmpty()) return 2;
    const bool presetFlow=QApplication::arguments().contains(QStringLiteral("--preset-after-import"));
    const QString output=presetFlow ? QDir(evidenceRoot).absoluteFilePath(
        QStringLiteral("r")+QUuid::createUuid().toString(QUuid::Id128).left(12)) : evidenceRoot;
    if(presetFlow && QDir(output).exists()) return 2;
    QDir().mkpath(output);
    QTextStream(stdout)<<"SCENE_EVIDENCE "<<output<<Qt::endl;
    HostMainWindow window(module); window.resize(1600,960); window.show();
    if(QApplication::arguments().contains(QStringLiteral("--restore-multilayer")))
    {
        auto* preset=window.findChild<QComboBox*>(QStringLiteral("hostProcessPresetCombo"));
        preset->setCurrentIndex(preset->findData(QStringLiteral("multilayer_transparent_varnish_lower_support")));
        QTemporaryDir temporary;
        if(!temporary.isValid()) return 16;
        QSettings store(temporary.filePath(QStringLiteral("workspace.ini")),QSettings::IniFormat);
        if(!HostWorkspaceState::Save(store,&window,window.m_workspaceSplitter,window.m_workspaceTabs,
            window.m_inspectorTabs,window.m_sliceSettingsPanel->Settings())) return 16;
        hostworkspacepreferences restored;
        if(!HostWorkspaceState::Restore(store,&window,window.m_workspaceSplitter,window.m_workspaceTabs,
            window.m_inspectorTabs,&restored)) return 16;
        window.m_sliceSettingsPanel->SetPersistentSettings(restored.slicesettings);
        QTextStream(stdout)<<"MULTILAYER_WORKSPACE_RESTORE_PASS"<<Qt::endl;
    }
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
    if(!presetFlow) for(const auto* profile:{"host-reference-material-parity","host-reference-transfer-channel","host-reference-default"})
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
    auto* start=window.findChild<QPushButton*>(QStringLiteral("hostSliceStartButton"));
    if(presetFlow)
    {
        QApplication::processEvents();
        const auto* validation=window.findChild<QLabel*>(QStringLiteral("hostSliceValidationLabel"));
        QTextStream(stdout)<<"PRESET_AFTER_IMPORT ready="<<window.m_sliceSettingsPanel->IsReady()
            <<" button="<<(start && start->isEnabled())<<" profile="<<window.m_importWorkflow->SceneProfileId()
            <<" reason="<<(validation ? validation->text() : QString{})<<Qt::endl;
        if(!start || !start->isEnabled()) return 15;
        auto* path=window.findChild<QLineEdit*>(QStringLiteral("hostSliceOutputEdit"));
        if(!path) return 17;
        {
            const auto previous=path->text(); path->clear();
            if(start->isEnabled() || start->toolTip()!=window.m_sliceSettingsPanel->ReadinessMessage()) return 17;
            path->setText(previous);
            if(!start->isEnabled()) return 17;
        }
    }
    auto settings=window.m_sliceSettingsPanel->Settings();
    settings.dpix=100;settings.dpiy=100;settings.layerthicknessmm=0.15;
    settings.outputdirectory=QDir(output).absoluteFilePath(QStringLiteral("package"));
    window.m_sliceSettingsPanel->SetPersistentSettings(settings);
    window.OnSliceSettingsChanged();
    if(!presetFlow) window.m_profilePanel->SelectProfile(transfer ? QStringLiteral("host-reference-transfer-channel")
        : QStringLiteral("host-reference-material-parity"));
    QTextStream(stdout)<<"PRE_SLICE ready="<<window.m_sliceSettingsPanel->IsReady()
        <<" hash="<<window.m_sliceSettingsPanel->EffectiveProfile().profilehash
        <<" T="<<window.m_sliceSettingsPanel->Settings().transferchannel.enabled<<Qt::endl;
    if(presetFlow) start->click(); else window.OnStartSlice();
    // F-41：这两个期限原为 120 s / 130 s，而本机空载实测整轮需 108~117 s——余量不足 1.1 倍。
    // 有负载时切片超过 120 s，循环退出后走 OnCancelSlice() 并 return 6，对外表现为
    // 「154 s 失败且无任何消息」，既不是 ctest 超时（那是 180 s）也不像真回归，每次都要定向复跑才能定责。
    // 抬到 480 s / 520 s（约 4 倍余量），并保持 < 注册处的 ctest TIMEOUT 600 s：
    // 内部期限先触发才有自述消息，让 ctest 先超时只会得到一条无信息的红灯。
    constexpr qint64 kSliceWaitMs{480000};
    constexpr qint64 kResultLoadWaitMs{520000};
    QElapsedTimer timer;timer.start();
    while(window.m_sliceJobController->IsActive() && timer.elapsed()<kSliceWaitMs)
    {
        QApplication::processEvents();QThread::msleep(10);
    }
    if(window.m_sliceJobController->IsActive())
    {
        // 原先这里直接 return 6，不打印任何原因——失败无法与真回归区分。
        QTextStream(stdout)<<"SCENE_UI_SLICE_TIMEOUT waited_ms="<<timer.elapsed()
            <<" limit_ms="<<kSliceWaitMs<<" reason=slice_job_still_active"<<Qt::endl;
        window.OnCancelSlice();
        return 6;
    }
    const auto completion=window.m_sliceJobController->Completion();
    QTextStream(stdout)<<"SCENE_UI_SLICE success="<<completion.success<<" code="<<completion.code
        <<" message="<<completion.message<<Qt::endl;
    while(window.m_resultLoadActive && timer.elapsed()<kResultLoadWaitMs) { QApplication::processEvents(); QThread::msleep(10); }
    if(window.m_resultLoadActive)
    {
        QTextStream(stdout)<<"SCENE_UI_RESULT_LOAD_TIMEOUT waited_ms="<<timer.elapsed()
            <<" limit_ms="<<kResultLoadWaitMs<<" reason=result_load_still_active"<<Qt::endl;
    }
    return completion.success ? 0 : 7;
}
