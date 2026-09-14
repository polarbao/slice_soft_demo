#pragma once
#include "HostProcessPresetCatalog.h"

// Selecting a protocol profile is also an explicit opt-in/out of T output.
// Preserve edited material/support parameters; only align the protocol policy.
inline bool AlignHostProfileProtocol(const QString& id,hostslicesettings* settings,QString* error)
{
    const bool targetT=id==QStringLiteral("host-reference-transfer-channel");
    if (targetT && settings->packageprotocol!=HostPackageProtocol::Rgbwsvt)
    {
        hostprocesspreset variant;
        const QString variantId=settings->processpresetid+QStringLiteral("_rgbwsvt");
        if (!HostProcessPresetCatalog::Resolve(variantId,&variant))
        {
            if(error) *error=QStringLiteral("当前自定义/候选工艺没有已验证的缩裹 T 版本，请先选择支持 T 的常用工艺。");
            return false;
        }
        settings->processpresetid=variantId;
        settings->packageprotocol=variant.packageprotocol;
        settings->transferchannel=variant.transferchannel;
    }
    else if(!targetT && id!=QStringLiteral("host-reference-package-review")
        && settings->packageprotocol==HostPackageProtocol::Rgbwsvt)
    {
        settings->packageprotocol=HostPackageProtocol::Rgbwsv;
        settings->transferchannel.enabled=false;
        if(settings->processpresetid.endsWith(QStringLiteral("_rgbwsvt")))
            settings->processpresetid.chop(8);
    }
    settings->profileid=id;
    return true;
}
