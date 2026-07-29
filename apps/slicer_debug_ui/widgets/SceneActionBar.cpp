#include "SceneActionBar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>

namespace
{

void SetElidedLabelText(
    QLabel* label,
    const QString& prefix,
    const QString& value)
{
    const QString fullText = prefix + value;
    const int textWidth =
        label->maximumWidth() > 0
        ? label->maximumWidth()
        : label->width();
    label->setText(
        label->fontMetrics().elidedText(
            fullText,
            Qt::ElideRight,
            textWidth));
    label->setToolTip(fullText);
}

}  // namespace

SceneActionBar::SceneActionBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sceneActionBar"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(8);

    m_importButton = new QPushButton(
        QStringLiteral("导入模型"),
        this);
    m_importButton->setObjectName(
        QStringLiteral("jobImportModelsButton"));
    m_importButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_importButton->setMinimumSize(110, 36);
    m_importButton->setToolTip(
        QStringLiteral("导入一个或多个 OBJ、STL、3MF 模型"));
    m_importButton->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+O")));

    m_saveButton = new QPushButton(
        QStringLiteral("保存场景"),
        this);
    m_saveButton->setObjectName(
        QStringLiteral("jobSaveSceneButton"));
    m_saveButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveButton->setMinimumSize(104, 36);
    m_saveButton->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+S")));

    m_modeLabel = new QLabel(this);
    m_modeLabel->setObjectName(
        QStringLiteral("jobModeSummaryLabel"));
    m_modeLabel->setMinimumWidth(128);
    m_modeLabel->setSizePolicy(
        QSizePolicy::Minimum,
        QSizePolicy::Fixed);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setObjectName(
        QStringLiteral("jobProfileSelector"));
    m_profileCombo->setMinimumWidth(180);
    m_profileCombo->setMaximumWidth(300);
    m_profileCombo->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_profileCombo->setMinimumContentsLength(20);
    m_profileCombo->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Fixed);
    m_profileCombo->setToolTip(
        QStringLiteral(
            "快速切换生产工艺 Profile。该控件会更新生产配置；"
            "右侧“诊断试算”中的材料和宽度不会修改生产切片。"));

    m_sliceButton = new QPushButton(
        QStringLiteral("切片当前场景"),
        this);
    m_sliceButton->setObjectName(
        QStringLiteral("sliceCurrentSceneButton"));
    m_sliceButton->setIcon(
        style()->standardIcon(QStyle::SP_MediaPlay));
    m_sliceButton->setMinimumHeight(36);
    m_sliceButton->setMinimumWidth(148);
    m_sliceButton->setSizePolicy(
        QSizePolicy::Minimum,
        QSizePolicy::Fixed);
    m_sliceButton->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Return")));

    m_cancelButton = new QPushButton(this);
    m_cancelButton->setObjectName(
        QStringLiteral("cancelCurrentSceneSliceButton"));
    m_cancelButton->setIcon(
        style()->standardIcon(QStyle::SP_BrowserStop));
    m_cancelButton->setToolTip(
        QStringLiteral("取消当前场景切片"));
    m_cancelButton->setFixedSize(36, 36);
    m_cancelButton->setShortcut(
        QKeySequence(Qt::Key_Escape));

    m_statusLabel = new QLabel(
        QStringLiteral("待切片"),
        this);
    m_statusLabel->setObjectName(
        QStringLiteral("sceneSliceActionStateLabel"));
    m_statusLabel->setMinimumWidth(150);
    m_statusLabel->setMaximumWidth(300);
    m_statusLabel->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Fixed);

    layout->addWidget(m_importButton);
    layout->addWidget(m_saveButton);
    layout->addSpacing(8);
    layout->addWidget(m_modeLabel);
    layout->addWidget(m_profileCombo, 1);
    layout->addWidget(m_sliceButton);
    layout->addWidget(m_cancelButton);
    layout->addWidget(m_statusLabel);

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &SceneActionBar::SigImportRequested);
    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &SceneActionBar::SigSaveRequested);
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
    connect(
        m_profileCombo,
        qOverload<int>(&QComboBox::activated),
        this,
        [this](const int index)
        {
            emit SigProfileRequested(
                m_profileCombo->itemData(index).toString());
        });
    SceneActionBarPresentation presentation;
    presentation.modelabel = QStringLiteral("传统切片");
    presentation.profilelabel = QStringLiteral("自定义");
    presentation.statustext = QStringLiteral("待导入");
    presentation.savereason = QStringLiteral("请先导入模型。");
    presentation.slicereason = QStringLiteral("请先导入模型。");
    SetPresentation(presentation);
}

void SceneActionBar::SetPresentation(
    const SceneActionBarPresentation& presentation)
{
    m_importButton->setEnabled(presentation.canimport);
    m_saveButton->setEnabled(presentation.cansave);
    m_saveButton->setToolTip(presentation.savereason);
    m_sliceButton->setEnabled(presentation.canslice);
    m_sliceButton->setToolTip(presentation.slicereason);
    m_cancelButton->setEnabled(presentation.cancancel);
    m_profileCombo->setEnabled(
        presentation.canselectprofile);
    SetElidedLabelText(
        m_modeLabel,
        QStringLiteral("模式："),
        presentation.modelabel);
    SetSelectedProfileId(
        presentation.profilelabel);
    SetElidedLabelText(
        m_statusLabel,
        QString{},
        presentation.statustext);
    m_statusLabel->setToolTip(
        presentation.statustext
        + QStringLiteral("\n")
        + presentation.slicereason);
}

void SceneActionBar::SetProfileOptions(
    const QList<SceneActionBarProfileOption>& options,
    const QString& selectedProfileId)
{
    const QSignalBlocker blocker(m_profileCombo);
    m_profileCombo->clear();
    m_profileCombo->addItem(
        QStringLiteral("自定义配置"),
        QString{});
    m_profileCombo->setItemData(
        0,
        QStringLiteral(
            "保留当前手动配置；完整路径和高级场景位于“视图 -> 项目与高级工具”。"),
        Qt::ToolTipRole);
    for (const SceneActionBarProfileOption& option : options)
    {
        const int index = m_profileCombo->count();
        m_profileCombo->addItem(
            option.label,
            option.id);
        m_profileCombo->setItemData(
            index,
            option.tooltip,
            Qt::ToolTipRole);
    }
    SetSelectedProfileId(selectedProfileId);
}

void SceneActionBar::SetSelectedProfileId(
    const QString& profileId)
{
    const QSignalBlocker blocker(m_profileCombo);
    const QString normalizedProfileId =
        profileId == QStringLiteral("自定义")
        ? QString{}
        : profileId;
    int index =
        m_profileCombo->findData(normalizedProfileId);
    if (index < 0
        && !normalizedProfileId.trimmed().isEmpty())
    {
        m_profileCombo->insertItem(
            0,
            normalizedProfileId,
            normalizedProfileId);
        m_profileCombo->setItemData(
            0,
            QStringLiteral("当前生产 Profile：")
                + normalizedProfileId,
            Qt::ToolTipRole);
        index = 0;
    }
    if (index < 0)
    {
        index = 0;
    }
    m_profileCombo->setCurrentIndex(index);
    const QString tooltip =
        m_profileCombo->itemData(
            index,
            Qt::ToolTipRole).toString();
    if (!tooltip.isEmpty())
    {
        m_profileCombo->setToolTip(tooltip);
    }
}

QString SceneActionBar::SelectedProfileId() const
{
    return m_profileCombo->currentData().toString();
}

QPushButton* SceneActionBar::SliceButton() const
{
    return m_sliceButton;
}
