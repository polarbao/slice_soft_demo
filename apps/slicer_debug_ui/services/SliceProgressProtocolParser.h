#pragma once

#include <cstdint>
#include <QString>
#include <QVector>

/**
 * @brief One progress update emitted by slicer_cli.
 */
struct SliceProgressEvent
{
    QString phase;
    int current{0};
    int total{0};
    int percent{0};
    double elapsedms{0.0};
};

/**
 * @brief Detailed timing values emitted after a slicing run.
 */
struct SliceTimingEvent
{
    QString engine;
    QString profilelevel;
    double configloadms{0.0};
    double modelloadms{0.0};
    double gridsetupms{0.0};
    double sliceprocessingms{0.0};
    double layercomputems{0.0};
    double layercomposems{0.0};
    double tiffwritems{0.0};
    double previewwritems{0.0};
    double reportbuildms{0.0};
    double reportwritems{0.0};
    double packagepublishms{0.0};
    double outputwritems{0.0};
    double totalms{0.0};
    bool memoryavailable{false};
    std::uint64_t workingsetbytes{0};
    std::uint64_t peakworkingsetbytes{0};
};

/**
 * @brief Parsed events produced by one stdout chunk.
 */
struct SliceProtocolUpdate
{
    QVector<SliceProgressEvent> progress;
    QVector<SliceTimingEvent> timings;
};

/**
 * @brief Incrementally parse stable slicer_cli progress and timing protocol lines.
 */
class SliceProgressProtocolParser final
{
public:
    /**
     * @brief Clear buffered partial output from the previous process.
     */
    void Reset();

    /**
     * @brief Append one stdout chunk and parse every completed protocol line.
     * @param text Process stdout text, which may contain partial or multiple lines.
     * @return Parsed progress and timing events.
     */
    SliceProtocolUpdate Append(const QString& text);

private:
    QString m_buffer;
};
