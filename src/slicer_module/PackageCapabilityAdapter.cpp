#include "slicer_module/PackageCapabilityAdapter.h"

#include "slicer_module/ModelCapabilityAdapter.h"
#include "slicer_core/api/PackageQueryFacade.h"
#include "slicer_core/api/implementation/PackageQueryFacadeImplementation.h"

#include <array>
#include <filesystem>
#include <memory>

namespace slicesoft::module
{

class PackageCapabilityAdapter::Implementation final
{
public:
    Implementation()
        : m_facade(
            slicer_core::api::implementation::CreatePackageQueryFacade())
    {
    }

    [[nodiscard]] slicer_core::Json Execute(
        const std::string& capability,
        const slicer_core::Json& request) const
    {
        if (capability == "package.verify")
        {
            return Verify(request);
        }
        if (capability == "package.get_summary")
        {
            return GetSummary(request);
        }
        if (capability == "package.get_layer_descriptor")
        {
            return GetLayerDescriptor(request);
        }
        if (capability == "package.render_layer_preview")
        {
            return RenderLayerPreview(request);
        }
        if (capability == "package.read_report")
        {
            return ReadReport(request);
        }
        return MakeFailure(
            "PM-SLICER-INTERNAL-0099",
            "package capability adapter received an unsupported capability",
            capability);
    }

private:
    [[nodiscard]] static std::filesystem::path ReadPackageDir(
        const slicer_core::Json& request)
    {
        return RequireString(request, "packageDir");
    }

    [[nodiscard]] static slicer_core::Json MakeChannelCounts(
        const std::array<std::uint64_t, 6>& counts)
    {
        return slicer_core::Json::object({
            {"R", counts[0]},
            {"G", counts[1]},
            {"B", counts[2]},
            {"W", counts[3]},
            {"S", counts[4]},
            {"V", counts[5]}});
    }

    [[nodiscard]] slicer_core::Json Verify(
        const slicer_core::Json& request) const
    {
        NeverCancelToken cancelToken;
        const auto result = m_facade->Verify(ReadPackageDir(request), cancelToken);
        if (!result.IsOk())
        {
            return MakeFailure(*result.Error());
        }
        slicer_core::Json::Array errors;
        for (const auto& error : result.Value()->errors)
        {
            errors.emplace_back(slicer_core::Json::object({
                {"code", error.code},
                {"message", error.message}}));
        }
        slicer_core::Json::Array checksums;
        for (const auto& checksum : result.Value()->per_layer_checksum)
        {
            checksums.emplace_back(MakeNumberArray(checksum));
        }
        return MakeSuccess({
            {"valid", result.Value()->valid},
            {"errors", slicer_core::Json{std::move(errors)}},
            {"perLayerChecksum", slicer_core::Json{std::move(checksums)}},
            {"layerCount", result.Value()->layer_count}});
    }

    [[nodiscard]] slicer_core::Json GetSummary(
        const slicer_core::Json& request) const
    {
        const auto result = m_facade->GetSummary(ReadPackageDir(request));
        if (!result.IsOk())
        {
            return MakeFailure(*result.Error());
        }
        slicer_core::Json::Array channels;
        for (const std::string& channel : result.Value()->channels)
        {
            channels.emplace_back(channel);
        }
        slicer_core::Json::Array perInstance;
        for (const auto& instance : result.Value()->per_instance)
        {
            perInstance.emplace_back(ParseStructuredObject(instance));
        }
        return MakeSuccess({
            {"packageIdentity", result.Value()->package_identity},
            {"schema", result.Value()->schema},
            {"layerCount", result.Value()->layer_count},
            {"grid", slicer_core::Json::object({
                {"widthPx", result.Value()->grid.width_px},
                {"heightPx", result.Value()->grid.height_px},
                {"dpiX", result.Value()->grid.dpi_x},
                {"dpiY", result.Value()->grid.dpi_y}})},
            {"channels", slicer_core::Json{std::move(channels)}},
            {"bitDepth", result.Value()->bit_depth},
            {"polarity", result.Value()->polarity},
            {"perInstance", slicer_core::Json{std::move(perInstance)}},
            {"profileEcho", ParseStructuredObject(result.Value()->profile_echo)}});
    }

    [[nodiscard]] slicer_core::Json GetLayerDescriptor(
        const slicer_core::Json& request) const
    {
        const auto result = m_facade->GetLayerDescriptor(
            ReadPackageDir(request),
            RequireInteger(request, "layerIndex"));
        if (!result.IsOk())
        {
            return MakeFailure(*result.Error());
        }
        return MakeSuccess({
            {"layerIndex", result.Value()->layer_index},
            {"zMm", result.Value()->z_mm},
            {"widthPx", result.Value()->width_px},
            {"heightPx", result.Value()->height_px},
            {"printPixels", MakeChannelCounts(result.Value()->print_pixels)},
            {"emptyPixels", MakeChannelCounts(result.Value()->empty_pixels)},
            {"storageMode", result.Value()->storage_mode},
            {"tiffPath", result.Value()->tiff_path.generic_string()}});
    }

    [[nodiscard]] slicer_core::Json RenderLayerPreview(
        const slicer_core::Json& request) const
    {
        slicer_core::api::PreviewRequest previewRequest;
        previewRequest.package_dir = ReadPackageDir(request);
        previewRequest.layer_index = RequireInteger(request, "layerIndex");
        previewRequest.mode = RequireString(request, "mode");
        previewRequest.max_width_px = RequireInteger(request, "maxWidthPx");
        previewRequest.output_path = RequireString(request, "outputPath");
        for (const slicer_core::Json& channel : RequireArray(request, "channels"))
        {
            if (!channel.is_string())
            {
                throw CapabilityRequestError("channels must contain strings");
            }
            previewRequest.channels.push_back(channel.as_string());
        }
        NeverCancelToken cancelToken;
        const auto result = m_facade->RenderLayerPreview(
            previewRequest,
            cancelToken);
        if (!result.IsOk())
        {
            return MakeFailure(*result.Error());
        }
        return MakeSuccess({
            {"outputPath", result.Value()->output_path.generic_string()},
            {"widthPx", result.Value()->width_px},
            {"heightPx", result.Value()->height_px},
            {"cacheKey", result.Value()->cache_key}});
    }

    [[nodiscard]] slicer_core::Json ReadReport(
        const slicer_core::Json& request) const
    {
        const auto result = m_facade->ReadReport(
            ReadPackageDir(request),
            RequireString(request, "reportName"));
        if (!result.IsOk())
        {
            return MakeFailure(*result.Error());
        }
        return MakeSuccess({
            {"reportName", result.Value()->report_name},
            {"reportSchema", result.Value()->report_schema},
            {"data", ParseStructuredObject(result.Value()->data)},
            {"sourcePath", result.Value()->source_path.generic_string()}});
    }

    std::unique_ptr<slicer_core::api::PackageQueryFacade> m_facade;
};

PackageCapabilityAdapter::PackageCapabilityAdapter()
    : m_implementation(std::make_unique<Implementation>())
{
}

PackageCapabilityAdapter::~PackageCapabilityAdapter() = default;

slicer_core::Json PackageCapabilityAdapter::Execute(
    const std::string& capability,
    const slicer_core::Json& request) const
{
    return m_implementation->Execute(capability, request);
}

}  // namespace slicesoft::module
