#pragma once

#include <QJsonObject>
#include <QString>

/** @brief Material channel used for the host's solid model fill. */
enum class HostMaterialStrategy
{
    RgbSolid,
    WhiteSolid,
    VarnishSolid
};

/** @brief Support generation mode exposed by the reference host Profile. */
enum class HostSupportMode
{
    None,
    BottomProjection,
    UnsupportedOnly,
    BottomProjectionPlusUnsupported,
    FullVerticalProjection
};

/** @brief Host-owned internal-void support parameters. */
struct hostinternalvoidsettings
{
    bool enabled{true};
    int minareapx{16};
};

/** @brief Host-owned maximum-footprint support base parameters. */
struct hostbaseprojectionsettings
{
    bool enabled{false};
    int layercount{30};
};

/** @brief Host-owned editable support parameters. */
struct hostsupportsettings
{
    bool enabled{true};
    HostSupportMode mode{HostSupportMode::BottomProjection};
    double offsetmm{0.0};
    int minareapx{0};
    hostinternalvoidsettings internalvoid;
    hostbaseprojectionsettings baseprojection;
};

/** @brief Device-owned build volume injected into the first scene Commit. */
struct hostbuildvolume
{
    double widthmm{230.0};
    double heightmm{100.0};
    double zlimitmm{60.0};
    QString origin{QStringLiteral("lower_left")};
    QString xdirection{QStringLiteral("positive")};
    QString ydirection{QStringLiteral("positive")};
};

/** @brief Host-owned editable slice parameters. */
struct hostslicesettings
{
    QString profileid;
    QString modelpath;
    QString modelformat;
    QString outputdirectory;
    int dpix{635};
    int dpiy{600};
    double layerthicknessmm{0.038};
    HostMaterialStrategy materialstrategy{HostMaterialStrategy::RgbSolid};
    hostbuildvolume buildvolume;
    hostsupportsettings support;
};

/** @brief Validated Profile preview ready for a future slice request. */
struct hosteffectiveprofile
{
    QJsonObject profile;
    QString profilehash;
};

/** @brief Builds and validates host-owned effective slice Profiles. */
class HostEffectiveProfileBuilder final
{
public:
    /**
     * @brief Validates editable host settings without calling the module.
     * @param settings Host-owned settings to inspect.
     * @param error Receives a user-readable validation reason.
     * @return True when settings can produce an effective Profile.
     */
    static bool Validate(
        const hostslicesettings& settings,
        QString* error);

    /**
     * @brief Builds a self-hashed effective Profile from host settings.
     * @param settings Valid host-owned settings.
     * @param effectiveProfile Receives parsed Profile JSON and hash.
     * @param error Receives a fail-closed reason.
     * @return True when the exact future submission Profile was built.
     */
    static bool Build(
        const hostslicesettings& settings,
        hosteffectiveprofile* effectiveProfile,
        QString* error);

    /**
     * @brief Converts a material strategy to a stable host identifier.
     * @param strategy Strategy selected by the operator.
     * @return Stable lowercase identifier used by UI tests and persistence.
     */
    static QString MaterialStrategyId(HostMaterialStrategy strategy);

    /**
     * @brief Converts a support mode to a stable production Profile value.
     * @param mode Support mode selected by the operator.
     * @return Stable identifier written to `support.mode`.
     */
    static QString SupportModeId(HostSupportMode mode);

    /**
     * @brief Compares two build volumes using the frozen scene semantics.
     * @param left First host volume.
     * @param right Second host volume.
     * @return True when dimensions, origin and axes are equal.
     */
    static bool BuildVolumesEqual(
        const hostbuildvolume& left,
        const hostbuildvolume& right);
};
