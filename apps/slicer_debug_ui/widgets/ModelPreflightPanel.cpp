#include "ModelPreflightPanel.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ModelPreflightPanel::ModelPreflightPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("modelPreflightPanel"));
    auto* layout = new QVBoxLayout(this);
    auto* stateRow = new QHBoxLayout();

    m_stateLabel = new QLabel(QStringLiteral("待检测"), this);
    m_stateLabel->setObjectName(QStringLiteral("modelPreflightState"));
    m_stateLabel->setWordWrap(true);
    m_modeLabel = new QLabel(QStringLiteral("传统切片"), this);
    m_modeLabel->setObjectName(QStringLiteral("modelPreflightMode"));
    m_modeLabel->setWordWrap(true);
    m_recheckButton = new QPushButton(this);
    m_recheckButton->setObjectName(QStringLiteral("modelPreflightRecheck"));
    m_recheckButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_recheckButton->setToolTip(QStringLiteral("重新执行模型预检"));
    m_recheckButton->setFixedSize(30, 30);
    m_cancelButton = new QPushButton(this);
    m_cancelButton->setObjectName(QStringLiteral("modelPreflightCancel"));
    m_cancelButton->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));
    m_cancelButton->setToolTip(QStringLiteral("取消当前模型预检"));
    m_cancelButton->setFixedSize(30, 30);
    m_cancelButton->setEnabled(false);

    stateRow->addWidget(m_stateLabel, 1);
    stateRow->addWidget(m_modeLabel, 1);
    stateRow->addWidget(m_recheckButton);
    stateRow->addWidget(m_cancelButton);
    layout->addLayout(stateRow);

    m_admissionLabel = new QLabel(QStringLiteral("禁止进入当前模式"), this);
    m_admissionLabel->setObjectName(QStringLiteral("modelPreflightAdmission"));
    m_admissionLabel->setWordWrap(true);
    layout->addWidget(m_admissionLabel);

    m_detailLabel = new QLabel(
        QStringLiteral("generation=0，cache=未命中，fast=未完成，full=未完成"),
        this);
    m_detailLabel->setObjectName(QStringLiteral("modelPreflightDetail"));
    m_detailLabel->setWordWrap(true);
    layout->addWidget(m_detailLabel);

    m_issueTable = new QTableWidget(this);
    m_issueTable->setObjectName(QStringLiteral("modelPreflightIssues"));
    m_issueTable->setColumnCount(4);
    m_issueTable->setHorizontalHeaderLabels(
        QStringList{QStringLiteral("级别"),
                    QStringLiteral("问题"),
                    QStringLiteral("数量"),
                    QStringLiteral("建议")});
    m_issueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_issueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_issueTable->setWordWrap(true);
    m_issueTable->verticalHeader()->setVisible(false);
    m_issueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_issueTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_issueTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_issueTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_issueTable->setColumnWidth(1, 160);
    layout->addWidget(m_issueTable, 1);

    connect(
        m_recheckButton,
        &QPushButton::clicked,
        this,
        &ModelPreflightPanel::SigRecheckRequested);
    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &ModelPreflightPanel::SigCancelRequested);
}

void ModelPreflightPanel::ShowPresentation(
    const ModelPreflightPresentation& presentation)
{
    m_stateLabel->setText(presentation.state);
    m_modeLabel->setText(presentation.mode);
    m_admissionLabel->setText(presentation.admission);
    m_detailLabel->setText(presentation.detail);
    m_recheckButton->setEnabled(presentation.canrecheck);
    m_cancelButton->setEnabled(presentation.cancancel);

    m_issueTable->setRowCount(presentation.issues.size());
    for (int rowIndex{0}; rowIndex < presentation.issues.size(); ++rowIndex)
    {
        const ModelPreflightIssuePresentation& issue =
            presentation.issues.at(rowIndex);
        auto* severityItem = new QTableWidgetItem(issue.severity);
        auto* summaryItem = new QTableWidgetItem(issue.summary);
        auto* countItem = new QTableWidgetItem(issue.count);
        auto* recommendationItem = new QTableWidgetItem(issue.recommendation);
        const QString tooltip = issue.code + QStringLiteral("\n")
            + issue.recommendation;
        severityItem->setToolTip(tooltip);
        summaryItem->setToolTip(tooltip);
        countItem->setToolTip(tooltip);
        recommendationItem->setToolTip(tooltip);
        m_issueTable->setItem(rowIndex, 0, severityItem);
        m_issueTable->setItem(rowIndex, 1, summaryItem);
        m_issueTable->setItem(rowIndex, 2, countItem);
        m_issueTable->setItem(rowIndex, 3, recommendationItem);
    }
    m_issueTable->resizeRowsToContents();
}
