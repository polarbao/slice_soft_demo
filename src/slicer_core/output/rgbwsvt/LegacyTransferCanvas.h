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
    LegacyTransferCanvas(const Grid& grid, bool enabled)
        : width_(grid.width_px), height_(grid.height_px), origin_(grid.origin_x_mm)
    {
        if (!enabled) return;
        if (width_ <= 0 || height_ <= 0 || !std::isfinite(origin_)
            || !std::isfinite(grid.pixel_size_x_mm) || grid.pixel_size_x_mm <= 0)
            throw std::runtime_error("X-origin transfer sampling grid is invalid");
        if (origin_ <= 0) return;
        const double columns = std::ceil(origin_ / grid.pixel_size_x_mm);
        if (!std::isfinite(columns) || columns <= 0
            || columns > std::numeric_limits<int>::max() - width_)
            throw std::runtime_error("X-origin transfer canvas exceeds supported extent");
        columns_ = static_cast<int>(columns);
        origin_ -= columns * grid.pixel_size_x_mm;
        if (!std::isfinite(origin_) || origin_ > 0 || origin_ <= -grid.pixel_size_x_mm)
            throw std::runtime_error("X-origin transfer canvas has invalid pixel phase");
        (void)ByteCount(kRgbwsvtChannelCount);
    }

    template<class Grid>
    Grid OutputGrid(Grid grid) const
    {
        grid.width_px += columns_;
        grid.origin_x_mm = origin_;
        return grid;
    }

    void Apply(RgbwsvtProductionLayer& layer) const
    {
        if (columns_ == 0) return;
        if (layer.widthPx != width_ || layer.heightPx != height_
            || layer.channelOrder != CurrentRgbwsvtProtocol().channelOrder)
            throw std::runtime_error("X-origin transfer layer differs from sampling grid");
        Pad(layer.channels, kRgbwsvtChannelCount, 255U);
        layer.widthPx += columns_;
    }

    void PadPreviewMask(std::vector<std::uint8_t>& mask) const { Pad(mask, 1U, 0U); }

private:
    std::size_t ByteCount(std::size_t channels) const
    {
        const auto row = static_cast<std::size_t>(width_ + columns_) * channels;
        if (height_ <= 0 || width_ <= 0 || row > std::vector<std::uint8_t>().max_size()
                / static_cast<std::size_t>(height_))
            throw std::runtime_error("X-origin transfer canvas byte count overflow");
        return row * static_cast<std::size_t>(height_);
    }

    void Pad(std::vector<std::uint8_t>& bytes, std::size_t channels, std::uint8_t empty) const
    {
        if (columns_ == 0) return;
        const auto oldRow = static_cast<std::size_t>(width_) * channels;
        if (bytes.size() != oldRow * static_cast<std::size_t>(height_))
            throw std::runtime_error("X-origin transfer input byte count mismatch");
        const auto prefix = static_cast<std::size_t>(columns_) * channels;
        const auto newRow = oldRow + prefix;
        bytes.resize(ByteCount(channels));
        // Back-to-front keeps unread rows intact, with no second full-layer buffer.
        for (std::size_t y = static_cast<std::size_t>(height_); y-- > 0;)
        {
            auto* data = bytes.data();
            std::move_backward(data + y * oldRow, data + (y + 1) * oldRow, data + (y + 1) * newRow);
            std::fill_n(data + y * newRow, prefix, empty);
        }
    }

    int width_;
    int height_;
    int columns_{0};
    double origin_;
};
}
