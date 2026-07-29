#include "SliceProgressProtocolParser.h"

#include <QMap>
#include <QStringList>

namespace
{

constexpr auto kProgressPrefix = "SLICE_PROGRESS ";
constexpr auto kTimingPrefix = "SLICE_TIMING ";

QMap<QString, QString> ParseFields(const QString& text)
{
    QMap<QString, QString> fields;
    const QStringList parts = text.split(' ', Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        const int separator = part.indexOf('=');
        if (separator <= 0 || separator + 1 >= part.size())
        {
            continue;
        }
        fields.insert(part.left(separator), part.mid(separator + 1));
    }
    return fields;
}

double ReadDouble(const QMap<QString, QString>& fields, const QString& key)
{
    bool valid{false};
    const double value = fields.value(key).toDouble(&valid);
    return valid ? value : 0.0;
}

int ReadInt(const QMap<QString, QString>& fields, const QString& key)
{
    bool valid{false};
    const int value = fields.value(key).toInt(&valid);
    return valid ? value : 0;
}

std::uint64_t ReadUnsigned(
    const QMap<QString, QString>& fields,
    const QString& key)
{
    bool valid{false};
    const qulonglong value = fields.value(key).toULongLong(&valid);
    return valid ? static_cast<std::uint64_t>(value) : 0U;
}

void ParseLine(const QString& line, SliceProtocolUpdate* update)
{
    if (line.startsWith(QLatin1String(kProgressPrefix)))
    {
        const QMap<QString, QString> fields = ParseFields(line.mid(static_cast<int>(qstrlen(kProgressPrefix))));
        SliceProgressEvent event;
        event.phase = fields.value(QStringLiteral("phase"));
        event.current = ReadInt(fields, QStringLiteral("current"));
        event.total = ReadInt(fields, QStringLiteral("total"));
        event.percent = qBound(0, ReadInt(fields, QStringLiteral("percent")), 100);
        event.elapsedms = ReadDouble(fields, QStringLiteral("elapsedMs"));
        if (!event.phase.isEmpty())
        {
            update->progress.push_back(event);
        }
        return;
    }

    if (!line.startsWith(QLatin1String(kTimingPrefix)))
    {
        return;
    }

    const QMap<QString, QString> fields = ParseFields(line.mid(static_cast<int>(qstrlen(kTimingPrefix))));
    SliceTimingEvent event;
    event.engine = fields.value(QStringLiteral("engine"));
    event.profilelevel = fields.value(QStringLiteral("profileLevel"));
    event.configloadms = ReadDouble(fields, QStringLiteral("configLoadMs"));
    event.modelloadms = ReadDouble(fields, QStringLiteral("modelLoadMs"));
    event.gridsetupms = ReadDouble(fields, QStringLiteral("gridSetupMs"));
    event.sliceprocessingms = ReadDouble(fields, QStringLiteral("sliceProcessingMs"));
    event.layercomputems = ReadDouble(fields, QStringLiteral("layerComputeMs"));
    event.layercomposems = ReadDouble(fields, QStringLiteral("layerComposeMs"));
    event.tiffwritems = ReadDouble(fields, QStringLiteral("tiffWriteMs"));
    event.previewwritems = ReadDouble(fields, QStringLiteral("previewWriteMs"));
    event.reportbuildms = ReadDouble(fields, QStringLiteral("reportBuildMs"));
    event.reportwritems = ReadDouble(fields, QStringLiteral("reportWriteMs"));
    event.packagepublishms = ReadDouble(fields, QStringLiteral("packagePublishMs"));
    event.outputwritems = ReadDouble(fields, QStringLiteral("outputWriteMs"));
    event.totalms = ReadDouble(fields, QStringLiteral("totalMs"));
    event.memoryavailable =
        ReadInt(fields, QStringLiteral("memoryAvailable")) != 0;
    event.workingsetbytes =
        ReadUnsigned(fields, QStringLiteral("workingSetBytes"));
    event.peakworkingsetbytes =
        ReadUnsigned(fields, QStringLiteral("peakWorkingSetBytes"));
    if (!event.engine.isEmpty())
    {
        update->timings.push_back(event);
    }
}

}  // namespace

void SliceProgressProtocolParser::Reset()
{
    m_buffer.clear();
}

SliceProtocolUpdate SliceProgressProtocolParser::Append(const QString& text)
{
    SliceProtocolUpdate update;
    m_buffer += text;
    int newline = m_buffer.indexOf('\n');
    while (newline >= 0)
    {
        QString line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (line.endsWith('\r'))
        {
            line.chop(1);
        }
        ParseLine(line, &update);
        newline = m_buffer.indexOf('\n');
    }
    return update;
}
