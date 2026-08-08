#include "ViewWorkspaceWidget.h"

#include "TopViewCanvasWidget.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
QLabel* CreatePlaceholderCanvas(
    const QString& objectName,
    const QString& title,
    QWidget* parent)
{
    auto* canvas = new QLabel(parent);
    canvas->setObjectName(objectName);
    canvas->setAlignment(Qt::AlignCenter);
    canvas->setText(title);
    canvas->setMinimumSize(640, 400);
    canvas->setStyleSheet(QStringLiteral(
        "QLabel { color: #f2f3f5; background: #2b2d31; "
        "border: 1px solid #686d75; }"));
    return canvas;
}
}

ViewWorkspaceWidget::ViewWorkspaceWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(0);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);

    m_topButton = new QToolButton(this);
    m_topButton->setObjectName(QStringLiteral("topViewButton"));
    m_topButton->setText(QStringLiteral("俯视"));
    m_topButton->setCheckable(true);
    m_topButton->setToolTip(QStringLiteral("+Z 正交俯视，显示真实表面纹理"));
    group->addButton(m_topButton);

    m_threeDButton = new QToolButton(this);
    m_threeDButton->setObjectName(QStringLiteral("threeDViewButton"));
    m_threeDButton->setText(QStringLiteral("3D"));
    m_threeDButton->setCheckable(true);
    m_threeDButton->setToolTip(QStringLiteral(
        "带纹理三维视图，支持本地相机操作"));
    group->addButton(m_threeDButton);

    toolbar->addWidget(m_topButton);
    toolbar->addWidget(m_threeDButton);
    toolbar->addStretch(1);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("viewErrorLabel"));
    m_errorLabel->setStyleSheet(QStringLiteral("color: #c62828;"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();

    m_selectionLabel = new QLabel(
        QStringLiteral("未选择模型实例。"), this);
    m_selectionLabel->setObjectName(QStringLiteral("viewSelectionLabel"));
    m_selectionLabel->setWordWrap(true);
    m_selectionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_canvasStack = new QStackedWidget(this);
    m_canvasStack->setObjectName(QStringLiteral("viewCanvasStack"));
    m_topCanvas = new TopViewCanvasWidget(m_canvasStack);
    m_topCanvas->setObjectName(QStringLiteral("topCanvas"));
    m_canvasStack->addWidget(m_topCanvas);
    m_canvasStack->addWidget(CreatePlaceholderCanvas(
        QStringLiteral("threeDCanvas"),
        QStringLiteral("3D 工作区\n正交 / 透视 · 七向预设"),
        m_canvasStack));

    root->addLayout(toolbar);
    root->addWidget(m_errorLabel);
    root->addWidget(m_selectionLabel);
    root->addWidget(m_canvasStack, 1);

    connect(m_topButton, &QToolButton::clicked, this, [this]()
    {
        SetMode(HostViewMode::Top);
    });
    connect(m_threeDButton, &QToolButton::clicked, this, [this]()
    {
        SetMode(HostViewMode::ThreeD);
    });
    ApplyMode();
}

void ViewWorkspaceWidget::SetMode(const HostViewMode mode)
{
    m_switch.SetMode(mode);
    ApplyMode();
}

HostViewMode ViewWorkspaceWidget::Mode() const
{
    return m_switch.Mode();
}

void ViewWorkspaceWidget::ShowViewError(const QString& message)
{
    m_errorLabel->setText(message);
    m_errorLabel->setVisible(!message.isEmpty());
}

void ViewWorkspaceWidget::SetSelectedInstances(
    const QStringList& instanceIds)
{
    if (instanceIds.isEmpty())
    {
        m_selectionLabel->setText(QStringLiteral("未选择模型实例。"));
        return;
    }
    m_selectionLabel->setText(
        instanceIds.size() == 1
            ? QStringLiteral("当前模型：%1").arg(instanceIds.front())
            : QStringLiteral("已选择 %1 个模型：%2")
                  .arg(instanceIds.size())
                  .arg(instanceIds.join(QStringLiteral("、"))));
}

void ViewWorkspaceWidget::SetTopImage(const QImage& image)
{
    m_topCanvas->SetImage(image);
}

void ViewWorkspaceWidget::ClearTopImage()
{
    m_topCanvas->ClearImage();
}

QSize ViewWorkspaceWidget::TopRenderSize() const
{
    const QSize canvasSize = m_topCanvas->size();
    return {
        (std::max)(canvasSize.width(), 800),
        (std::max)(canvasSize.height(), 480)};
}

TopViewCanvasWidget* ViewWorkspaceWidget::TopCanvas() const
{
    return m_topCanvas;
}

void ViewWorkspaceWidget::ApplyMode()
{
    const bool top = m_switch.Mode() == HostViewMode::Top;
    m_topButton->setChecked(top);
    m_threeDButton->setChecked(!top);
    m_canvasStack->setCurrentIndex(top ? 0 : 1);
}
