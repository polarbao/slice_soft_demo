#pragma once

namespace slicer_core {

inline constexpr int kDefaultOutputDpiX{635};
inline constexpr int kDefaultOutputDpiY{600};
inline constexpr double kDefaultLayerThicknessMm{0.038};
inline constexpr int kMinimumOutputDpi{72};
inline constexpr int kMaximumOutputDpi{2400};
inline constexpr double kMillimetersPerInch{25.4};
inline constexpr double kOutputPixelSizeToleranceMm{1.0e-9};

/**
 * @brief Check whether one output-axis DPI value is supported.
 * @param dpi Output resolution for one raster axis.
 * @return True when dpi is inside the shared defensive range.
 */
bool IsSupportedOutputDpi(int dpi) noexcept;

/**
 * @brief Check whether a physical pixel size matches one output-axis DPI.
 * @param dpi Output resolution for one raster axis.
 * @param pixelSizeMm Physical pixel size in millimeters.
 * @return True when both values are valid and consistent within protocol tolerance.
 */
bool IsOutputPixelSizeConsistent(
    int dpi,
    double pixelSizeMm) noexcept;

}  // namespace slicer_core
