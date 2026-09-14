#include "HostWheelPolicy.h"
#include "ThreeDCanvasWidget.h"
#include "TopViewCanvasWidget.h"
#include <QAbstractItemView>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QVBoxLayout>
#include <stdexcept>

namespace
{
void Require(bool ok,const char* message)
{
    if(!ok) throw std::runtime_error(message);
}

void Wheel(QWidget* target,Qt::MouseButtons buttons=Qt::NoButton,
    QPoint pixel={},QPoint angle={0,-120})
{
    const QPoint center=target->rect().center();
    QWheelEvent event(center,target->mapToGlobal(center),pixel,angle,buttons,
        Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(target,&event);
}

void ParameterAndScrollTest()
{
    QWidget root; HostWheelPolicy policy(&root);
    auto* layout=new QVBoxLayout(&root);
    auto* scroll=new QScrollArea(&root); scroll->setWidgetResizable(true);
    auto* content=new QWidget; content->setMinimumHeight(1600);
    auto* fields=new QVBoxLayout(content);
    auto* spin=new QDoubleSpinBox(content); spin->setRange(0,100); spin->setValue(50);
    auto* combo=new QComboBox(content);
    for(int i=0;i<80;++i) combo->addItem(QString::number(i));
    combo->setCurrentIndex(40); combo->setMaxVisibleItems(8);
    auto* slider=new QSlider(Qt::Horizontal,content); slider->setValue(50);
    auto* tabs=new QTabBar(content); tabs->addTab("one"); tabs->addTab("two");
    fields->addWidget(spin); fields->addWidget(combo); fields->addWidget(slider);
    fields->addWidget(tabs); fields->addStretch();
    scroll->setWidget(content); layout->addWidget(scroll);
    root.resize(420,400); root.show(); QApplication::processEvents();

    int edits=0;
    QObject::connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),[&](double){++edits;});
    QObject::connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),[&](int){++edits;});
    QObject::connect(slider,&QSlider::valueChanged,[&](int){++edits;});
    for(bool focused:{false,true})
        for(QWidget* field:{static_cast<QWidget*>(spin),static_cast<QWidget*>(combo),
            static_cast<QWidget*>(slider),
            static_cast<QWidget*>(spin->findChild<QLineEdit*>())})
        {
            scroll->verticalScrollBar()->setValue(0);
            if(focused) field->setFocus(); else field->clearFocus();
            Wheel(field);
            Require(edits==0,"wheel changed focused/unfocused parameter or tab");
            Require(scroll->verticalScrollBar()->value()>0,"parameter wheel did not scroll parent");
            Wheel(field,Qt::NoButton,{0,-20},{});
            Require(edits==0,"pixel wheel changed parameter");
        }
    Require(spin->value()==50 && combo->currentIndex()==40 && slider->value()==50
        && tabs->currentIndex()==0,"wheel mutated a parameter");
    Wheel(tabs);
    Require(tabs->currentIndex()==1,"tab wheel navigation blocked");
    Wheel(tabs,Qt::NoButton,{},QPoint(0,120));
    Require(tabs->currentIndex()==0,"reverse tab wheel navigation blocked");

    scroll->verticalScrollBar()->setValue(0);
    Wheel(scroll->viewport());
    Require(scroll->verticalScrollBar()->value()>0,"ordinary scrolling blocked");
    scroll->verticalScrollBar()->setValue(0);
    combo->showPopup(); QApplication::processEvents();
    auto* popup=combo->view();
    popup->verticalScrollBar()->setValue(0);
    Wheel(popup->viewport());
    Require(popup->verticalScrollBar()->value()>0,"open combo list scrolling blocked");
    Require(combo->currentIndex()==40,"popup wheel committed another option");
    combo->hidePopup();

    QKeyEvent up(QEvent::KeyPress,Qt::Key_Up,Qt::NoModifier);
    QApplication::sendEvent(spin,&up);
    Require(spin->value()==51,"keyboard editing blocked");
    spin->stepUp();
    Require(spin->value()==52,"explicit step editing blocked");
    auto* lateSpin=new QDoubleSpinBox(content); lateSpin->setValue(5);
    fields->insertWidget(0,lateSpin); Wheel(lateSpin);
    Require(lateSpin->value()==5,"dynamically created input missed wheel guard");

    QWidget otherWindow; QDoubleSpinBox otherSpin(&otherWindow); otherSpin.setValue(5);
    Wheel(&otherSpin);
    Require(otherSpin.value()!=5,"wheel guard leaked to unrelated windows");
}

void CanvasTest()
{
    QWidget root; HostWheelPolicy policy(&root);
    TopViewCanvasWidget top(&root); top.resize(300,200);
    ThreeDCanvasWidget three(&root); three.resize(300,200);
    int redraws=0; three.SetCameraChangedCallback([&]{++redraws;});
    Wheel(&top); Wheel(&three);
    Require(top.ZoomFactor()==1 && redraws==0,"empty canvas navigated");
    QImage image(300,200,QImage::Format_RGB32); image.fill(Qt::white);
    top.SetImage(image); three.SetImage(image); three.SetSceneBounds({0,0,0,100,80,20});
    redraws=0;
    Wheel(&top); Wheel(&three);
    Require(top.ZoomFactor()!=1 && redraws>0,"ordinary canvas zoom blocked");
    const auto zoom=top.ZoomFactor(); const int before=redraws;
    for(auto button:{Qt::LeftButton,Qt::MiddleButton,Qt::RightButton})
    {
        Wheel(&top,button); Wheel(&three,button);
        Require(top.ZoomFactor()==zoom && redraws==before,"wheel zoomed during drag");
    }
}
}

void RunHostWheelPolicyTests()
{
    ParameterAndScrollTest();
    CanvasTest();
}
