#include "HostTransferProfileBridge.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
bool IsValidTransferSettings(const hosttransferchannelsettings& transfer)
{
    if (!transfer.enabled
        || transfer.matchsource != QStringLiteral("material_diffuse_rgb")
        || transfer.materialdiffusergbvalues.isEmpty()
        || (transfer.missingregion != QStringLiteral("allow_empty")
            && transfer.missingregion != QStringLiteral("fail_closed"))
        || transfer.multiplematches != QStringLiteral("fail_closed")
        || transfer.value != 0
        || (transfer.selfintersectionpolicy != QStringLiteral("reject")
            && transfer.selfintersectionpolicy
                != QStringLiteral("tolerate_closed_self_intersection"))
        || transfer.maxselfintersectionpairs <= 0)
    {
        return false;
    }
    QSet<QString> unique;
    for (const hostrgbcolor& colour : transfer.materialdiffusergbvalues)
    {
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
    }
    return true;
}

QJsonObject TransferPolicyObject(
    const hosttransferchannelsettings& transfer)
{
    QJsonArray colours;
    for (const hostrgbcolor& colour : transfer.materialdiffusergbvalues)
    {
        colours.append(QJsonArray{colour.red, colour.green, colour.blue});
    }
    return QJsonObject{
        {QStringLiteral("enabled"), transfer.enabled},
        {QStringLiteral("matchSource"), transfer.matchsource},
        {QStringLiteral("materialDiffuseRgbValues"), colours},
        {QStringLiteral("missingRegion"), transfer.missingregion},
        {QStringLiteral("multipleMatches"), transfer.multiplematches},
        {QStringLiteral("value"), transfer.value},
        {QStringLiteral("topology"), QJsonObject{
            {QStringLiteral("selfIntersectionPolicy"),
             transfer.selfintersectionpolicy},
            {QStringLiteral("maxSelfIntersectionPairs"),
             transfer.maxselfintersectionpairs}}}};
}

QByteArray QuotedJsonString(const QString& value)
{
    const QByteArray encoded = QJsonDocument(
        QJsonArray{value}).toJson(QJsonDocument::Compact);
    return encoded.mid(1, encoded.size() - 2);
}

QByteArray CanonicalJson(const QJsonValue& value)
{
    if (value.isArray())
    {
        const QJsonArray array = value.toArray();
        QByteArray result{"["};
        if (!array.isEmpty())
        {
            result += '\n';
            for (int index = 0; index < array.size(); ++index)
            {
                result += CanonicalJson(array.at(index));
                result += index + 1 < array.size() ? ",\n" : "\n";
            }
        }
        return result + ']';
    }
    if (value.isObject())
    {
        const QJsonObject object = value.toObject();
        QStringList keys = object.keys();
        keys.sort(Qt::CaseSensitive);
        QByteArray result{"{"};
        if (!keys.isEmpty())
        {
            result += '\n';
            for (int index = 0; index < keys.size(); ++index)
            {
                const QString& key = keys.at(index);
                result += QuotedJsonString(key) + ": "
                    + CanonicalJson(object.value(key));
                result += index + 1 < keys.size() ? ",\n" : "\n";
            }
        }
        return result + '}';
    }
    if (value.isString())
    {
        return QuotedJsonString(value.toString());
    }
    if (value.isBool())
    {
        return value.toBool() ? QByteArrayLiteral("true")
                              : QByteArrayLiteral("false");
    }
    if (value.isDouble())
    {
        const double number = value.toDouble();
        std::ostringstream output;
        if (std::floor(number) == number)
        {
            output << std::setprecision(0) << std::fixed << number;
        }
        else
        {
            output << std::defaultfloat << std::setprecision(15) << number;
        }
        return QByteArray::fromStdString(output.str());
    }
    return QByteArrayLiteral("null");
}

QString ComputeProfileHash(QJsonObject profile)
{
    profile.remove(QStringLiteral("profileHash"));
    return QStringLiteral("sha256:")
        + QString::fromLatin1(QCryptographicHash::hash(
            CanonicalJson(profile), QCryptographicHash::Sha256).toHex());
}
}

bool HostTransferProfileBridge::Validate(
    const hostslicesettings& settings,
    QString* error)
{
    const bool usesTransferProtocol = settings.packageprotocol
        == HostPackageProtocol::Rgbwsvt;
    if (usesTransferProtocol == settings.transferchannel.enabled
        && (!usesTransferProtocol
            || (settings.profileid
                    == QStringLiteral("host-reference-transfer-channel")
                && IsValidTransferSettings(settings.transferchannel)))
        && (usesTransferProtocol
            || settings.profileid
                != QStringLiteral("host-reference-transfer-channel")))
    {
        return true;
    }
    if (error != nullptr)
    {
        *error = QStringLiteral(
            "RGBWSVT Profile、Package 协议与外部 T 通道策略必须显式一致。");
    }
    return false;
}

void HostTransferProfileBridge::Apply(
    const hostslicesettings& settings,
    QJsonObject* profile,
    QString* profileHash)
{
    if (settings.packageprotocol != HostPackageProtocol::Rgbwsvt)
    {
        return;
    }
    QJsonObject output = profile->value(QStringLiteral("output")).toObject();
    output.insert(
        QStringLiteral("packageProtocol"),
        HostEffectiveProfileBuilder::PackageProtocolId(
            settings.packageprotocol));
    output.insert(
        QStringLiteral("channelOrder"),
        QJsonArray{QStringLiteral("R"), QStringLiteral("G"),
                   QStringLiteral("B"), QStringLiteral("W"),
                   QStringLiteral("S"), QStringLiteral("V"),
                   QStringLiteral("T")});
    profile->insert(QStringLiteral("output"), output);
    profile->insert(
        QStringLiteral("transferChannelPolicy"),
        TransferPolicyObject(settings.transferchannel));
    *profileHash = ComputeProfileHash(*profile);
    profile->insert(QStringLiteral("profileHash"), *profileHash);
}

QString HostEffectiveProfileBuilder::TiffCompressionId(
    const HostTiffCompression compression)
{
    switch (compression)
    {
    case HostTiffCompression::None:
        return QStringLiteral("none");
    case HostTiffCompression::PackBits:
        return QStringLiteral("packbits");
    }
    return QStringLiteral("unknown");
}

QString HostEffectiveProfileBuilder::PackageProtocolId(
    const HostPackageProtocol protocol)
{
    switch (protocol)
    {
    case HostPackageProtocol::Rgbwsv:
        return QStringLiteral("p0.rgbwsv.2");
    case HostPackageProtocol::Rgbwsvt:
        return QStringLiteral("p0.rgbwsvt.1");
    }
    return QStringLiteral("unknown");
}

bool HostEffectiveProfileBuilder::BuildVolumesEqual(
    const hostbuildvolume& left,
    const hostbuildvolume& right)
{
    constexpr double epsilon = 1.0e-9;
    return std::abs(left.widthmm - right.widthmm) <= epsilon
        && std::abs(left.heightmm - right.heightmm) <= epsilon
        && std::abs(left.zlimitmm - right.zlimitmm) <= epsilon
        && left.origin == right.origin
        && left.xdirection == right.xdirection
        && left.ydirection == right.ydirection;
}
