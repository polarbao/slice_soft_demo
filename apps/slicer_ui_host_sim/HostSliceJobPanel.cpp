#include "HostSliceJobPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

HostSliceJobPanel::HostSliceJobPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildInterface();
    SetReady(false, QStringLiteral("请先选择可切片 Profile 并导入模型。"));
}

void HostSliceJobPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("hostSliceJobStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_phaseLabel = new QLabel(this);
    m_phaseLabel->setObjectName(QStringLiteral("hostSliceJobPhaseLabel"));
    m_phaseLabel->setWordWrap(true);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("hostSliceJobProgressBar"));
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    auto* commands = new QHBoxLayout();
    m_startButton = new QPushButton(QStringLiteral("开始切片"), this);
    m_startButton->setObjectName(QStringLiteral("hostSliceStartButton"));
    m_startButton->setToolTip(QStringLiteral(
        "提交当前已 Commit 场景与有效 Profile，不会静默切换引擎"));
    m_cancelButton = new QPushButton(QStringLiteral("取消作业"), this);
    m_cancelButton->setObjectName(QStringLiteral("hostSliceCancelButton"));
    m_cancelButton->setToolTip(QStringLiteral(
        "协作式取消 Worker 作业并等待 staging 安全清理"));
    commands->addWidget(m_startButton);
    commands->addWidget(m_cancelButton);

    m_detailView = new QPlainTextEdit(this);
    m_detailView->setObjectName(QStringLiteral("hostSliceJobDetailView"));
    m_detailView->setReadOnly(true);
    m_detailView->setPlaceholderText(QStringLiteral(
        "终结状态、错误码和输出目录将在这里显示。"));

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_phaseLabel);
    layout->addWidget(m_progressBar);
    layout->addLayout(commands);
    layout->addWidget(m_detailView, 1);

    connect(
        m_startButton,
        &QPushButton::clicked,
        this,
        &HostSliceJobPanel::OnStartRequested);
    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &HostSliceJobPanel::OnCancelRequested);
}

void HostSliceJobPanel::SetReady(
    const bool ready,
    const QString& reason)
{
    m_ready = ready;
    if (!m_active)
    {
        m_statusLabel->setText(
            ready ? QStringLiteral("切片作业已就绪") : reason);
    }
    UpdateButtons();
}

void HostSliceJobPanel::SetActive()
{
    m_active = true;
    m_progressBar->setValue(0);
    m_statusLabel->setText(QStringLiteral("切片作业已提交"));
    m_phaseLabel->setText(QStringLiteral("排队中"));
    m_detailView->clear();
    UpdateButtons();
}

void HostSliceJobPanel::UpdateProgress(
    const QString& state,
    const QString& phase,
    const int current,
    const int total,
    const int percent,
    const qint64 elapsedMs)
{
    m_progressBar->setValue(percent);
    m_statusLabel->setText(QStringLiteral("状态：%1 · %2%")
                               .arg(state)
                               .arg(percent));
    m_phaseLabel->setText(
        QStringLiteral("阶段：%1 · %2/%3 · %4 ms")
            .arg(phase.isEmpty() ? QStringLiteral("unknown") : phase)
            .arg(current)
            .arg(total)
            .arg(elapsedMs));
}

void HostSliceJobPanel::ShowCompletion(
    const bool success,
    const bool cancelled,
    const QString& code,
    const QString& message,
    const QString& detail,
    const QString& packageDirectory,
    const qint64 elapsedMs,
    const qint64 cancelLatencyMs)
{
    m_active = false;
    m_progressBar->setValue(success || cancelled ? 100 : m_progressBar->value());
    m_statusLabel->setText(
        success ? QStringLiteral("切片完成")
                : cancelled ? QStringLiteral("切片已取消")
                            : QStringLiteral("切片失败"));
    QStringList details{
        QStringLiteral("code=%1").arg(
            code.isEmpty() ? QStringLiteral("未返回") : code),
        QStringLiteral("elapsedMs=%1").arg(elapsedMs)};
    if (cancelLatencyMs >= 0)
    {
        details.append(QStringLiteral("cancelLatencyMs=%1")
                           .arg(cancelLatencyMs));
    }
    if (!packageDirectory.isEmpty())
    {
        details.append(QStringLiteral("packageDir=%1").arg(packageDirectory));
    }
    if (!message.isEmpty())
    {
        details.append(QStringLiteral("message=%1").arg(message));
    }
    if (!detail.isEmpty())
    {
        details.append(QStringLiteral("detail=%1").arg(detail));
    }
    m_detailView->setPlainText(details.join(QLatin1Char('\n')));
    UpdateButtons();
}

void HostSliceJobPanel::OnStartRequested()
{
    if (m_ready && !m_active)
    {
        emit SigStartRequested();
    }
}

void HostSliceJobPanel::OnCancelRequested()
{
    if (m_active)
    {
        m_cancelButton->setEnabled(false);
        emit SigCancelRequested();
    }
}

void HostSliceJobPanel::UpdateButtons()
{
    m_startButton->setEnabled(m_ready && !m_active);
    m_cancelButton->setEnabled(m_active);
}
