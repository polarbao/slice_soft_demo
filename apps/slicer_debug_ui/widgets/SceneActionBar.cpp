#include "SceneActionBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>

SceneActionBar::SceneActionBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sceneActionBar"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_sliceButton = new QPushButton(
        QStringLiteral("切片当前场景"),
        this);
    m_sliceButton->setObjectName(
        QStringLiteral("sliceCurrentSceneButton"));
    m_sliceButton->setMinimumHeight(36);
    m_sliceButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed);

    m_cancelButton = new QPushButton(this);
    m_cancelButton->setObjectName(
        QStringLiteral("cancelCurrentSceneSliceButton"));
    m_cancelButton->setIcon(
        style()->standardIcon(QStyle::SP_BrowserStop));
    m_cancelButton->setToolTip(
        QStringLiteral("取消当前场景切片"));
    m_cancelButton->setFixedSize(36, 36);

    m_statusLabel = new QLabel(
        QStringLiteral("待切片"),
        this);
    m_statusLabel->setObjectName(
        QStringLiteral("sceneSliceActionStateLabel"));
    m_statusLabel->setWordWrap(true);

    layout->addWidget(m_sliceButton, 1);
    layout->addWidget(m_cancelButton);
    layout->addWidget(m_statusLabel);

    connect(
        m_sliceButton,
        &QPushButton::clicked,
        this,
        &SceneActionBar::SigSliceRequested);
    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &SceneActionBar::SigCancelRequested);
    SetPresentation(
        false,
        false,
        QStringLiteral("待导入"),
        QStringLiteral("请先导入模型。"));
}

void SceneActionBar::SetPresentation(
    const bool canSlice,
    const bool canCancel,
    const QString& status,
    const QString& reason)
{
    m_sliceButton->setEnabled(canSlice);
    m_sliceButton->setToolTip(reason);
    m_cancelButton->setEnabled(canCancel);
    m_statusLabel->setText(status);
    m_statusLabel->setToolTip(reason);
}

QPushButton* SceneActionBar::SliceButton() const
{
    return m_sliceButton;
}
