#include "ModelTransformPanel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{

QDoubleSpinBox* CreateSpinBox(
    QWidget* parent,
    const QString& objectName,
    const double minimum,
    const double maximum,
    const int decimals,
    const double step,
    const QString& suffix,
    const QString& tooltip)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    spin->setToolTip(tooltip);
    return spin;
}

QString PreflightStatusText(
    const slicer_core::ModelPreflightStatus status)
{
    switch (status)
    {
    case slicer_core::ModelPreflightStatus::NotRun:
        return QStringLiteral("未运行");
    case slicer_core::ModelPreflightStatus::Pending:
        return QStringLiteral("等待");
    case slicer_core::ModelPreflightStatus::Running:
        return QStringLiteral("检测中");
    case slicer_core::ModelPreflightStatus::Passed:
        return QStringLiteral("通过");
    case slicer_core::ModelPreflightStatus::Warning:
        return QStringLiteral("警告");
    case slicer_core::ModelPreflightStatus::Blocked:
        return QStringLiteral("阻断");
    case slicer_core::ModelPreflightStatus::Stale:
        return QStringLiteral("过期");
    case slicer_core::ModelPreflightStatus::Cancelled:
        return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}

QString AdmissionText(
    const slicer_core::ModelPreflightAdmissionStatus status)
{
    switch (status)
    {
    case slicer_core::ModelPreflightAdmissionStatus::Passed:
        return QStringLiteral("通过");
    case slicer_core::ModelPreflightAdmissionStatus::Warning:
        return QStringLiteral("警告");
    case slicer_core::ModelPreflightAdmissionStatus::Blocked:
        return QStringLiteral("阻断");
    }
    return QStringLiteral("未知");
}

}  // namespace

ModelTransformPanel::ModelTransformPanel(
    SceneDocument* document,
    SceneSelectionModel* selectionModel,
    SceneTransformController* controller,
    QWidget* parent)
    : QWidget(parent),
      m_document(document),
      m_selectionModel(selectionModel),
      m_controller(controller)
{
    Q_ASSERT(m_document != nullptr);
    Q_ASSERT(m_selectionModel != nullptr);
    Q_ASSERT(m_controller != nullptr);

    setObjectName(QStringLiteral("modelTransformPanel"));
    setMinimumWidth(230);
    setMaximumWidth(310);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    auto* title = new QLabel(QStringLiteral("精确变换"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    m_identityLabel = new QLabel(QStringLiteral("未加载模型"), this);
    m_identityLabel->setObjectName(
        QStringLiteral("modelTransformIdentity"));
    m_identityLabel->setWordWrap(true);
    layout->addWidget(m_identityLabel);

    m_revisionLabel = new QLabel(QStringLiteral("revision --"), this);
    m_revisionLabel->setObjectName(
        QStringLiteral("modelTransformRevision"));
    m_revisionLabel->setWordWrap(true);
    layout->addWidget(m_revisionLabel);

    auto* form = new QFormLayout();
    m_translateXSpin = CreateSpinBox(
        this,
        QStringLiteral("modelTransformTranslateX"),
        -10000.0,
        10000.0,
        2,
        0.10,
        QStringLiteral(" mm"),
        QStringLiteral("模型实例沿 X 方向平移；可键入 0.01 mm。"));
    m_translateYSpin = CreateSpinBox(
        this,
        QStringLiteral("modelTransformTranslateY"),
        -10000.0,
        10000.0,
        2,
        0.10,
        QStringLiteral(" mm"),
        QStringLiteral("模型实例沿 Y 方向平移；可键入 0.01 mm。"));
    m_rotateZSpin = CreateSpinBox(
        this,
        QStringLiteral("modelTransformRotateZ"),
        -180.0,
        180.0,
        2,
        1.0,
        QStringLiteral(" °"),
        QStringLiteral("围绕源包围盒中心和最低 Z 轴点旋转。"));
    m_uniformScaleSpin = CreateSpinBox(
        this,
        QStringLiteral("modelTransformUniformScale"),
        0.01,
        100.0,
        4,
        0.01,
        {},
        QStringLiteral("统一缩放，不改变源模型文件。"));
    form->addRow(QStringLiteral("X"), m_translateXSpin);
    form->addRow(QStringLiteral("Y"), m_translateYSpin);
    form->addRow(QStringLiteral("绕 Z"), m_rotateZSpin);
    form->addRow(QStringLiteral("统一缩放"), m_uniformScaleSpin);
    layout->addLayout(form);

    m_applyButton = new QPushButton(QStringLiteral("应用"), this);
    m_applyButton->setObjectName(
        QStringLiteral("modelTransformApplyButton"));
    m_applyButton->setToolTip(
        QStringLiteral("原子提交四项数值并异步刷新俯视几何"));
    layout->addWidget(m_applyButton);

    auto* commandRow = new QHBoxLayout();
    m_centerButton = new QPushButton(QStringLiteral("原点居中"), this);
    m_centerButton->setObjectName(
        QStringLiteral("modelTransformCenterButton"));
    m_centerButton->setToolTip(
        QStringLiteral("将当前 XY 包围盒中心移动到软件场景原点"));
    m_resetButton = new QPushButton(QStringLiteral("重置"), this);
    m_resetButton->setObjectName(
        QStringLiteral("modelTransformResetButton"));
    m_resetButton->setToolTip(
        QStringLiteral("恢复实例 identity；不撤销模型自动姿态"));
    commandRow->addWidget(m_centerButton);
    commandRow->addWidget(m_resetButton);
    layout->addLayout(commandRow);

    auto* mirrorRow = new QHBoxLayout();
    m_mirrorXButton = new QPushButton(QStringLiteral("镜像 X"), this);
    m_mirrorXButton->setObjectName(
        QStringLiteral("modelTransformMirrorXButton"));
    m_mirrorXButton->setCheckable(true);
    m_mirrorXButton->setToolTip(
        QStringLiteral("沿模型源包围盒中心镜像 X，不修改源文件"));
    m_mirrorYButton = new QPushButton(QStringLiteral("镜像 Y"), this);
    m_mirrorYButton->setObjectName(
        QStringLiteral("modelTransformMirrorYButton"));
    m_mirrorYButton->setCheckable(true);
    m_mirrorYButton->setToolTip(
        QStringLiteral("沿模型源包围盒中心镜像 Y，不修改源文件"));
    mirrorRow->addWidget(m_mirrorXButton);
    mirrorRow->addWidget(m_mirrorYButton);
    layout->addLayout(mirrorRow);

    m_saveButton = new QPushButton(QStringLiteral("保存场景配置"), this);
    m_saveButton->setObjectName(
        QStringLiteral("modelTransformSaveButton"));
    m_saveButton->setToolTip(
        QStringLiteral("保存到 UI session，不覆盖源 Profile 或模型文件"));
    layout->addWidget(m_saveButton);

    m_sourcePreflightLabel = new QLabel(
        QStringLiteral("源模型预检：未运行"),
        this);
    m_sourcePreflightLabel->setObjectName(
        QStringLiteral("modelTransformSourcePreflight"));
    m_sourcePreflightLabel->setWordWrap(true);
    layout->addWidget(m_sourcePreflightLabel);

    m_transformedPreflightLabel = new QLabel(
        QStringLiteral("变换后预检：未运行"),
        this);
    m_transformedPreflightLabel->setObjectName(
        QStringLiteral("modelTransformEffectivePreflight"));
    m_transformedPreflightLabel->setWordWrap(true);
    layout->addWidget(m_transformedPreflightLabel);

    m_stateLabel = new QLabel(QStringLiteral("请选择模型。"), this);
    m_stateLabel->setObjectName(
        QStringLiteral("modelTransformState"));
    m_stateLabel->setWordWrap(true);
    layout->addWidget(m_stateLabel);
    layout->addStretch(1);

    connect(
        m_applyButton,
        &QPushButton::clicked,
        this,
        &ModelTransformPanel::OnApply);
    connect(
        m_centerButton,
        &QPushButton::clicked,
        this,
        &ModelTransformPanel::OnCenter);
    connect(
        m_resetButton,
        &QPushButton::clicked,
        this,
        &ModelTransformPanel::OnReset);
    connect(
        m_mirrorXButton,
        &QPushButton::clicked,
        this,
        &ModelTransformPanel::OnMirrorX);
    connect(
        m_mirrorYButton,
        &QPushButton::clicked,
        this,
        &ModelTransformPanel::OnMirrorY);
    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &ModelTransformPanel::SigSaveRequested);
    connect(
        m_document,
        &SceneDocument::SigChanged,
        this,
        &ModelTransformPanel::OnDocumentChanged);
    connect(
        m_selectionModel,
        &SceneSelectionModel::SigSelectionChanged,
        this,
        &ModelTransformPanel::OnSelectionChanged);
    connect(
        m_controller,
        &SceneTransformController::SigCommandFailed,
        this,
        &ModelTransformPanel::OnCommandFailed);

    SyncFields();
    UpdateAvailability();
}

void ModelTransformPanel::OnApply()
{
    if (!m_document->Instance().has_value())
    {
        return;
    }
    slicer_core::ModelTransform transform =
        m_document->Instance()->transform;
    transform.translatexmm = m_translateXSpin->value();
    transform.translateymm = m_translateYSpin->value();
    transform.rotatezdeg = m_rotateZSpin->value();
    transform.uniformscale = m_uniformScaleSpin->value();
    ShowCommandResult(
        m_controller->SetTransform(
            transform,
            m_document->SceneRevision(),
            m_document->Instance()->transformrevision),
        QStringLiteral("变换已提交，正在刷新俯视几何。"));
}

void ModelTransformPanel::OnCenter()
{
    if (!m_document->Instance().has_value())
    {
        return;
    }
    ShowCommandResult(
        m_controller->CenterAtSceneOrigin(
            m_document->SceneRevision(),
            m_document->Instance()->transformrevision),
        QStringLiteral("已按软件场景原点居中。"));
}

void ModelTransformPanel::OnReset()
{
    if (!m_document->Instance().has_value())
    {
        return;
    }
    ShowCommandResult(
        m_controller->ResetTransform(
            m_document->SceneRevision(),
            m_document->Instance()->transformrevision),
        QStringLiteral("实例变换已重置。"));
}

void ModelTransformPanel::OnMirrorX()
{
    if (!m_document->Instance().has_value())
    {
        return;
    }
    slicer_core::ModelTransform transform =
        m_document->Instance()->transform;
    transform.mirrorx = m_mirrorXButton->isChecked();
    ShowCommandResult(
        m_controller->SetTransform(
            transform,
            m_document->SceneRevision(),
            m_document->Instance()->transformrevision),
        QStringLiteral("X 镜像已提交，正在重新投影和预检。"));
    SyncFields();
}

void ModelTransformPanel::OnMirrorY()
{
    if (!m_document->Instance().has_value())
    {
        return;
    }
    slicer_core::ModelTransform transform =
        m_document->Instance()->transform;
    transform.mirrory = m_mirrorYButton->isChecked();
    ShowCommandResult(
        m_controller->SetTransform(
            transform,
            m_document->SceneRevision(),
            m_document->Instance()->transformrevision),
        QStringLiteral("Y 镜像已提交，正在重新投影和预检。"));
    SyncFields();
}

void ModelTransformPanel::OnDocumentChanged()
{
    SyncFields();
    UpdateAvailability();
}

void ModelTransformPanel::OnSelectionChanged(const QString& instanceId)
{
    Q_UNUSED(instanceId);
    SyncFields();
    UpdateAvailability();
}

void ModelTransformPanel::OnCommandFailed(
    const QString& code,
    const QString& message)
{
    m_stateLabel->setText(code + QStringLiteral("：") + message);
    emit SigStatusMessage(m_stateLabel->text());
}

void ModelTransformPanel::SyncFields()
{
    if (!m_document->Instance().has_value())
    {
        m_identityLabel->setText(QStringLiteral("未加载模型"));
        m_revisionLabel->setText(QStringLiteral("revision --"));
        m_sourcePreflightLabel->setText(
            QStringLiteral("源模型预检：未运行"));
        m_transformedPreflightLabel->setText(
            QStringLiteral("变换后预检：未运行"));
        m_mirrorXButton->setChecked(false);
        m_mirrorYButton->setChecked(false);
        return;
    }

    const slicer_core::ModelInstance& instance =
        m_document->Instance().value();
    m_identityLabel->setText(
        QStringLiteral("实例：%1")
            .arg(QString::fromStdString(instance.instanceid)));
    m_revisionLabel->setText(
        QStringLiteral("scene=%1，transform=%2，%3%4")
            .arg(m_document->SceneRevision())
            .arg(instance.transformrevision)
            .arg(instance.locked ? QStringLiteral("已锁定，")
                                 : QString())
            .arg(m_document->IsDirty()
                     || m_document->EffectiveConfigPath().isEmpty()
                 ? QStringLiteral("未保存")
                 : QStringLiteral("已同步")));

    if (!m_translateXSpin->hasFocus()
        && !m_translateYSpin->hasFocus()
        && !m_rotateZSpin->hasFocus()
        && !m_uniformScaleSpin->hasFocus())
    {
        m_translateXSpin->setValue(instance.transform.translatexmm);
        m_translateYSpin->setValue(instance.transform.translateymm);
        m_rotateZSpin->setValue(instance.transform.rotatezdeg);
        m_uniformScaleSpin->setValue(instance.transform.uniformscale);
    }
    m_mirrorXButton->setChecked(instance.transform.mirrorx);
    m_mirrorYButton->setChecked(instance.transform.mirrory);

    switch (m_document->TransformedPreflightState())
    {
    case SceneTransformedPreflightState::NotRun:
        m_sourcePreflightLabel->setText(
            QStringLiteral("源模型预检：未运行"));
        m_transformedPreflightLabel->setText(
            QStringLiteral("变换后预检：未运行"));
        break;
    case SceneTransformedPreflightState::Running:
        m_sourcePreflightLabel->setText(
            QStringLiteral("源模型预检：检测中"));
        m_transformedPreflightLabel->setText(
            QStringLiteral("变换后预检：检测中"));
        break;
    case SceneTransformedPreflightState::Ready:
        if (m_document->TransformedPreflight().has_value())
        {
            const auto& preflight =
                m_document->TransformedPreflight().value();
            m_sourcePreflightLabel->setText(
                QStringLiteral("源模型预检：%1")
                    .arg(PreflightStatusText(
                        preflight.source.result.status)));
            m_transformedPreflightLabel->setText(
                QStringLiteral(
                    "变换后预检：%1；Legacy=%2，Global=%3")
                    .arg(PreflightStatusText(
                        preflight.transformed.result.status))
                    .arg(AdmissionText(
                        preflight.transformed.result
                            .legacyAdmission.status))
                    .arg(AdmissionText(
                        preflight.transformed.result
                            .globalAdmission.status)));
        }
        break;
    case SceneTransformedPreflightState::Failed:
        m_sourcePreflightLabel->setText(
            QStringLiteral("源模型预检：失败"));
        m_transformedPreflightLabel->setText(
            QStringLiteral("变换后预检：失败"));
        break;
    case SceneTransformedPreflightState::Cancelled:
        m_sourcePreflightLabel->setText(
            QStringLiteral("源模型预检：已取消"));
        m_transformedPreflightLabel->setText(
            QStringLiteral("变换后预检：已取消"));
        break;
    case SceneTransformedPreflightState::Stale:
        m_sourcePreflightLabel->setText(
            QStringLiteral("源模型预检：过期"));
        m_transformedPreflightLabel->setText(
            QStringLiteral("变换后预检：等待重检"));
        break;
    }
}

void ModelTransformPanel::UpdateAvailability()
{
    const bool hasInstance = m_document->Instance().has_value();
    const bool selected = hasInstance
        && m_selectionModel->SelectedInstance()
            == QString::fromStdString(
                m_document->Instance()->instanceid);
    const bool editable =
        selected && !m_document->Instance()->locked;
    m_translateXSpin->setEnabled(editable);
    m_translateYSpin->setEnabled(editable);
    m_rotateZSpin->setEnabled(editable);
    m_uniformScaleSpin->setEnabled(editable);
    m_applyButton->setEnabled(editable);
    m_resetButton->setEnabled(editable);
    m_mirrorXButton->setEnabled(editable);
    m_mirrorYButton->setEnabled(editable);
    m_centerButton->setEnabled(
        editable
        && m_document->Geometry().has_value()
        && !m_document->IsGeometryStale());
    m_saveButton->setEnabled(
        editable
        && !m_document->IsGeometryStale());

    if (!hasInstance)
    {
        m_stateLabel->setText(QStringLiteral("请先导入模型。"));
    }
    else if (!selected)
    {
        m_stateLabel->setText(QStringLiteral("请在俯视画布中选择模型。"));
    }
    else if (m_document->Instance()->locked)
    {
        m_stateLabel->setText(QStringLiteral("当前模型已锁定。"));
    }
    else if (m_document->IsGeometryStale())
    {
        m_stateLabel->setText(QStringLiteral("俯视几何正在刷新或已过期。"));
    }
    else if (m_document->IsDirty()
             || m_document->EffectiveConfigPath().isEmpty())
    {
        m_stateLabel->setText(QStringLiteral("变换尚未保存到场景配置。"));
    }
    else
    {
        m_stateLabel->setText(QStringLiteral("变换状态已同步。"));
    }
}

void ModelTransformPanel::ShowCommandResult(
    const SceneTransformCommandResult& result,
    const QString& successMessage)
{
    if (!result.IsValid())
    {
        return;
    }
    if (!result.changed)
    {
        m_stateLabel->setText(QStringLiteral("变换未发生变化。"));
    }
    else
    {
        m_stateLabel->setText(successMessage);
    }
    emit SigStatusMessage(m_stateLabel->text());
}
