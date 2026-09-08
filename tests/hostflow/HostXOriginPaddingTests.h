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
    settings.scenepadtooriginx = true;
    if (!HostEffectiveProfileBuilder::Build(settings, &padded, &error)) return false;
    const auto bytes = QJsonDocument(padded.profile).toJson(QJsonDocument::Compact);
    std::istringstream input(bytes.toStdString());
    const auto hash = slicer_core::api::ComputeProfileDocumentHash(slicer_core::Json::parse(input));
    settings.scenepadtooriginx = false;
    if (!HostEffectiveProfileBuilder::Build(settings, &disabled, &error)) return false;
    HostSliceSettingsPanel panel;
    auto* check = panel.findChild<QCheckBox*>(QStringLiteral("hostScenePadToOriginXCheck"));
    if (!check || check->isChecked()) return false;
    check->setChecked(true);
    const bool enabled = panel.Settings().scenepadtooriginx;
    panel.SetPersistentSettings(settings);
    const bool cleared = !check->isChecked();
    settings.scenepadtooriginx = true;
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
        && transferHash == transfer.profilehash.toStdString()
        && transfer.profile.value(QStringLiteral("output")).toObject().value(QStringLiteral("scenePadToOriginX")).toBool()
        && base.profile == disabled.profile && base.profilehash != padded.profilehash
        && hash == padded.profilehash.toStdString()
        && !base.profile.value(QStringLiteral("output")).toObject().contains(QStringLiteral("scenePadToOriginX"))
        && padded.profile.value(QStringLiteral("output")).toObject().value(QStringLiteral("scenePadToOriginX")).toBool();
    if (!ok) errors << "XPAD UI/profile/persistence/hash failed" << Qt::endl;
    return ok;
}
