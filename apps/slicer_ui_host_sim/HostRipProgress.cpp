#include "HostRipJobController.h"
#include <QDir>

void HostRipJobController::InitializeProgress()
{
    auto* timer=new QTimer(this);
    timer->setInterval(500);
    connect(timer,&QTimer::timeout,this,&HostRipJobController::UpdateProgress);
    timer->start();
}

void HostRipJobController::UpdateProgress()
{
    if (!IsActive()) return;
    QString phase;
    int observed=-1;
    if (m_cancelRequested) phase=QStringLiteral("正在取消");
    else switch(m_phase)
    {
    case Phase::ValidatingInput: phase=QStringLiteral("输入校验与源身份检查"); break;
    case Phase::RunningProcess:
        phase=QStringLiteral("RIP 运算与文件输出");
        if (!m_stagingDirectory.isEmpty())
            observed=QDir(m_stagingDirectory).entryList({QStringLiteral("*.tif"),QStringLiteral("*.tiff")},QDir::Files).size();
        break;
    case Phase::ValidatingOutput: phase=QStringLiteral("输出校验与源身份复核"); break;
    case Phase::Publishing: phase=QStringLiteral("报告与发布"); break;
    case Phase::Idle: return;
    }
    emit SigProgress(phase,observed,m_package.layerCount,m_elapsed.isValid()?m_elapsed.elapsed():0);
}

void HostRipJobController::PublishState(const QString& state,const QString& message)
{
    emit SigStateChanged(state,message);
    UpdateProgress();
}
