#pragma once

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDial>
#include <QSlider>
#include <QWheelEvent>

// Wheel gestures navigate views and tabs; they never edit host parameters.
class HostWheelPolicy final : public QObject
{
public:
    explicit HostWheelPolicy(QWidget* root) : QObject(root), m_root(root)
    {
        qApp->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* receiver,QEvent* event) override
    {
        if(event->type()!=QEvent::Wheel) return false;
        auto* widget=qobject_cast<QWidget*>(receiver);
        if(!widget || (widget!=m_root && !m_root->isAncestorOf(widget))) return false;
        QWidget* control=nullptr;
        for(auto* current=widget;current && current!=m_root;current=current->parentWidget())
        {
            // Includes combo popup lists: keep their native scrolling behavior.
            if(qobject_cast<QAbstractScrollArea*>(current)) return false;
            if(qobject_cast<QAbstractSpinBox*>(current) || qobject_cast<QComboBox*>(current)
                || qobject_cast<QSlider*>(current) || qobject_cast<QDial*>(current))
            {
                control=current;
                break;
            }
        }
        if(!control) return false;
        auto* wheel=static_cast<QWheelEvent*>(event);
        for(auto* parent=control->parentWidget();parent;parent=parent->parentWidget())
        {
            if(auto* area=qobject_cast<QAbstractScrollArea*>(parent))
            {
                auto* viewport=area->viewport();
                const QPointF local=viewport->mapFromGlobal(wheel->globalPosition().toPoint());
                QWheelEvent forwarded(local,wheel->globalPosition(),wheel->pixelDelta(),
                    wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),wheel->phase(),
                    wheel->inverted(),wheel->source());
                QApplication::sendEvent(viewport,&forwarded);
                break;
            }
            if(parent==m_root) break;
        }
        wheel->accept();
        return true;
    }

private:
    QWidget* m_root;
};
