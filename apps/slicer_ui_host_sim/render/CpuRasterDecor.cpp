#include "CpuRasterDecor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cpu_raster_detail
{
namespace
{
using Matrix = std::array<float, 16>;

struct Point final
{
    float x{0.0F};
    float y{0.0F};
};

Matrix Multiply(const Matrix& a, const Matrix& b)
{
    Matrix result{};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int item = 0; item < 4; ++item)
            {
                result.at(static_cast<std::size_t>(row * 4 + column)) +=
                    a.at(static_cast<std::size_t>(row * 4 + item))
                    * b.at(static_cast<std::size_t>(item * 4 + column));
            }
        }
    }
    return result;
}

bool Project(
    const Matrix& matrix,
    const float x,
    const float y,
    const float z,
    const slicer::render::ImageOut& output,
    Point* point)
{
    const std::array<float, 4> value{x, y, z, 1.0F};
    std::array<float, 4> clip{};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            clip.at(static_cast<std::size_t>(row)) +=
                matrix.at(static_cast<std::size_t>(row * 4 + column))
                * value.at(static_cast<std::size_t>(column));
        }
    }
    if (point == nullptr || std::abs(clip[3]) < 1.0e-7F)
    {
        return false;
    }
    const float inverseW = 1.0F / clip[3];
    point->x = (clip[0] * inverseW * 0.5F + 0.5F)
        * static_cast<float>(output.widthPx - 1U);
    point->y = (0.5F - clip[1] * inverseW * 0.5F)
        * static_cast<float>(output.heightPx - 1U);
    return std::isfinite(point->x) && std::isfinite(point->y);
}

void DrawLine(
    slicer::render::ImageOut* output,
    const Point& a,
    const Point& b,
    const std::array<std::uint8_t, 4>& color)
{
    const float deltaX = b.x - a.x;
    const float deltaY = b.y - a.y;
    const int steps = static_cast<int>(std::ceil((std::max)(
        std::abs(deltaX), std::abs(deltaY))));
    for (int step = 0; step <= steps && steps > 0; ++step)
    {
        const float factor = static_cast<float>(step)
            / static_cast<float>(steps);
        const int x = static_cast<int>(std::lround(a.x + deltaX * factor));
        const int y = static_cast<int>(std::lround(a.y + deltaY * factor));
        if (x >= 0 && y >= 0 && x < static_cast<int>(output->widthPx)
            && y < static_cast<int>(output->heightPx))
        {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * output->widthPx
                 + static_cast<std::size_t>(x)) * 4U;
            std::copy(color.begin(), color.end(), output->rgba8.begin() + offset);
        }
    }
}

void DrawWorldLine(
    const Matrix& viewProjection,
    slicer::render::ImageOut* output,
    const std::array<float, 3>& from,
    const std::array<float, 3>& to,
    const std::array<std::uint8_t, 4>& color)
{
    Point a;
    Point b;
    if (Project(viewProjection, from[0], from[1], from[2], *output, &a)
        && Project(viewProjection, to[0], to[1], to[2], *output, &b))
    {
        DrawLine(output, a, b, color);
    }
}
}

void DrawSceneDecor(
    const slicer::render::FrameDesc& frame,
    slicer::render::ImageOut* output)
{
    Matrix view{};
    Matrix projection{};
    std::copy(frame.camera.viewMatrix, frame.camera.viewMatrix + 16, view.begin());
    std::copy(frame.camera.projMatrix, frame.camera.projMatrix + 16,
              projection.begin());
    const Matrix viewProjection = Multiply(projection, view);
    const float width = frame.decor.buildVolumeMm[0];
    const float height = frame.decor.buildVolumeMm[1];
    const float spacing = frame.decor.gridMinorMm;
    if (frame.decor.showGrid && spacing > 0.0F)
    {
        const int xLines = static_cast<int>(std::floor(width / spacing));
        const int yLines = static_cast<int>(std::floor(height / spacing));
        for (int index = 0; index <= xLines; ++index)
        {
            const float x = static_cast<float>(index) * spacing;
            const bool major = frame.decor.gridMajorMm > 0.0F
                && std::fmod(x, frame.decor.gridMajorMm) < 0.001F;
            DrawWorldLine(viewProjection, output, {x, 0.0F, 0.0F},
                {x, height, 0.0F}, major
                    ? std::array<std::uint8_t, 4>{111U, 116U, 126U, 255U}
                    : std::array<std::uint8_t, 4>{83U, 87U, 94U, 255U});
        }
        for (int index = 0; index <= yLines; ++index)
        {
            const float y = static_cast<float>(index) * spacing;
            const bool major = frame.decor.gridMajorMm > 0.0F
                && std::fmod(y, frame.decor.gridMajorMm) < 0.001F;
            DrawWorldLine(viewProjection, output, {0.0F, y, 0.0F},
                {width, y, 0.0F}, major
                    ? std::array<std::uint8_t, 4>{111U, 116U, 126U, 255U}
                    : std::array<std::uint8_t, 4>{83U, 87U, 94U, 255U});
        }
    }
    if (frame.decor.showBuildVolume)
    {
        const std::array<std::uint8_t, 4> color{148U, 156U, 170U, 255U};
        DrawWorldLine(viewProjection, output, {0.0F, 0.0F, 0.0F},
                      {width, 0.0F, 0.0F}, color);
        DrawWorldLine(viewProjection, output, {width, 0.0F, 0.0F},
                      {width, height, 0.0F}, color);
        DrawWorldLine(viewProjection, output, {width, height, 0.0F},
                      {0.0F, height, 0.0F}, color);
        DrawWorldLine(viewProjection, output, {0.0F, height, 0.0F},
                      {0.0F, 0.0F, 0.0F}, color);
    }
    if (frame.decor.showAxes)
    {
        const float axisLength = (std::max)(5.0F, (std::min)({
            width, height, frame.decor.buildVolumeMm[2]}) * 0.15F);
        DrawWorldLine(viewProjection, output, {0.0F, 0.0F, 0.0F},
                      {axisLength, 0.0F, 0.0F}, {218U, 76U, 76U, 255U});
        DrawWorldLine(viewProjection, output, {0.0F, 0.0F, 0.0F},
                      {0.0F, axisLength, 0.0F}, {89U, 190U, 107U, 255U});
        DrawWorldLine(viewProjection, output, {0.0F, 0.0F, 0.0F},
                      {0.0F, 0.0F, axisLength}, {86U, 139U, 214U, 255U});
    }
}

}  // namespace cpu_raster_detail
