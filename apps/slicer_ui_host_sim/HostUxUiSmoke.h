#pragma once
#include "HostMainWindow.h"
#include "HostProfileTransition.h"
#include "HostInspectorPageSmoke.h"
#include <QApplication>
#include <QAbstractSpinBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTabWidget>
#include <QTextStream>
#include <QThread>
#include <QWheelEvent>
#include <QComboBox>

inline bool CheckHostProfileTransitions()
{
    const auto regular=QStringLiteral("host-reference-default");
    const auto transfer=QStringLiteral("host-reference-transfer-channel");
    for(const auto& preset:HostProcessPresetCatalog::Presets())
    {
        if(!preset.transfereligible) continue;
        hostslicesettings settings;
        settings.profileid=regular; settings.processpresetid=preset.id;
        settings.dpix=643; settings.scenepadtooriginy=true;
        settings.support.offsetmm=0.25; settings.texture.topsurfacelayers=7;
        QString error;
        if(!AlignHostProfileProtocol(transfer,&settings,&error)
            || settings.packageprotocol!=HostPackageProtocol::Rgbwsvt || !settings.transferchannel.enabled
            || settings.dpix!=643 || !settings.scenepadtooriginy
            || settings.support.offsetmm!=0.25 || settings.texture.topsurfacelayers!=7) return false;
        if(!AlignHostProfileProtocol(regular,&settings,&error)
            || settings.packageprotocol!=HostPackageProtocol::Rgbwsv || settings.transferchannel.enabled
            || settings.processpresetid!=preset.id) return false;
    }
    hostslicesettings unsupported; unsupported.profileid=regular;
    QString error;
    return !AlignHostProfileProtocol(transfer,&unsupported,&error) && !error.isEmpty()
        && unsupported.profileid==regular && unsupported.processpresetid==QStringLiteral("custom")
        && unsupported.packageprotocol==HostPackageProtocol::Rgbwsv;
}

inline int RunHostUxUiSmoke(const QString& modulePath,const QString& output)
{
    if (output.isEmpty()) return 2;
    if (!CheckHostProfileTransitions()) return 9;
    QDir().mkpath(output);
    HostMainWindow window(modulePath);
    window.resize(1920,1080); window.show(); QApplication::processEvents();
    auto* tabs=window.findChild<QTabWidget*>(QStringLiteral("hostSceneInspectorTabs"));
    auto* splitter=window.findChild<QSplitter*>(QStringLiteral("workspaceSplitter"));
    if(!tabs || !splitter) return 3;
    splitter->setSizes({1400,420});
    if(!CheckHostTabWheel(tabs)) return 12;
    for (int i=0;i<tabs->count();++i)
    {
        tabs->setCurrentIndex(i); QApplication::processEvents();
        auto* scroll=tabs->currentWidget()->findChild<QScrollArea*>(QStringLiteral("hostInspectorPageScroll"));
        if(!scroll) return 3;
        auto* sections=tabs->currentWidget()->findChild<QTabWidget*>(QStringLiteral("hostInspectorSections"));
        if(sections && !CheckHostTabWheel(sections)) return 12;
        for(int section=0;section<(sections ? sections->count() : 1);++section)
        {
        if(sections) sections->setCurrentIndex(section);
        for(auto* button:tabs->currentWidget()->findChildren<QToolButton*>())
            if(button->isCheckable()) button->setChecked(true);
        // Expanding nested panels posts another round of LayoutRequest events.
        for(int pass=0;pass<10;++pass) { QApplication::processEvents(); QThread::msleep(5); }
        for(auto* field:tabs->currentWidget()->findChildren<QWidget*>())
        {
            const char* property=qobject_cast<QAbstractSpinBox*>(field) ? "value"
                : qobject_cast<QComboBox*>(field) ? "currentIndex" : nullptr;
            if(!property || !field->isVisible() || !field->isEnabled()) continue;
            const auto before=field->property(property);
            field->setFocus();
            const QPoint center=field->rect().center();
            QWheelEvent wheel(center,field->mapToGlobal(center),{},{0,-120},
                Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QApplication::sendEvent(field,&wheel);
            if(field->property(property)!=before) return 10;
            if(tabs->currentIndex()!=i || (sections && sections->currentIndex()!=section)) return 14;
        }
        scroll->verticalScrollBar()->setValue(0);
        for (auto* input:tabs->currentWidget()->findChildren<QAbstractSpinBox*>())
        {
            if(input->isVisible() && (input->height()<input->sizeHint().height()
                || input->width()<input->minimumWidth())) return 4;
            if(input->isVisible())
            {
                const auto* editor=input->findChild<QLineEdit*>();
                if(editor && editor->minimumHeight()>0) return 7;
                if(!input->parentWidget()->rect().contains(input->geometry()))
                {
                    QTextStream(stderr)<<"field clipped: "<<input->objectName()
                        <<" parent="<<input->parentWidget()->metaObject()->className()
                        <<" rect="<<input->x()<<","<<input->y()<<","<<input->width()<<","<<input->height()
                        <<" parentSize="<<input->parentWidget()->width()<<","<<input->parentWidget()->height()<<Qt::endl;
                    window.grab().save(output+"/clipped.png");
                    return 8;
                }
            }
        }
        for (auto* form:tabs->currentWidget()->findChildren<QFormLayout*>())
            for (int row=0;row<form->rowCount();++row)
                if (auto* item=form->itemAt(row,QFormLayout::LabelRole))
                    if (auto* label=qobject_cast<QLabel*>(item->widget()))
                        if (label->isVisible() && (label->height()<label->sizeHint().height()
                            || label->width()<label->sizeHint().width()))
                        {
                            QTextStream(stderr)<<"label clipped: "<<label->text()<<" h="<<label->height()
                                <<" required="<<label->heightForWidth(label->width())<<Qt::endl;
                            return 6;
                        }
        const int overflow=scroll->verticalScrollBar()->maximum();
        QTextStream(stdout)<<"PAGE "<<i<<" SECTION "<<section<<" overflow="<<overflow<<Qt::endl;
        if((i==0 || i==1 || i==4 || i==5) && overflow!=0) return 11;
        if (!window.grab().save(output+QStringLiteral("/tab_%1_section_%2.png").arg(i).arg(section))) return 5;
        auto* inspector=window.findChild<QWidget*>(QStringLiteral("hostModelImportPanel"));
        if(!inspector || !inspector->grab().save(
            output+QStringLiteral("/inspector_%1_section_%2.png").arg(i).arg(section))) return 5;
        if(i==2)
        {
            const auto sizes=splitter->sizes();
            splitter->setSizes({1200,680});
            QApplication::processEvents();
            if(!inspector->grab().save(output+QStringLiteral("/inspector_settings_wide.png"))) return 5;
            splitter->setSizes(sizes);
            QApplication::processEvents();
        }
        }
    }
    if(!CheckHostInspectorScrollFallback(&window,tabs,output)) return 13;
    window.close();
    QTextStream(stdout)<<"HOSTUX_UI_SCALE_PASS dpr="<<window.devicePixelRatioF()<<Qt::endl;
    return 0;
}
