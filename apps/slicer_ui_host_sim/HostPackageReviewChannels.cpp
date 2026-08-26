#include "HostPackageReviewChannels.h"

#include <QJsonValue>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>

namespace
{
constexpr std::array<const char*, 7> ChannelNames{
    "R", "G", "B", "W", "S", "V", "T"};
}  // namespace

bool ReadChannelCounts(
    const QJsonObject& object,
    const QStringList& channels,
    hostchannelcounts* counts)
{
    if (counts == nullptr)
    {
        return false;
    }
    for (const QString& channel : channels)
    {
        const auto found = std::find_if(
            ChannelNames.begin(),
            ChannelNames.end(),
            [&channel](const char* name)
            {
                return channel == QString::fromLatin1(name);
            });
        if (found == ChannelNames.end())
        {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(ChannelNames.begin(), found));
        const QJsonValue value = object.value(channel);
        if (!value.isDouble() || value.toDouble() < 0.0)
        {
            return false;
        }
        counts->values[index] = static_cast<quint64>(value.toDouble());
    }
    return true;
}

bool IsFrozenChannelSet(const QStringList& channels)
{
    const QStringList rgbwsv{
        QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"),
        QStringLiteral("W"), QStringLiteral("S"), QStringLiteral("V")};
    QStringList rgbwsvt = rgbwsv;
    rgbwsvt.append(QStringLiteral("T"));
    return channels == rgbwsv || channels == rgbwsvt;
}

bool IsFrozenChannelName(const QString& channel)
{
    for (const char* name : ChannelNames)
    {
        if (channel == QString::fromLatin1(name))
        {
            return true;
        }
    }
    return false;
}
