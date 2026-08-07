#pragma once

#include <QJsonObject>
#include <QString>

struct capabilitycoveragefixture
{
    QString modelpath;
    QString packagedirectory;
    QString previewpath;
    QString modelid;
    QString scenehash;
    QString profilehash;
    quint64 scenerevision{0};
    quint64 scenehandle{0};
    QJsonObject committedscene;
    QJsonObject profile;
};

/** @brief Host-only builders for the frozen capability request DTOs. */
class CapabilityCoverageRequests final
{
public:
    /**
     * @brief Initializes fixture paths before model import.
     * @param repositoryRoot SliceSoft repository root.
     * @param evidenceRoot Writable host evidence root.
     * @param fixture Receives normalized fixture paths.
     * @param error Receives a path validation error.
     * @return True when the reference model exists and output paths are valid.
     */
    static bool InitializePaths(
        const QString& repositoryRoot,
        const QString& evidenceRoot,
        capabilitycoveragefixture* fixture,
        QString* error);

    /**
     * @brief Binds imported model identity and builds the reference Profile.
     * @param imported Successful model.import response.
     * @param fixture In/out host fixture state.
     * @param error Receives malformed metadata or Profile-builder failure.
     * @return True when the model identity and default Profile are ready.
     */
    static bool BindImportedModel(
        const QJsonObject& imported,
        capabilitycoveragefixture* fixture,
        QString* error);

    /**
     * @brief Builds a self-hashed Profile with a requested layer thickness.
     * @param fixture Bound host fixture.
     * @param layerThicknessMm Positive layer thickness in millimetres.
     * @param profile Receives the Profile object.
     * @param profileHash Receives the frozen Profile identity.
     * @param error Receives a builder or JSON error.
     * @return True when the Profile is valid JSON.
     */
    static bool BuildProfile(
        const capabilitycoveragefixture& fixture,
        double layerThicknessMm,
        QJsonObject* profile,
        QString* profileHash,
        QString* error);
};
