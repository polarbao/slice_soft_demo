#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Three-dimensional model bounds in millimetres. */
typedef struct HostBounds3
{
    double min[3];
    double max[3];
} HostBounds3;

/**
 * @brief Builds a complete Stage 13B scene document for one imported model.
 * @param modelPath Normalized absolute model path.
 * @param resourceRoot Normalized absolute OBJ resource directory.
 * @param sourceDigest Imported source digest.
 * @param resourceDigest Adjacent-resource identity for the imported model.
 * @param sourceBounds Imported local bounds.
 * @param effectiveBounds Effective world bounds after the encoded transform.
 * @param sceneRevision Scene revision to encode.
 * @param translateXMm Effective X translation.
 * @param translateYMm Effective Y translation.
 * @param transformRevision Instance transform revision.
 * @return Heap JSON string owned by the caller, or NULL on failure.
 */
char* HostBuildScene(
    const char* modelPath,
    const char* resourceRoot,
    const char* sourceDigest,
    const char* resourceDigest,
    const HostBounds3* sourceBounds,
    const HostBounds3* effectiveBounds,
    unsigned long long sceneRevision,
    double translateXMm,
    double translateYMm,
    unsigned long long transformRevision);

/**
 * @brief Computes the frozen resource identity for the untextured OBJ fixture.
 * @param modelPath Normalized absolute model path.
 * @param resourceDigest Receives 64 lowercase SHA-256 characters.
 * @param resourceDigestCapacity Output buffer capacity.
 * @return Non-zero on success.
 */
int HostComputeUntexturedObjResourceDigest(
    const char* modelPath,
    char* resourceDigest,
    unsigned long resourceDigestCapacity);

/**
 * @brief Builds a self-hashed effective Profile for the reference slice.
 * @param modelPath Normalized absolute model path.
 * @param packageDirectory Normalized absolute package directory.
 * @param profileHash Receives `sha256:` plus 64 lowercase hex characters.
 * @param profileHashCapacity Output buffer capacity.
 * @return Heap JSON string owned by the caller, or NULL on failure.
 */
char* HostBuildProfile(
    const char* modelPath,
    const char* packageDirectory,
    char* profileHash,
    unsigned long profileHashCapacity);

/**
 * @brief Builds a self-hashed effective Profile with an explicit layer height.
 * @param modelPath Normalized absolute model path.
 * @param packageDirectory Normalized absolute package directory.
 * @param layerThicknessMm Positive layer thickness in millimetres.
 * @param profileHash Receives `sha256:` plus 64 lowercase hex characters.
 * @param profileHashCapacity Output buffer capacity.
 * @return Heap JSON string owned by the caller, or NULL on failure.
 */
char* HostBuildProfileWithLayerThickness(
    const char* modelPath,
    const char* packageDirectory,
    double layerThicknessMm,
    char* profileHash,
    unsigned long profileHashCapacity);

#ifdef __cplusplus
}
#endif
