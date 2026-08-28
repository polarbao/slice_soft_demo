#include "HostPackageReviewChannels.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QJsonValue>
#include <QSet>

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

void SelectDefaultPreviewMode(
    QComboBox* combo,
    const QStringList& packageChannels)
{
    if (combo == nullptr || packageChannels.isEmpty())
    {
        return;
    }
    const QSet<QString> available(
        packageChannels.begin(), packageChannels.end());
    // 并集项（索引 0）的通道集随包改写，而不是提供多个固定并集让用户去挑：
    // 七通道并集是六通道并集的严格超集，对七通道包而言后者纯属冗余；
    // 对六通道包，七通道并集又因缺 T 平面必然失败。包是几通道是软件知道的事。
    if (combo->count() > 0)
    {
        combo->setItemData(0, packageChannels);
        combo->setItemText(
            0,
            QStringLiteral("全通道并集（%1 通道%2）")
                .arg(packageChannels.size())
                .arg(available.contains(QStringLiteral("T"))
                         ? QStringLiteral("·含缩裹")
                         : QString{}));
    }
    int preferred = -1;
    int widest = -1;
    for (int index = 0; index < combo->count(); ++index)
    {
        const QStringList wanted = combo->itemData(index).toStringList();
        bool satisfiable = !wanted.isEmpty();
        for (const QString& channel : wanted)
        {
            if (!available.contains(channel))
            {
                satisfiable = false;
                break;
            }
        }
        // 包里没有的通道一律置灰：留着可选而必然失败，等于把「这个包是几通道」
        // 的知识推给用户，且失败信息出现在渲染阶段而非选择阶段。
        auto* model = qobject_cast<QStandardItemModel*>(combo->model());
        if (model != nullptr)
        {
            QStandardItem* item = model->item(index);
            if (item != nullptr)
            {
                item->setEnabled(satisfiable);
            }
        }
        // 默认取【该包能满足的最宽组合】，使切片产出的全部内容默认可见。
        if (satisfiable && wanted.size() > widest)
        {
            widest = wanted.size();
            preferred = index;
        }
    }
    if (preferred >= 0)
    {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(preferred);
    }
}
