#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

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
