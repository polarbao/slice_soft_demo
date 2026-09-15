#pragma once

#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

inline void GroupHostInspectorSections(QWidget* page,const QStringList& titles)
{
    auto* layout=qobject_cast<QVBoxLayout*>(page->layout());
    const auto groups=page->findChildren<QGroupBox*>(QString{},Qt::FindDirectChildrenOnly);
    if(!layout || groups.size()!=titles.size()) return;
    auto* sections=new QTabWidget(page);
    sections->setObjectName(QStringLiteral("hostInspectorSections"));
    const int position=layout->indexOf(groups.front());
    for(int i=0;i<groups.size();++i)
    {
        layout->removeWidget(groups[i]);
        sections->addTab(groups[i],titles[i]);
        for(auto* form:groups[i]->findChildren<QFormLayout*>())
            form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    }
    layout->insertWidget(position,sections);
}

inline void ConfigureHostInspectorPages(QTabWidget* tabs)
{
    // Keep the tab bar outside the scroll viewport. Each page owns its range,
    // so an expanded slicing form cannot inflate RIP/layout/model pages.
    for(int i=0;i<tabs->count();++i)
    {
        auto* page=tabs->widget(i);
        auto* content=new QWidget;
        content->setObjectName(QStringLiteral("hostInspectorPageContent"));
        content->setLayout(page->layout());
        auto* layout=new QVBoxLayout(page);
        layout->setContentsMargins(0,0,0,0);
        auto* scroll=new QScrollArea(page);
        scroll->setObjectName(QStringLiteral("hostInspectorPageScroll"));
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setWidget(content);
        layout->addWidget(scroll);
    }
}
