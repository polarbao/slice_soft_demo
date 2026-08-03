#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

/**
 * @brief Supported production materials for a single-material relief model.
 */
enum class SingleMaterialReliefMaterial
{
    White,
    Varnish,
};

/**
 * @brief Stable fail-closed errors emitted by the single-material resolver.
 */
enum class SingleMaterialReliefErrorCode
{
    None,
    UnsupportedProfile,
    InvalidMaterial,
    ConfigConflict,
    ProfileLocked,
};

/**
 * @brief UI-independent requested and effective single-material state.
 */
struct SingleMaterialReliefState
{
    QString profileid;
    SingleMaterialReliefMaterial requestedmaterial{
        SingleMaterialReliefMaterial::White};
    QString effectivechannel{QStringLiteral("W")};
    bool editable{false};
    bool valid{false};
    bool stale{false};
    SingleMaterialReliefErrorCode errorcode{
        SingleMaterialReliefErrorCode::None};
    QString lockreason;
    QStringList issues;
};

/**
 * @brief Result of atomically applying one single-material relief state.
 */
struct SingleMaterialReliefApplyResult
{
    bool applied{false};
    QJsonObject config;
    SingleMaterialReliefState state;
    QString errorcode;
    QStringList issues;
};

/**
 * @brief Resolves the complete W/V field group for single-material relief.
 */
class SingleMaterialReliefResolver final
{
public:
    /**
     * @brief Read and validate the effective single-material configuration.
     * @param config Effective configuration root.
     * @param profileId Stable UI Profile id.
     * @param editable Whether the current Profile permits production edits.
     * @param stale Whether the current package is already stale.
     * @return Validated requested/effective state.
     */
    static SingleMaterialReliefState Read(
        const QJsonObject& config,
        const QString& profileId,
        bool editable,
        bool stale);

    /**
     * @brief Return a state with a new requested single material.
     * @param current Current validated state.
     * @param requestedMaterial Requested white or varnish material.
     * @return Updated state; changing material marks the output stale.
     */
    static SingleMaterialReliefState Update(
        const SingleMaterialReliefState& current,
        SingleMaterialReliefMaterial requestedMaterial);

    /**
     * @brief Apply the complete W/V production field group atomically.
     * @param config Source configuration.
     * @param state Requested production state.
     * @return Applied configuration or a fail-closed unchanged source.
     */
    static SingleMaterialReliefApplyResult Apply(
        const QJsonObject& config,
        const SingleMaterialReliefState& state);

    /**
     * @brief Return the stable string representation of an error code.
     * @param errorCode Resolver error code.
     * @return Stable error code string.
     */
    static QString ErrorCodeValue(
        SingleMaterialReliefErrorCode errorCode);
};
