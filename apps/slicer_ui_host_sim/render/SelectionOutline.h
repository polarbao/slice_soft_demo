#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace slicer::render
{
// Screen-space silhouette, independent of the underlying material colors.
inline void DrawSelectionOutline(std::uint8_t* rgba, int width, int height,
    const std::vector<std::uint8_t>& mask)
{
    if (mask.empty()) return;
    // Only the exterior silhouette is a selection border. Transparent artwork
    // and internal raster gaps must not paint contours over the model surface.
    std::vector<std::uint8_t> exterior(mask.size());
    std::vector<std::size_t> pending;
    const auto visit=[&](std::size_t p) {
        if(!mask[p] && !exterior[p]) { exterior[p]=1; pending.push_back(p); }
    };
    for(int x=0;x<width;++x) { visit(x); visit(static_cast<std::size_t>(height-1)*width+x); }
    for(int y=0;y<height;++y) { visit(static_cast<std::size_t>(y)*width); visit(static_cast<std::size_t>(y)*width+width-1); }
    for(std::size_t i=0;i<pending.size();++i)
    {
        const auto p=pending[i]; const auto x=p%width; const auto y=p/width;
        if(x>0) visit(p-1); if(x+1<static_cast<std::size_t>(width)) visit(p+1);
        if(y>0) visit(p-width); if(y+1<static_cast<std::size_t>(height)) visit(p+width);
    }
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const auto at = static_cast<std::size_t>(y) * width + x;
            if (!mask[at]) continue;
            const bool edge = x == 0 || y == 0 || x+1 == width || y+1 == height
                || exterior[at-1] || exterior[at+1] || exterior[at-width] || exterior[at+width];
            if (!edge) continue;
            for (int ny=(std::max)(0,y-2);ny<(std::min)(height,y+3);++ny)
                for (int nx=(std::max)(0,x-2);nx<(std::min)(width,x+3);++nx)
            {
                const auto pixel=static_cast<std::size_t>(ny)*width+nx;
                const std::uint8_t color[4]{mask[pixel] ? std::uint8_t{32} : std::uint8_t{20},
                    mask[pixel] ? std::uint8_t{144} : std::uint8_t{25},
                    mask[pixel] ? std::uint8_t{255} : std::uint8_t{30}, 255};
                std::copy_n(color, 4, rgba + pixel * 4);
            }
        }
}
}
