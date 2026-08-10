#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

/** @brief Host-owned description of one selectable production Profile. */
struct hostprofiledescriptor
{
    QString profileid;
    QString displayname;
    QString description;
    QString productionsafety;
    QStringList tags;
    QStringList requiredcapabilities;
    QString usage;
    QString defaultprocess;
    QString outputcontract;
    QString limitations;
};

/** @brief Capability-intersection result for one host Profile. */
struct hostprofileavailability
{
    hostprofiledescriptor profile;
    bool available{false};
    QStringList missingcapabilities;
};

/** @brief Resolved module capabilities and host Profile availability. */
struct hostprofilecatalogresolution
{
    QStringList modulecapabilities;
    QList<hostprofileavailability> profiles;
};

/**
 * @brief Abstract host-owned Profile directory.
 *
 * The public slicer module declares capabilities only. The printing host owns
 * device/material Profiles and supplies them through this interface.
 */
class IHostProfileCatalog
{
public:
    /** @brief Releases a host Profile provider. */
    virtual ~IHostProfileCatalog() = default;

    /**
     * @brief Returns the Profiles owned by the host application.
     * @return Stable Profile descriptors for the current host session.
     */
    virtual QList<hostprofiledescriptor> Profiles() const = 0;
};

/** @brief Reference-host Profile fixture independent from slicer internals. */
class ReferenceHostProfileCatalog final : public IHostProfileCatalog
{
public:
    /**
     * @brief Returns the reference host's public Profile fixtures.
     * @return Production, restricted and diagnostic Profile descriptors.
     */
    QList<hostprofiledescriptor> Profiles() const override;
};

/** @brief Resolves host Profiles against structured module information. */
class HostProfileCapabilityResolver final
{
public:
    /**
     * @brief Intersects host Profile requirements with pm_module_info provides.
     * @param catalog Host-owned Profile directory.
     * @param moduleInfo UTF-8 slicesoft.module_info.1 payload.
     * @param resolution Receives capabilities and per-Profile availability.
     * @param error Receives a fail-closed validation reason.
     * @return True when both inputs are structurally valid.
     */
    static bool Resolve(
        const IHostProfileCatalog& catalog,
        const QByteArray& moduleInfo,
        hostprofilecatalogresolution* resolution,
        QString* error);
};
