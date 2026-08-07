#include "HostMainWindow.h"

#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QWidget>

HostMainWindow::HostMainWindow(
    const QString& modulePath,
    QWidget* parent)
    : QMainWindow(parent)
{
    BuildInterface();
    LoadModule(modulePath);
}

void HostMainWindow::BuildInterface()
{
    setWindowTitle(QStringLiteral("SliceSoft 打印宿主参考实现"));
    resize(900, 560);

    auto* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    m_statusLabel = new QLabel(centralWidget);
    m_statusLabel->setObjectName(QStringLiteral("moduleStatusLabel"));
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_pathLabel = new QLabel(centralWidget);
    m_pathLabel->setObjectName(QStringLiteral("modulePathLabel"));
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setWordWrap(true);

    m_moduleInfoView = new QPlainTextEdit(centralWidget);
    m_moduleInfoView->setObjectName(QStringLiteral("moduleInfoView"));
    m_moduleInfoView->setReadOnly(true);
    m_moduleInfoView->setFont(QFontDatabase::systemFont(
        QFontDatabase::FixedFont));

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_pathLabel);
    layout->addWidget(m_moduleInfoView, 1);
    setCentralWidget(centralWidget);
}

void HostMainWindow::LoadModule(const QString& modulePath)
{
    m_pathLabel->setText(QStringLiteral("模块：%1").arg(modulePath));

    QString error;
    if (!m_client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        m_statusLabel->setText(QStringLiteral("模块不可用"));
        m_moduleInfoView->setPlainText(error);
        return;
    }

    QByteArray selfTestReport;
    if (!m_client.SelfTest(&selfTestReport, &error))
    {
        m_statusLabel->setText(QStringLiteral("模块自检失败"));
        m_moduleInfoView->setPlainText(error);
        return;
    }

    m_statusLabel->setText(
        QStringLiteral("模块已就绪 · SPI v%1 · ABI 调用 %2 次")
            .arg(PM_SPI_VERSION)
            .arg(m_client.CallCount()));
    m_moduleInfoView->setPlainText(
        QStringLiteral("模块信息\n%1\n\n自检报告\n%2")
            .arg(
                QString::fromUtf8(m_client.ModuleInfo()),
                QString::fromUtf8(selfTestReport)));
}
