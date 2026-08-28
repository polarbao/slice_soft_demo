#include "HostPackageReviewPreviewMode.h"

#include <QComboBox>
#include <QHash>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QString>
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

void AttachPseudoColourTooltips(QComboBox* combo)
{
    if (combo == nullptr)
    {
        return;
    }
    static const QHash<QString, QString> kPseudoColour{
        {QStringLiteral("W"), QStringLiteral("青蓝 0,170,255")},
        {QStringLiteral("S"), QStringLiteral("纯绿 0,255,0")},
        {QStringLiteral("V"), QStringLiteral("中灰 127,127,127")},
        {QStringLiteral("T"), QStringLiteral("品红 255,0,255")}};
    for (int index = 0; index < combo->count(); ++index)
    {
        const QStringList channels = combo->itemData(index).toStringList();
        if (channels.size() != 1)
        {
            continue;
        }
        const auto found = kPseudoColour.constFind(channels.first());
        if (found == kPseudoColour.constEnd())
        {
            continue;
        }
        combo->setItemData(
            index,
            QStringLiteral("以伪彩色（%1）显示该通道，不代表生产 TIFF 像素值。")
                .arg(found.value()),
            Qt::ToolTipRole);
    }
}
