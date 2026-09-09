#include "slicer_core/pipeline/MultiModelSliceOrchestrator.h"
#include "slicer_core/config.h"
#include "slicer_core/config/TransferChannelConfig.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
using namespace slicer_core;
void Require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
MultiModelLayerComposeRequest Fixture(double x, double pitch, bool second, double y = 4.1, double pitchY = 0.2, bool varyY = false)
{
    MultiModelLayerComposeRequest request;
    request.currentscenerevision = 1;
    request.quantizationtolerance = 0.500001;
    request.admission.sceneid = "xpad";
    request.admission.sourcescenerevision = 1;
    request.admission.purpose = SceneValidationPurpose::FunctionalFixture;
    request.admission.scenestatus = SceneCollisionStatus::Passed;
    request.admission.functionalallowed = true;
    for (int i = 0; i < (second ? 2 : 1); ++i)
    {
        SceneInstanceRaster raster;
        raster.sceneid = "xpad";
        raster.modelid = "model" + std::to_string(i);
        raster.instanceid = "instance" + std::to_string(i);
        raster.scenerevision = 1;
        raster.transformrevision = raster.admittedtransformrevision = 1;
        raster.transformhash = raster.admittedtransformhash = "identity";
        raster.admitted = true;
        raster.protocol = FixedSceneRasterProtocol();
        raster.localgrid = {3, 2, 3, x + i * 6.5 * pitch, y + (varyY ? i * 4.5 * pitchY : 0), 0, pitch, pitchY, 0.1};
        for (int z = 0; z < 3; ++z)
        {
            SceneInstanceRasterLayer layer;
            layer.layerindex = z;
            layer.zmm = (z + 0.5) * 0.1;
            layer.output.layerIndex = z;
            layer.output.zMm = layer.zmm;
            layer.output.widthPx = 3;
            layer.output.heightPx = 2;
            layer.output.channelOrder = raster.protocol.channel_order;
            layer.output.channels.assign(36, 255);
            layer.modelownership.assign(6, 0);
            layer.modelvarnishownership.assign(6, 0);
            layer.outervarnishownership.assign(6, 0);
            layer.supportownership.assign(6, 0);
            layer.modelownership[1] = 1;
            layer.output.channels[6] = static_cast<std::uint8_t>(50 + z);
            layer.supportownership[3] = 1;
            layer.output.channels[3 * 6 + 4] = 0;
            raster.layers.push_back(std::move(layer));
        }
        SceneCollisionInstanceResult admitted;
        admitted.modelid = raster.modelid;
        admitted.instanceid = raster.instanceid;
        admitted.visible = admitted.boundsvalid = admitted.inbounds = true;
        admitted.admissionstatus = SceneInstanceAdmissionStatus::Admitted;
        admitted.transformrevision = 1;
        admitted.transformhash = "identity";
        request.admission.instances.push_back(admitted);
        request.instances.push_back(std::move(raster));
    }
    request.admission.statistics.totalinstancecount = request.instances.size();
    request.admission.statistics.visibleinstancecount = request.instances.size();
    return request;
}
void Compare(const RgbwsvProductionLayer& base, const RgbwsvProductionLayer& padded, int n, int m)
{
    Require(padded.widthPx == base.widthPx + n && padded.heightPx == base.heightPx + m,
        "padding dimensions");
    Require(padded.zMm == base.zMm && padded.layerIndex == base.layerIndex, "Z unchanged");
    for (int y = 0; y < padded.heightPx; ++y)
    {
        const auto row = padded.channels.begin() + static_cast<std::ptrdiff_t>(y) * padded.widthPx * 6;
        if (y < m)
        {
            Require(std::all_of(row, row + padded.widthPx * 6, [](auto v) { return v == 255; }), "bottom rows print");
            continue;
        }
        Require(std::all_of(row, row + n * 6, [](auto v) { return v == 255; }), "empty prefix");
        const auto original = base.channels.begin() + static_cast<std::ptrdiff_t>(y - m) * base.widthPx * 6;
        Require(std::equal(original, original + base.widthPx * 6, row + n * 6), "body byte drift");
    }
}
void Check(double x, double pitch, bool second, double y = 4.1, double pitchY = 0.2, bool padX = true, bool padY = false)
{
    auto request = Fixture(x, pitch, second, y, pitchY, padY);
    const auto base = ComposeAdmittedSceneRasters(request);
    Require(base.IsValid(), "base composition failed");
    request.padtooriginx = padX;
    request.padtooriginy = padY;
    const auto padded = ComposeAdmittedSceneRasters(request);
    Require(padded.IsValid(), "padded composition failed");
    const int n = padX && x > 0 ? static_cast<int>(std::ceil(x / pitch)) : 0;
    const int m = padY && y > 0 ? static_cast<int>(std::ceil(y / pitchY)) : 0;
    Require(std::abs(padded.grid.originxmm + n * pitch - x) < 1e-9, "phase drift");
    Require(std::abs(padded.grid.originymm + m * pitchY - y) < 1e-9, "Y phase drift");
    Require(padded.statistics.modelpixels == base.statistics.modelpixels
        && padded.statistics.supportpixels == base.statistics.supportpixels, "print statistics drift");
    if (padded.grid.widthpx != base.grid.widthpx + n)
        std::cerr << "width off=" << base.grid.widthpx << " on=" << padded.grid.widthpx << " n=" << n << '\n';
    Require(padded.statistics.emptypixels == base.statistics.emptypixels
        + (static_cast<std::size_t>(padded.grid.widthpx) * padded.grid.heightpx
            - static_cast<std::size_t>(base.grid.widthpx) * base.grid.heightpx) * 3, "empty statistics");
    for (std::size_t z = 0; z < base.layers.size(); ++z) Compare(base.layers[z], padded.layers[z], n, m);
    const auto owning = ComposeAdmittedSceneRasters(MultiModelLayerComposeRequest(request));
    Require(owning.IsValid(), "owned padding failed");
    for (std::size_t z = 0; z < base.layers.size(); ++z) Compare(base.layers[z], owning.layers[z], n, m);
    auto inputs = request.instances;
    for (auto& instance : request.instances) instance.layers.clear();
    request.layerprovider = [&inputs](const SceneInstanceRaster& instance, int layer) {
        for (const auto& input : inputs)
            if (input.instanceid == instance.instanceid) return &input.layers.at(static_cast<std::size_t>(layer));
        return static_cast<const SceneInstanceRasterLayer*>(nullptr);
    };
    int count = 0;
    request.layersink = [&](int z, RgbwsvProductionLayer&& layer, const auto&) {
        Compare(base.layers.at(static_cast<std::size_t>(z)), layer, n, m);
        ++count;
    };
    const auto streamed = ComposeAdmittedSceneRastersValidated(std::move(request));
    Require(streamed.IsValid() && count == 3 && streamed.Value().layers.empty(), "streaming padding failed");
}
}
void RunXPaddingProduction(bool real, int dpi, double thickness, bool benchmark, bool padY = false, bool padX = true);
void RunXPaddingTransfer(bool real, bool benchmark, bool padY = false, bool padX = true);
int main(int argc, char** argv)
{
    try
    {
        for (const double pitch : {0.2, 25.4 / 600.0, 25.4 / 635.0})
            for (const double x : {0.0, -0.3, 0.01, 20.07984, 50.0})
                for (const bool second : {false, true}) Check(x, pitch, second);
        for (const double y : {-0.3, 0.0, 0.001, 1.506456, 2.0})
            for (const bool x : {false, true})
                for (const bool second : {false, true})
                    Check(0.3, 25.4 / 635, second, y, 25.4 / 600, x, true);
        Check(0.3, 0.2, true, 1.5, 0.1, false, false);
        auto overflow = Fixture(static_cast<double>(std::numeric_limits<int>::max()), 0.1, false);
        overflow.padtooriginx = true;
        Require(!ComposeAdmittedSceneRasters(overflow).IsValid(), "overflow must fail closed");
        overflow = Fixture(0.1, 0.2, false, 1e20);
        overflow.padtooriginy = true;
        Require(!ComposeAdmittedSceneRasters(overflow).IsValid(), "Y overflow must fail closed");
        OutputConfig output;
        output.scene_pad_to_origin_x = true;
        output.package_protocol = "p0.rgbwsvt.1";
        output.channel_order = {"R", "G", "B", "W", "S", "V", "T"};
        TransferChannelPolicyConfig policy;
        policy.enabled = true;
        policy.material_diffuse_rgb_values = {{255, 220, 198}};
        ValidateTransferChannelConfiguration(output, policy);
        const SceneCanvasAxisPadding invalid{-1, 2};
        Require(!invalid.IsValid(-0.1, 0.2, 4), "invalid padding evidence");
        const std::string mode = argc > 1 ? argv[1] : "";
        const bool xy = mode.ends_with("-xy");
        RunXPaddingTransfer(mode == "--real-t" || mode == "--bench-t" || mode == "--real-t-xy", mode == "--bench-t", xy);
        if (mode != "--real-t" && mode != "--bench-t" && mode != "--real-t-xy")
            RunXPaddingProduction(mode == "--real" || mode == "--bench" || mode == "--real-xy",
                argc > 2 ? std::stoi(argv[2]) : 127, argc > 3 ? std::stod(argv[3]) : 0.5, mode == "--bench", xy);
        if (mode.empty())
            for (bool x : {false, true})
            {
                RunXPaddingTransfer(false, false, true, x);
                RunXPaddingProduction(false, 127, 0.5, false, true, x);
            }
        std::cout << "XPAD_UNIT_PASS default/prefix/body/statistics/phase/streaming/overflow\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
