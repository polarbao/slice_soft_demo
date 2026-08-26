#include "HostWorkspaceTransferState.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include <QSet>

namespace
{
QByteArray SerializeColours(const QVector<hostrgbcolor>& colours)
{
    QJsonArray values;
    for (const hostrgbcolor& colour : colours)
    {
        values.append(QJsonArray{colour.red, colour.green, colour.blue});
    }
    return QJsonDocument(values).toJson(QJsonDocument::Compact);
}

bool ParseColours(
    const QByteArray& encoded,
    QVector<hostrgbcolor>* colours)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(encoded, &error);
    if (!document.isArray() || error.error != QJsonParseError::NoError)
    {
        return false;
    }
    QSet<QString> unique;
    for (const QJsonValue& value : document.array())
    {
        const QJsonArray channels = value.toArray();
        if (!value.isArray() || channels.size() != 3)
        {
            return false;
        }
        const hostrgbcolor colour{
            channels.at(0).toInt(-1),
            channels.at(1).toInt(-1),
            channels.at(2).toInt(-1)};
        if (colour.red < 0 || colour.red > 255
            || colour.green < 0 || colour.green > 255
            || colour.blue < 0 || colour.blue > 255)
        {
            return false;
        }
        const QString key = QStringLiteral("%1,%2,%3")
            .arg(colour.red).arg(colour.green).arg(colour.blue);
        if (unique.contains(key))
        {
            return false;
        }
        unique.insert(key);
        colours->push_back(colour);
    }
    return true;
}
}

void HostWorkspaceTransferState::Save(
    QSettings& settings,
    const HostPackageProtocol protocol,
    const hosttransferchannelsettings& transfer)
{
    settings.setValue(
        QStringLiteral("packageProtocol"),
        HostEffectiveProfileBuilder::PackageProtocolId(protocol));
    settings.setValue(
        QStringLiteral("transferChannel/enabled"), transfer.enabled);
    settings.setValue(
        QStringLiteral("transferChannel/matchSource"), transfer.matchsource);
    settings.setValue(
        QStringLiteral("transferChannel/materialDiffuseRgbValues"),
        SerializeColours(transfer.materialdiffusergbvalues));
    settings.setValue(
        QStringLiteral("transferChannel/missingRegion"),
        transfer.missingregion);
    settings.setValue(
        QStringLiteral("transferChannel/multipleMatches"),
        transfer.multiplematches);
    settings.setValue(
        QStringLiteral("transferChannel/value"), transfer.value);
    settings.setValue(
        QStringLiteral("transferChannel/topology/selfIntersectionPolicy"),
        transfer.selfintersectionpolicy);
    settings.setValue(
        QStringLiteral("transferChannel/topology/maxSelfIntersectionPairs"),
        transfer.maxselfintersectionpairs);
}

bool HostWorkspaceTransferState::Restore(
    QSettings& settings,
    const int schemaVersion,
    const QString& profileId,
    const QString& processPresetId,
    HostPackageProtocol* protocol,
    hosttransferchannelsettings* transfer)
{
    if (protocol == nullptr || transfer == nullptr)
    {
        return false;
    }
    if (schemaVersion == 7)
    {
        *protocol = HostPackageProtocol::Rgbwsv;
        *transfer = {};
        return profileId != QStringLiteral("host-reference-transfer-channel")
            && !processPresetId.endsWith(QStringLiteral("_rgbwsvt"));
    }
    if (schemaVersion != 8)
    {
        return false;
    }
    const QString protocolId = settings.value(
        QStringLiteral("packageProtocol")).toString();
    if (protocolId == QStringLiteral("p0.rgbwsv.2"))
    {
        *protocol = HostPackageProtocol::Rgbwsv;
    }
    else if (protocolId == QStringLiteral("p0.rgbwsvt.1"))
    {
        *protocol = HostPackageProtocol::Rgbwsvt;
    }
    else
    {
        return false;
    }
    hosttransferchannelsettings restored;
    restored.enabled = settings.value(
        QStringLiteral("transferChannel/enabled")).toBool();
    restored.matchsource = settings.value(
        QStringLiteral("transferChannel/matchSource")).toString();
    restored.missingregion = settings.value(
        QStringLiteral("transferChannel/missingRegion")).toString();
    restored.multiplematches = settings.value(
        QStringLiteral("transferChannel/multipleMatches")).toString();
    restored.value = settings.value(
        QStringLiteral("transferChannel/value"), -1).toInt();
    restored.selfintersectionpolicy = settings.value(
        QStringLiteral(
            "transferChannel/topology/selfIntersectionPolicy")).toString();
    restored.maxselfintersectionpairs = settings.value(
        QStringLiteral(
            "transferChannel/topology/maxSelfIntersectionPairs"), -1).toInt();
    const bool coloursValid = ParseColours(
        settings.value(QStringLiteral(
            "transferChannel/materialDiffuseRgbValues")).toByteArray(),
        &restored.materialdiffusergbvalues);
    const bool policyValid = restored.matchsource
            == QStringLiteral("material_diffuse_rgb")
        && (restored.missingregion == QStringLiteral("allow_empty")
            || restored.missingregion == QStringLiteral("fail_closed"))
        && restored.multiplematches == QStringLiteral("fail_closed")
        && restored.value == 0
        && (restored.selfintersectionpolicy == QStringLiteral("reject")
            || restored.selfintersectionpolicy
                == QStringLiteral("tolerate_closed_self_intersection"))
        && restored.maxselfintersectionpairs > 0;
    const bool protocolValid = (*protocol == HostPackageProtocol::Rgbwsvt)
        ? restored.enabled && coloursValid
            && !restored.materialdiffusergbvalues.isEmpty()
        : !restored.enabled && coloursValid;
    const bool identityValid = (*protocol == HostPackageProtocol::Rgbwsvt)
        ? profileId == QStringLiteral("host-reference-transfer-channel")
            && processPresetId.endsWith(QStringLiteral("_rgbwsvt"))
        : profileId != QStringLiteral("host-reference-transfer-channel")
            && !processPresetId.endsWith(QStringLiteral("_rgbwsvt"));
    if (!policyValid || !protocolValid || !identityValid)
    {
        return false;
    }
    *transfer = restored;
    return true;
}
