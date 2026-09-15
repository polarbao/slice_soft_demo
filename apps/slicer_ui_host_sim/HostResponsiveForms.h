#pragma once
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

inline void ConfigureHostForms(QWidget* root)
{
    for (auto* form : root->findChildren<QFormLayout*>())
    {
        // Stacked labels avoid Qt 5 form-row height clipping at fractional DPI.
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        // Qt 5's wrapped form rows under-report nested layout heights. Give
        // compound fields a widget so their minimum size participates normally.
        for (int row=0;row<form->rowCount();++row)
            if (auto* item=form->itemAt(row,QFormLayout::FieldRole))
                if (auto* nested=item->layout())
                {
                    for(int index=0;index<form->count();++index)
                        if(form->itemAt(index)==item) { form->takeAt(index); break; }
                    nested->setParent(nullptr);
                    auto* field=new QWidget(form->parentWidget());
                    field->setObjectName(QStringLiteral("hostResponsiveField"));
                    field->setLayout(nested);
                    nested->setSizeConstraint(QLayout::SetMinimumSize);
                    form->setWidget(row,QFormLayout::FieldRole,field);
                }
        for (int row=0;row<form->rowCount();++row)
            if (auto* item=form->itemAt(row,QFormLayout::FieldRole))
                if (auto* field=item->widget())
                    if(field->layout()) field->layout()->setSizeConstraint(QLayout::SetMinimumSize);
        for (int row=0;row<form->rowCount();++row)
            if (auto* item=form->itemAt(row,QFormLayout::LabelRole))
                if (auto* label=qobject_cast<QLabel*>(item->widget()))
                {
                    label->setWordWrap(false);
                    label->setMinimumHeight(label->sizeHint().height());
                    label->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);
                }
    }
    for (auto* widget : root->findChildren<QWidget*>())
    {
        if (qobject_cast<QLineEdit*>(widget)
            && (qobject_cast<QAbstractSpinBox*>(widget->parentWidget())
                || qobject_cast<QComboBox*>(widget->parentWidget()))) continue;
        if (qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QLineEdit*>(widget)
            || qobject_cast<QComboBox*>(widget))
        {
            widget->setMinimumHeight(widget->sizeHint().height());
            widget->setMinimumWidth(widget->fontMetrics().horizontalAdvance(QStringLiteral("000000000000"))+32);
            if (auto* combo=qobject_cast<QComboBox*>(widget))
            {
                combo->setMinimumContentsLength(12);
                combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            }
        }
    }
    for(auto* field:root->findChildren<QWidget*>(QStringLiteral("hostResponsiveField")))
    {
        field->layout()->invalidate();
        field->setMinimumSize(field->layout()->minimumSize());
    }
}
