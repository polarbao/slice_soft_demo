#pragma once

#include "slicer_core/config.h"

#include <QString>
#include <QStringList>

enum class ModelFillMaterial
{
    Rgb,
    White,
    Varnish,
};

enum class SupportPlacement
{
    Lower,
    Upper,
    Both,
    UnsupportedOnly,
    FullVerticalProjection,
};

enum class SliceEngineRole
{
    LegacyProduction,
    OpenVdbUtilityCandidate,
};

/**
 * @brief Support settings edited by the Qt workbench.
 */
struct SupportSettings
{
    bool enabled{true};
    SupportPlacement placement{SupportPlacement::Lower};
    bool internalvoidenabled{true};
    int internalvoidminareapx{16};
    bool baseprojectionenabled{true};
    int baseprojectionlayercount{30};
};

/**
 * @brief Surface varnish settings that do not expand the XY model envelope.
 */
struct SurfaceVarnishSettings
{
    bool enabled{false};
    int thicknesspx{0};
};

/**
 * @brief Outer varnish shell settings expressed in physical units.
 */
struct OuterVarnishSettings
{
    bool enabled{false};
    double thicknessmm{0.0};
    double pixelpitchum{42.3};
};

/**
 * @brief Preview generation settings for the current slicing session.
 */
struct PreviewSettings
{
    QString outputpolicy{QStringLiteral("tiff_native")};
    bool enabled{false};
    int interval{10};
};

/**
 * @brief UI-control-independent state edited by the Qt slicing workbench.
 */
struct SliceSettingsState
{
    QString profileid;
    QString modelpath;
    QString outputdirectory;
    int dpix{slicer_core::kDefaultOutputDpiX};
    int dpiy{slicer_core::kDefaultOutputDpiY};
    double layerthicknessmm{slicer_core::kDefaultLayerThicknessMm};
    ModelFillMaterial modelfillmaterial{ModelFillMaterial::Rgb};
    SupportSettings support;
    SurfaceVarnishSettings surfacevarnish;
    OuterVarnishSettings outervarnish;
    PreviewSettings preview;
    SliceEngineRole enginerole{SliceEngineRole::LegacyProduction};
};

/**
 * @brief Validation result for one UI slicing settings state.
 */
struct SliceSettingsValidationResult
{
    QStringList warnings;
    QStringList errors;

    /**
     * @brief Determine whether the state can proceed to effective config generation.
     * @return true when no validation errors exist.
     */
    bool IsValid() const;
};

/**
 * @brief Owns slicing settings independently from visual controls.
 */
class SliceSettingsModel final
{
public:
    /**
     * @brief Construct a model with safe legacy production defaults.
     */
    SliceSettingsModel();

    /**
     * @brief Apply defaults for one stable Profile id.
     * @param profileId Stable Profile id from ScenarioRegistry.
     * @return true when the Profile is recognized; otherwise state is unchanged.
     */
    bool ApplyProfileDefaults(const QString& profileId);

    /**
     * @brief Replace the complete settings state.
     * @param state New widget-independent state.
     */
    void SetState(const SliceSettingsState& state);

    /**
     * @brief Return the current immutable settings state.
     * @return Current settings state.
     */
    const SliceSettingsState& State() const;

    /**
     * @brief Validate settings before generated effective config creation.
     * @return Validation errors and non-blocking warnings.
     */
    SliceSettingsValidationResult Validate() const;

private:
    SliceSettingsState m_state;
};
