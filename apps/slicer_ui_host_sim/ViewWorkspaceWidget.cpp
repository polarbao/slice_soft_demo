#include "ViewWorkspaceWidget.h"

#include "ThreeDCanvasWidget.h"
#include "TopViewCanvasWidget.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

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

    m_threeDControls = new QWidget(this);
    auto* cameraLayout = new QHBoxLayout(m_threeDControls);
    cameraLayout->setContentsMargins(0, 0, 0, 0);
    cameraLayout->setSpacing(6);
    m_presetCombo = new QComboBox(m_threeDControls);
    m_presetCombo->setObjectName(QStringLiteral("threeDCameraPresetCombo"));
    m_presetCombo->setToolTip(QStringLiteral("选择 3D 相机方向"));
    m_presetCombo->addItem(QStringLiteral("顶"), static_cast<int>(CameraPreset::Top));
    m_presetCombo->addItem(QStringLiteral("底"), static_cast<int>(CameraPreset::Bottom));
    m_presetCombo->addItem(QStringLiteral("前"), static_cast<int>(CameraPreset::Front));
    m_presetCombo->addItem(QStringLiteral("后"), static_cast<int>(CameraPreset::Back));
    m_presetCombo->addItem(QStringLiteral("左"), static_cast<int>(CameraPreset::Left));
    m_presetCombo->addItem(QStringLiteral("右"), static_cast<int>(CameraPreset::Right));
    m_presetCombo->addItem(
        QStringLiteral("等轴"), static_cast<int>(CameraPreset::Isometric));
    m_presetCombo->setCurrentIndex(6);
    m_projectionCombo = new QComboBox(m_threeDControls);
    m_projectionCombo->setObjectName(
        QStringLiteral("threeDToolbarProjectionCombo"));
    m_projectionCombo->setToolTip(QStringLiteral("切换 3D 投影方式"));
    m_projectionCombo->addItem(
        QStringLiteral("正交"),
        static_cast<int>(slicer::render::Projection::Orthographic));
    m_projectionCombo->addItem(
        QStringLiteral("透视"),
        static_cast<int>(slicer::render::Projection::Perspective));
    auto* fitButton = new QToolButton(m_threeDControls);
    fitButton->setObjectName(QStringLiteral("threeDFitButton"));
    fitButton->setText(QStringLiteral("适配"));
    fitButton->setToolTip(QStringLiteral("适配全部 3D 模型"));
    cameraLayout->addWidget(m_presetCombo);
    cameraLayout->addWidget(m_projectionCombo);
    cameraLayout->addWidget(fitButton);
    toolbar->addWidget(m_threeDControls);

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
    m_threeDCanvas = new ThreeDCanvasWidget(m_canvasStack);
    m_threeDCanvas->setObjectName(QStringLiteral("threeDCanvas"));
    m_canvasStack->addWidget(m_topCanvas);
    m_canvasStack->addWidget(m_threeDCanvas);

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
    connect(
        m_presetCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this](int)
        {
            m_threeDCanvas->SetPreset(static_cast<CameraPreset>(
                m_presetCombo->currentData().toInt()));
        });
    connect(
        m_projectionCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this](int)
        {
            m_threeDCanvas->SetProjection(
                static_cast<slicer::render::Projection>(
                    m_projectionCombo->currentData().toInt()));
        });
    connect(fitButton, &QToolButton::clicked, this, [this]()
    {
        m_threeDCanvas->FitScene();
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

void ViewWorkspaceWidget::SetThreeDImage(const QImage& image)
{
    m_threeDCanvas->SetImage(image);
}

void ViewWorkspaceWidget::ClearThreeDImage()
{
    m_threeDCanvas->ClearImage();
}

void ViewWorkspaceWidget::SetThreeDSceneBounds(const CameraBounds& bounds)
{
    m_threeDCanvas->SetSceneBounds(bounds);
}

QSize ViewWorkspaceWidget::ThreeDRenderSize() const
{
    return m_threeDCanvas->RenderSize();
}

ThreeDCanvasWidget* ViewWorkspaceWidget::ThreeDCanvas() const
{
    return m_threeDCanvas;
}

void ViewWorkspaceWidget::SetThreeDProjection(
    const slicer::render::Projection projection)
{
    const int index = m_projectionCombo->findData(static_cast<int>(projection));
    if (index >= 0 && index != m_projectionCombo->currentIndex())
    {
        m_projectionCombo->setCurrentIndex(index);
        return;
    }
    m_threeDCanvas->SetProjection(projection);
}

void ViewWorkspaceWidget::ApplyMode()
{
    const bool top = m_switch.Mode() == HostViewMode::Top;
    m_topButton->setChecked(top);
    m_threeDButton->setChecked(!top);
    m_canvasStack->setCurrentIndex(top ? 0 : 1);
    m_threeDControls->setVisible(!top);
}
