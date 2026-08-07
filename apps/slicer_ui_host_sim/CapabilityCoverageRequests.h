#pragma once

#include "HostRequestBuilder.h"

#include <QJsonObject>
#include <QString>

struct capabilitycoveragefixture
{
    QString modelpath;
    QString resourceroot;
    QString packagedirectory;
    QString previewpath;
    QString modelid;
    QString sourcedigest;
    QString resourcedigest;
    QString scenehash;
    QString profilehash;
    quint64 scenerevision{0};
    quint64 scenehandle{0};
    HostBounds3 sourcebounds{};
    HostBounds3 effectivebounds{};
    QJsonObject initialscene;
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
     * @brief Binds import metadata and builds source/committed scene documents.
     * @param imported Successful model.import response.
     * @param fixture In/out host fixture state.
     * @param error Receives malformed metadata or scene-builder failure.
     * @return True when both scene documents and the default Profile are ready.
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

    /**
     * @brief Builds a scene document for the fixture's current transforms.
     * @param fixture Bound host fixture.
     * @param committed Selects revision-one translated bounds when true.
     * @param scene Receives the scene object.
     * @param error Receives a builder or JSON error.
     * @return True when the scene is valid JSON.
     */
    static bool BuildScene(
        const capabilitycoveragefixture& fixture,
        bool committed,
        QJsonObject* scene,
        QString* error);

private:
    static bool ParseBounds(
        const QJsonObject& imported,
        HostBounds3* bounds,
        QString* error);
};
