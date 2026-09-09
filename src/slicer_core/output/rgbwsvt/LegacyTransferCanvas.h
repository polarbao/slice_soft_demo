#pragma once

#include "slicer_core/output/rgbwsvt/RgbwsvtProtocol.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace slicer_core
{
/** @brief Output-only padding. Sampling and material ownership keep the original grid. */
class LegacyTransferCanvas
{
public:
    template<class Grid>
    LegacyTransferCanvas(const Grid& grid, bool enableX, bool enableY = false, bool scene = true)
        : width_(grid.width_px), height_(grid.height_px), origin_(grid.origin_x_mm), originY_(grid.origin_y_mm)
    {
        if (!scene || (!enableX && !enableY)) return;
        columns_ = ExtendAxis(enableX, origin_, grid.pixel_size_x_mm, width_);
        rows_ = ExtendAxis(enableY, originY_, grid.pixel_size_y_mm, height_);
        (void)ByteCount(kRgbwsvtChannelCount);
    }

    template<class Grid>
    Grid OutputGrid(Grid grid) const
    {
        grid.width_px += columns_;
        grid.height_px += rows_;
        grid.origin_x_mm = origin_;
        grid.origin_y_mm = originY_;
        return grid;
    }

    void Apply(RgbwsvtProductionLayer& layer) const
    {
        if (columns_ == 0 && rows_ == 0) return;
        if (layer.widthPx != width_ || layer.heightPx != height_
            || layer.channelOrder != CurrentRgbwsvtProtocol().channelOrder)
            throw std::runtime_error("X-origin transfer layer differs from sampling grid");
        Pad(layer.channels, kRgbwsvtChannelCount, 255U);
        layer.widthPx += columns_;
        layer.heightPx += rows_;
    }

    void PadPreviewMask(std::vector<std::uint8_t>& mask) const { Pad(mask, 1U, 0U); }

private:
    static int ExtendAxis(bool enabled, double& origin, double pitch, int extent)
    {
        if (extent <= 0 || !std::isfinite(origin) || !std::isfinite(pitch) || pitch <= 0)
            throw std::runtime_error("origin transfer sampling grid is invalid");
        if (!enabled || origin <= 0) return 0;
        const double pixels = std::ceil(origin / pitch);
        if (!std::isfinite(pixels) || pixels <= 0 || pixels > std::numeric_limits<int>::max() - extent)
            throw std::runtime_error("origin transfer canvas exceeds supported extent");
        origin -= pixels * pitch;
        if (!std::isfinite(origin) || origin > 0 || origin <= -pitch)
            throw std::runtime_error("origin transfer canvas has invalid pixel phase");
        return static_cast<int>(pixels);
    }

    std::size_t ByteCount(std::size_t channels) const
    {
        const auto row = static_cast<std::size_t>(width_ + columns_) * channels;
        if (height_ <= 0 || width_ <= 0 || row > std::vector<std::uint8_t>().max_size()
                / static_cast<std::size_t>(height_ + rows_))
            throw std::runtime_error("X-origin transfer canvas byte count overflow");
        return row * static_cast<std::size_t>(height_ + rows_);
    }

    void Pad(std::vector<std::uint8_t>& bytes, std::size_t channels, std::uint8_t empty) const
    {
        if (columns_ == 0 && rows_ == 0) return;
        const auto oldRow = static_cast<std::size_t>(width_) * channels;
        if (bytes.size() != oldRow * static_cast<std::size_t>(height_))
            throw std::runtime_error("X-origin transfer input byte count mismatch");
        const auto prefix = static_cast<std::size_t>(columns_) * channels;
        const auto newRow = oldRow + prefix;
        const auto bottom = static_cast<std::size_t>(rows_) * newRow;
        bytes.resize(ByteCount(channels));
        // Back-to-front keeps unread rows intact, with no second full-layer buffer.
        for (std::size_t y = static_cast<std::size_t>(height_); y-- > 0;)
        {
            auto* data = bytes.data();
            std::move_backward(data + y * oldRow, data + (y + 1) * oldRow, data + bottom + (y + 1) * newRow);
            std::fill_n(data + bottom + y * newRow, prefix, empty);
        }
        // Internal minY-first rows become the bottom blank band in maxY-first TIFF/preview.
        std::fill_n(bytes.data(), bottom, empty);
    }

    int width_;
    int height_;
    int columns_{0};
    int rows_{0};
    double origin_;
    double originY_;
};
}
