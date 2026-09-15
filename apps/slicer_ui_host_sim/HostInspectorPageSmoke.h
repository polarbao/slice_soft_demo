#pragma once
#include <QApplication>
#include <QAbstractSpinBox>
#include <QMainWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QTabWidget>
#include <QThread>
#include <QWheelEvent>

inline bool CheckHostTabWheel(QTabWidget* tabs)
{
    if(tabs->count()<2) return true;
    tabs->setCurrentIndex(0);
    auto* bar=tabs->tabBar();
    for(int delta:{-120,120})
    {
        const QPoint center=bar->rect().center();
        QWheelEvent wheel(center,bar->mapToGlobal(center),{},{0,delta},
            Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(bar,&wheel);
        if(tabs->currentIndex()!=(delta<0 ? 1 : 0)) return false;
    }
    return true;
}

inline bool CheckHostInspectorScrollFallback(QMainWindow* window,QTabWidget* tabs,const QString& output)
{
    for(const auto size:{QSize(1280,720),QSize(1024,600)})
    {
        window->resize(size);
        for(int i=0;i<tabs->count();++i)
        {
            tabs->setCurrentIndex(i);
            auto* scroll=tabs->currentWidget()->findChild<QScrollArea*>(QStringLiteral("hostInspectorPageScroll"));
            if(!scroll) return false;
            auto* sections=tabs->currentWidget()->findChild<QTabWidget*>(QStringLiteral("hostInspectorSections"));
            for(int section=0;section<(sections ? sections->count() : 1);++section)
            {
                if(sections) sections->setCurrentIndex(section);
                for(int n=0;n<10;++n) { QApplication::processEvents(); QThread::msleep(5); }
                const QPoint tabPosition=tabs->tabBar()->mapTo(window,QPoint{});
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                QApplication::processEvents();
                if(tabs->tabBar()->mapTo(window,QPoint{})!=tabPosition) return false;
                if(!window->rect().contains(QRect(tabPosition,tabs->tabBar()->size()))) return false;
                for(auto* input:tabs->currentWidget()->findChildren<QAbstractSpinBox*>())
                    if(input->isVisible() && (!input->parentWidget()->rect().contains(input->geometry())
                        || input->height()<input->sizeHint().height())) return false;
                if((i==4 || i==5) && section==0)
                    if(!window->grab().save(output+QStringLiteral("/small_%1_tab_%2.png").arg(size.height()).arg(i)))
                        return false;
            }
        }
    }
    return true;
}
