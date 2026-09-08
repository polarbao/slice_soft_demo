#include "slicer_core/output/rgbwsvt/LegacyTransferCanvas.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtPackageReader.h"
#include "slicer_core/system/Utf8Path.h"
#include "slicer_core/json_value.h"
#include <fstream>
#include <iostream>

namespace
{
using namespace slicer_core;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
Json Read(const std::filesystem::path& path) { std::ifstream input(path); return Json::parse(input); }
struct Grid
{
    int width_px{3}, height_px{2};
    double origin_x_mm{20.07984}, pixel_size_x_mm{25.4 / 600};
};
}

void CheckXPaddingTransferHelper()
{
    for (double x : {-1.0, 0.0, 0.01, 20.07984})
    {
        Grid grid;
        grid.origin_x_mm = x;
        for (bool enabled : {false, true})
        {
            LegacyTransferCanvas canvas(grid, enabled);
            const auto output = canvas.OutputGrid(grid);
            const int n = enabled && x > 0 ? static_cast<int>(std::ceil(x / grid.pixel_size_x_mm)) : 0;
            Require(output.width_px == grid.width_px + n && output.height_px == grid.height_px, "T helper extent");
            Require(std::abs(output.origin_x_mm + n * grid.pixel_size_x_mm - x) < 1e-9, "T helper pixel phase");
            RgbwsvtProductionLayer layer;
            layer.widthPx = grid.width_px;
            layer.heightPx = grid.height_px;
            layer.channels.resize(42);
            for (std::size_t i = 0; i < layer.channels.size(); ++i) layer.channels[i] = static_cast<std::uint8_t>(i);
            const auto original = layer.channels;
            canvas.Apply(layer);
            for (std::size_t y = 0; y < 2; ++y)
            {
                const auto begin = layer.channels.begin() + static_cast<std::ptrdiff_t>(y) * output.width_px * 7;
                Require(std::all_of(begin, begin + n * 7, [](auto v) { return v == 255; }), "T helper prefix");
                Require(std::equal(original.begin() + y * 21, original.begin() + (y + 1) * 21, begin + n * 7), "T helper body");
            }
            std::vector<std::uint8_t> mask{1,0,1,0,1,1};
            canvas.PadPreviewMask(mask);
            Require(mask.size() == static_cast<std::size_t>(output.width_px) * 2, "preview mask dimensions");
            for (std::size_t y = 0; y < 2; ++y)
                Require(std::all_of(mask.begin() + y * output.width_px,
                    mask.begin() + y * output.width_px + n, [](auto v) { return v == 0; }), "preview mask prefix");
        }
    }
    Grid overflow;
    overflow.origin_x_mm = 1e20;
    bool rejected = false;
    try { LegacyTransferCanvas canvas(overflow, true); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "T overflow accepted");
    LegacyTransferCanvas canvas(Grid{}, true);
    RgbwsvtProductionLayer invalid;
    rejected = false;
    try { canvas.Apply(invalid); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "invalid T layer accepted");
}

slicer_core::Json MakeXPaddingTransferProfile(const std::filesystem::path&,
    const std::filesystem::path& model, const std::filesystem::path& package, bool pad)
{
    return Json::object({
        {"input", Json::object({{"format", "obj"}, {"modelPath", PathToUtf8(model)}})},
        {"output", Json::object({{"packageDir", PathToUtf8(package)}, {"packageProtocol", "p0.rgbwsvt.1"},
            {"channelOrder", Json::array({"R","G","B","W","S","V","T"})},
            {"dpiX", 600}, {"dpiY", 600}, {"layerThicknessMm", 0.2}, {"scenePadToOriginX", pad}})},
        {"autoOrient", Json::object({{"enabled", false}})},
        {"transferChannelPolicy", Json::object({{"enabled", true}, {"matchSource", "material_diffuse_rgb"},
            {"materialDiffuseRgbValues", Json::array({Json::array({255,220,198})})},
            {"missingRegion", "fail_closed"}, {"multipleMatches", "fail_closed"}, {"value", 0},
            {"topology", Json::object({{"selfIntersectionPolicy", "tolerate_closed_self_intersection"}, {"maxSelfIntersectionPairs", 64}})}})},
        {"preview", Json::object({{"enabled", true}, {"format", "ppm"}, {"interval", 1},
            {"channels", Json::array({"transfer", "rgb"})}})}
    });
}

void CompareXPaddingTransfer(const std::filesystem::path& off, const std::filesystem::path& on)
{
    const auto a = ValidateRgbwsvtPackage(off);
    const auto b = ValidateRgbwsvtPackage(on);
    const auto am = Read(off / "manifest.json"), bm = Read(on / "manifest.json");
    const double x = am.at("grid").at("originMm").at(0U).as_double();
    const auto n = static_cast<int>(std::ceil(x / (25.4 / a.dpiX)));
    Require(n > 0 && b.widthPx == a.widthPx + n && b.heightPx == a.heightPx && b.layerCount == a.layerCount,
        "T package grid dimensions differ");
    Require(b.productionAcceptance == "admitted", "T must exercise admitted scene route");
    Require(std::abs(bm.at("grid").at("originMm").at(0U).as_double() + n * (25.4/a.dpiX) - x) < 1e-9, "T package origin");
    Require(a.totalChannelStats[6].print_pixels > 0, "T fixture must print T");
    for (std::size_t c = 0; c < 7; ++c)
    {
        Require(a.totalChannelStats[c].print_pixels == b.totalChannelStats[c].print_pixels, "T print statistics drift");
        Require(b.totalChannelStats[c].empty_pixels == a.totalChannelStats[c].empty_pixels
            + static_cast<std::uint64_t>(n) * a.heightPx * a.layerCount, "T empty statistics drift");
    }
    for (std::size_t z = 0; z < a.layers.size(); ++z)
    {
        const auto al = ReadRgbwsvtPackageLayer(a.layers[z]), bl = ReadRgbwsvtPackageLayer(b.layers[z]);
        for (std::size_t y = 0; y < static_cast<std::size_t>(a.heightPx); ++y)
        {
            const auto ar = al.pixels.begin() + y * a.widthPx * 7;
            const auto br = bl.pixels.begin() + y * b.widthPx * 7;
            Require(std::all_of(br, br + n * 7, [](auto v) { return v == 255; }), "T TIFF prefix prints");
            Require(std::equal(ar, ar + a.widthPx * 7, br + n * 7), "T TIFF body drift");
        }
    }
    const auto sr = Read(on / "reports/slice_report.json");
    Require(sr.at("grid").at("widthPx").as_int() == b.widthPx, "T slice report width");
    const auto material = Read(on / "reports/material_process_report.json");
    Require(std::abs(material.at("transfer").at("coverageRatio").as_double()
        - static_cast<double>(b.totalChannelStats[6].print_pixels) / (static_cast<double>(b.widthPx) * b.heightPx * b.layerCount)) < 1e-9,
        "T material coverage denominator");
    std::size_t previews = 0;
    for (const auto& entry : std::filesystem::directory_iterator(on / "preview"))
    {
        if (entry.path().extension() != ".ppm") continue;
        std::ifstream file(entry.path(), std::ios::binary);
        std::string magic;
        int w, h, depth;
        file >> magic >> w >> h >> depth;
        Require(magic == "P6" && w == b.widthPx && h == b.heightPx && depth == 255, "T preview extent");
        file.get();
        const std::vector<char> paddedBytes{std::istreambuf_iterator<char>(file), {}};
        std::ifstream originalFile(off / "preview" / entry.path().filename(), std::ios::binary);
        originalFile >> magic >> w >> h >> depth;
        Require(w == a.widthPx && h == a.heightPx, "original preview extent");
        originalFile.get();
        const std::vector<char> originalBytes{std::istreambuf_iterator<char>(originalFile), {}};
        Require(paddedBytes.size() == static_cast<std::size_t>(b.widthPx) * b.heightPx * 3
            && originalBytes.size() == static_cast<std::size_t>(a.widthPx) * a.heightPx * 3, "preview payload size");
        for (std::size_t y = 0; y < static_cast<std::size_t>(a.heightPx); ++y)
        {
            const auto ar = originalBytes.begin() + y * a.widthPx * 3;
            const auto br = paddedBytes.begin() + y * b.widthPx * 3;
            Require(std::equal(ar, ar + a.widthPx * 3, br + n * 3), "preview body drift");
            for (int col = 1; col < n; ++col)
                Require(std::equal(br, br + 3, br + col * 3), "preview empty prefix not uniform");
        }
        ++previews;
    }
    Require(previews > 0, "T preview not exercised");
    std::cout << "XPAD_T_PACKAGE_PASS layers=" << b.layerCount << " width=" << a.widthPx << "->" << b.widthPx
        << " columns=" << n << " T=" << b.totalChannelStats[6].print_pixels << '\n';
}
