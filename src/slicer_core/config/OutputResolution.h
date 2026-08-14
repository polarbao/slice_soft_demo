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
 * @brief 检查一个输出轴 DPI 值是否受支持。
 * @param dpi 一个栅格轴的输出分辨率。
 * @return dpi 位于统一的防御性取值范围内时返回 true。
 */
bool IsSupportedOutputDpi(int dpi) noexcept;

/**
 * @brief 检查物理像素尺寸是否与一个输出轴 DPI 匹配。
 * @param dpi 一个栅格轴的输出分辨率。
 * @param pixelSizeMm 以毫米表示的物理像素尺寸。
 * @return 两个值均有效且在协议容差内一致时返回 true。
 */
bool IsOutputPixelSizeConsistent(
    int dpi,
    double pixelSizeMm) noexcept;

}  // namespace slicer_core
