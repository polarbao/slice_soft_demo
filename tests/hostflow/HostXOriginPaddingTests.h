#pragma once
#include "apps/slicer_ui_host_sim/HostSliceSettingsPanel.h"
#include "apps/slicer_ui_host_sim/HostTransferProfileBridge.h"
#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/json_value.h"
#include <QCheckBox>
#include <QJsonDocument>
#include <QTextStream>
#include <sstream>

inline bool VerifyXOriginPadding(const QString& model, const QString& output, QTextStream& errors)
{
    hostslicesettings settings;
    settings.modelpath = model;
    settings.modelformat = QStringLiteral("obj");
    settings.outputdirectory = output;
    settings.profileid = QStringLiteral("host-reference-default");
    QString error;
    hosteffectiveprofile base, padded, disabled;
    if (!HostEffectiveProfileBuilder::Build(settings, &base, &error)) return false;
    for (const bool x : {false, true})
        for (const bool y : {false, true})
        {
            settings.scenepadtooriginx = x;
            settings.scenepadtooriginy = y;
            hosteffectiveprofile result;
            if (!HostEffectiveProfileBuilder::Build(settings, &result, &error)) return false;
            const auto outputJson = result.profile.value(QStringLiteral("output")).toObject();
            if (outputJson.contains(QStringLiteral("scenePadToOriginX")) != x
                || outputJson.contains(QStringLiteral("scenePadToOriginY")) != y
                || (result.profilehash == base.profilehash) != (!x && !y)) return false;
        }
    settings.scenepadtooriginy = false;
    settings.scenepadtooriginx = true;
    if (!HostEffectiveProfileBuilder::Build(settings, &padded, &error)) return false;
    const auto bytes = QJsonDocument(padded.profile).toJson(QJsonDocument::Compact);
    std::istringstream input(bytes.toStdString());
    const auto hash = slicer_core::api::ComputeProfileDocumentHash(slicer_core::Json::parse(input));
    settings.scenepadtooriginx = false;
    if (!HostEffectiveProfileBuilder::Build(settings, &disabled, &error)) return false;
    HostSliceSettingsPanel panel;
    auto* check = panel.findChild<QCheckBox*>(QStringLiteral("hostScenePadToOriginXCheck"));
    auto* yCheck = panel.findChild<QCheckBox*>(QStringLiteral("hostScenePadToOriginYCheck"));
    if (!yCheck || yCheck->isChecked()) return false;
    yCheck->setChecked(true);
    if (!panel.Settings().scenepadtooriginy || panel.Settings().scenepadtooriginx) return false;
    if (!check || check->isChecked()) return false;
    check->setChecked(true);
    const bool enabled = panel.Settings().scenepadtooriginx;
    panel.SetPersistentSettings(settings);
    const bool cleared = !check->isChecked();
    if (yCheck->isChecked()) return false;
    settings.scenepadtooriginx = true;
    settings.scenepadtooriginy = true;
    panel.SetPersistentSettings(settings);
    settings.packageprotocol = HostPackageProtocol::Rgbwsvt;
    settings.profileid = QStringLiteral("host-reference-transfer-channel");
    settings.transferchannel.enabled = true;
    settings.transferchannel.materialdiffusergbvalues = {{255, 220, 198}};
    hosteffectiveprofile transfer;
    const bool transferAccepted = HostTransferProfileBridge::Validate(settings, &error)
        && HostEffectiveProfileBuilder::Build(settings, &transfer, &error);
    const auto transferBytes = QJsonDocument(transfer.profile).toJson(QJsonDocument::Compact);
    std::istringstream transferInput(transferBytes.toStdString());
    const auto transferHash = slicer_core::api::ComputeProfileDocumentHash(slicer_core::Json::parse(transferInput));
    const bool ok = enabled && cleared && check->isChecked() && transferAccepted
        && yCheck->isChecked()
        && transfer.profile.value(QStringLiteral("output")).toObject().value(QStringLiteral("scenePadToOriginY")).toBool()
        && transferHash == transfer.profilehash.toStdString()
        && transfer.profile.value(QStringLiteral("output")).toObject().value(QStringLiteral("scenePadToOriginX")).toBool()
        && base.profile == disabled.profile && base.profilehash != padded.profilehash
        && hash == padded.profilehash.toStdString()
        && !base.profile.value(QStringLiteral("output")).toObject().contains(QStringLiteral("scenePadToOriginX"))
        && padded.profile.value(QStringLiteral("output")).toObject().value(QStringLiteral("scenePadToOriginX")).toBool();
    if (!ok) errors << "XPAD UI/profile/persistence/hash failed" << Qt::endl;
    return ok;
}
