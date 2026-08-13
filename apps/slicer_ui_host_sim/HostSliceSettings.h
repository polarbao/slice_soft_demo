#pragma once

#include <QJsonObject>
#include <QString>

/** @brief Material channel used for the host's solid model fill. */
enum class HostMaterialStrategy
{
    RgbSolid,
    RgbWhite,
    RgbVarnish,
    RgbWhiteVarnish,
    WhiteSolid,
    VarnishSolid
};

/** @brief Material role used when resolving OBJ/3MF input materials. */
enum class HostMaterialRole
{
    Rgb,
    White,
    Varnish,
    Ignore,
    SupportCandidate
};

/** @brief Host-owned material policy and role-mapping parameters. */
struct hostmaterialprocesssettings
{
    bool rolemappingenabled{false};
    HostMaterialRole defaultrole{HostMaterialRole::Rgb};
    bool mapwhitenames{true};
    bool mapvarnishnames{true};
    bool allowinputsupportmaterial{false};
    int whiteexpandpx{0};
    int whiteshrinkpx{0};
    int varnishtoplayers{1};
    int maxunexpectedoverlappixels{0};
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

/** @brief Production texture application mode exposed by the host Profile. */
enum class HostTextureApplyMode
{
    SolidVolumeFromTopSurface,
    TopSurfaceOnly,
    TopSurfaceBand
};

/** @brief Texture filtering mode used while sampling UV coordinates. */
enum class HostTextureSampler
{
    Nearest,
    Bilinear
};

/** @brief Texture addressing mode used outside the normalized UV range. */
enum class HostTextureUvAddressMode
{
    Clamp,
    Repeat
};

/** @brief Fail-closed behavior for missing or invalid texture assets. */
enum class HostTextureMissingPolicy
{
    WarnAndFallback,
    FailFast
};

/** @brief RGB behavior below a limited texture surface band. */
enum class HostTextureNonSurfacePolicy
{
    ModelMaterial,
    Empty
};

/** @brief Carrier policy for texture pixels that are all channel-empty. */
enum class HostTextureWhitePolicy
{
    FailClosed,
    WhiteUnderbase
};

/** @brief Geometry occupancy sampling strategy exposed by host code. */
enum class HostGeometrySamplingStrategy
{
    LegacyCenterSample,
    LayerSlabSupersample2x2AtLeastTwoCandidate
};

/** @brief Host-owned production texture and Stage 15 white-carrier settings. */
struct hosttexturesettings
{
    bool enabled{false};
    HostTextureApplyMode applymode{
        HostTextureApplyMode::SolidVolumeFromTopSurface};
    int topsurfacelayers{1};
    HostTextureSampler sampler{HostTextureSampler::Bilinear};
    HostTextureUvAddressMode uvaddressmode{
        HostTextureUvAddressMode::Clamp};
    bool flipv{true};
    int fallbackred{0};
    int fallbackgreen{0};
    int fallbackblue{0};
    HostTextureMissingPolicy missingpolicy{
        HostTextureMissingPolicy::WarnAndFallback};
    HostTextureNonSurfacePolicy nonsurfacepolicy{
        HostTextureNonSurfacePolicy::ModelMaterial};
    HostTextureWhitePolicy whitepolicy{
        HostTextureWhitePolicy::FailClosed};
    int whiteinkthreshold{0};
    int whitevalue{0};
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
    hostmaterialprocesssettings materialprocess;
    hostbuildvolume buildvolume;
    hostsupportsettings support;
    hosttexturesettings texture;
    HostGeometrySamplingStrategy geometrysamplingstrategy{
        HostGeometrySamplingStrategy::LegacyCenterSample};
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
     * @brief Converts a material role to a stable Profile value.
     * @param role Material role selected by the operator.
     * @return Stable identifier written to `materialRoleMapping`.
     */
    static QString MaterialRoleId(HostMaterialRole role);

    /**
     * @brief Converts a support mode to a stable production Profile value.
     * @param mode Support mode selected by the operator.
     * @return Stable identifier written to `support.mode`.
     */
    static QString SupportModeId(HostSupportMode mode);

    /**
     * @brief Converts geometry sampling to its stable Profile value.
     * @param strategy Geometry sampling selected by host code.
     * @return Approved Profile identifier, or `unknown`.
     */
    static QString GeometrySamplingStrategyId(
        HostGeometrySamplingStrategy strategy);

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
