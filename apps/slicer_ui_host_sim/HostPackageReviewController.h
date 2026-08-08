#pragma once

#include "ModuleClient.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <memory>

class QTemporaryDir;

/** @brief Six-channel print-pixel counts in frozen RGBWSV order. */
struct hostchannelcounts
{
    std::array<quint64, 6> values{};
};

/** @brief Host-owned view of one production layer descriptor. */
struct hostlayerdescriptor
{
    int layerindex{0};
    double zmm{0.0};
    int widthpx{0};
    int heightpx{0};
    hostchannelcounts printpixels;
    hostchannelcounts emptypixels;
    QString storagemode;
    QString tiffpath;
};

/** @brief Verified package summary consumed by the reference host UI. */
struct hostpackagereview
{
    QString packagedirectory;
    QString packageidentity;
    QString schema;
    QString polarity;
    QStringList channels;
    QStringList verificationerrors;
    int layercount{0};
    int widthpx{0};
    int heightpx{0};
    int dpix{0};
    int dpiy{0};
    int bitdepth{0};
    int instancecount{0};
    QString profileversion;
    QString profilehash;
    bool valid{false};
    QVector<hostlayerdescriptor> layers;
};

/** @brief Named report returned through package.read_report. */
struct hostpackagereport
{
    QString name;
    QString schema;
    QString sourcepath;
    QJsonObject data;
};

/**
 * @brief Reads production package results exclusively through public SPI calls.
 */
class HostPackageReviewController final
{
public:
    /**
     * @brief Creates a package review controller for one loaded module.
     * @param client Public module ABI client.
     */
    explicit HostPackageReviewController(ModuleClient& client);

    /** @brief Releases the host-owned preview cache directory. */
    ~HostPackageReviewController();

    /**
     * @brief Verifies and loads summary plus all layer descriptors.
     * @param packageDirectory Published RGBWSV package directory.
     * @param error Receives a fail-closed validation reason.
     * @return True when the package and frozen protocol fields are valid.
     */
    bool Load(const QString& packageDirectory, QString* error);

    /**
     * @brief Renders one layer from production TIFF through the module ABI.
     * @param layerIndex Zero-based production layer index.
     * @param channels One or more channels in frozen RGBWSV spelling.
     * @param outputPath Receives the host-cache preview image path.
     * @param error Receives a rendering or contract error.
     * @return True when a readable preview image was published.
     */
    bool RenderPreview(
        int layerIndex,
        const QStringList& channels,
        QString* outputPath,
        QString* error);

    /**
     * @brief Reads one manifest-registered report through public SPI.
     * @param reportName Stable report map key such as slice.
     * @param report Receives the structured report.
     * @param error Receives a missing-report or contract error.
     * @return True when the named report is available.
     */
    bool ReadReport(
        const QString& reportName,
        hostpackagereport* report,
        QString* error);

    /**
     * @brief Returns the most recently loaded package review.
     * @return Immutable host-owned package data.
     */
    [[nodiscard]] const hostpackagereview& Review() const;

private:
    bool ExecuteObject(
        const QJsonObject& request,
        QJsonObject* response,
        QString* error);
    bool LoadVerification(QString* error);
    bool LoadSummary(QString* error);
    bool LoadLayers(QString* error);

    ModuleClient& m_client;
    std::unique_ptr<QTemporaryDir> m_previewCache;
    hostpackagereview m_review;
};
