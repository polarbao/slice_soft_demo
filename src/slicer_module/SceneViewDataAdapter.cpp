#include "slicer_module/SceneViewDataAdapter.h"

#include "slicer_core/api/viewdata/MeshAttributeQuantizer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace slicesoft::module
{

class SceneViewDataAdapter::Implementation final
{
public:
    [[nodiscard]] slicer_core::Json Serialize(
        const slicer_core::api::SceneViewData& data)
    {
        slicer_core::Json::Array appearances;
        for (const auto& appearance : data.appearances)
        {
            appearances.emplace_back(SerializeAppearance(appearance));
        }
        slicer_core::Json::Array meshes;
        std::map<std::string, slicer_core::Json> meshesByIdentity;
        for (const auto& mesh : data.meshes)
        {
            slicer_core::Json serialized = SerializeMesh(mesh);
            meshesByIdentity.emplace(mesh.mesh_identity, serialized);
            meshes.emplace_back(std::move(serialized));
        }
        slicer_core::Json::Array instances;
        for (const auto& instance : data.instances)
        {
            instances.emplace_back(SerializeInstance(
                instance,
                meshesByIdentity));
        }
        return MakeSuccess({
            {"viewdataIdentity", data.viewdata_identity},
            {"sceneRevision", data.scene_revision},
            {"viewMode", data.view_mode == slicer_core::api::ViewMode::Top
                ? "top" : "three_d"},
            {"units", "mm"},
            {"coordinateSystem", "right_handed_z_up"},
            {"byteOrder", "little_endian"},
            {"instances", slicer_core::Json{std::move(instances)}},
            {"appearances", slicer_core::Json{std::move(appearances)}},
            {"meshes", slicer_core::Json{std::move(meshes)}},
            {"truncated", data.truncated},
            {"truncationReason", data.truncation_reason.empty()
                ? slicer_core::Json{nullptr}
                : slicer_core::Json{data.truncation_reason}}});
    }

    [[nodiscard]] CapabilityOutput ReadBlob(const slicer_core::Json& request)
    {
        const std::string blobId = RequireString(request, "blobId");
        const int chunkIndex = RequireInteger(request, "chunkIndex");
        if (chunkIndex < 0)
        {
            throw CapabilityRequestError("chunkIndex must not be negative");
        }
        std::scoped_lock lock{m_mutex};
        const auto entry = m_blobs.find(blobId);
        if (entry == m_blobs.end())
        {
            return MakeCapabilityOutput(MakeFailure(
                "PM-SLICER-VIEWDATA-STALE",
                "view data blob is stale",
                blobId));
        }
        const std::size_t offset = static_cast<std::size_t>(chunkIndex)
            * ChunkBytes;
        if (offset >= entry->second.size())
        {
            return MakeCapabilityOutput(MakeFailure(
                "PM-SLICER-PROFILE-0031",
                "chunkIndex is outside the blob",
                std::to_string(chunkIndex)));
        }
        const std::size_t length = std::min(
            ChunkBytes,
            entry->second.size() - offset);
        CapabilityOutput output;
        output.succeeded = true;
        output.binary = true;
        output.bytes.assign(
            reinterpret_cast<const char*>(entry->second.data() + offset),
            length);
        return output;
    }

private:
    static constexpr std::size_t ChunkBytes{4U * 1024U * 1024U};
    static constexpr std::size_t MaxBlobBytes{256U * 1024U * 1024U};
    static constexpr std::size_t MaxBlobCount{64U};

    template <typename T>
    static void AppendBytes(
        std::vector<std::uint8_t>* bytes,
        const std::vector<T>& values)
    {
        const std::size_t oldSize = bytes->size();
        const std::size_t byteCount = values.size() * sizeof(T);
        bytes->resize(oldSize + byteCount);
        if (byteCount > 0U)
        {
            std::memcpy(bytes->data() + oldSize, values.data(), byteCount);
        }
    }

    [[nodiscard]] static const char* LodName(
        const slicer_core::api::ViewLod lod)
    {
        switch (lod)
        {
        case slicer_core::api::ViewLod::Lod0:
            return "lod0";
        case slicer_core::api::ViewLod::Lod1:
            return "lod1";
        case slicer_core::api::ViewLod::Lod2:
            return "lod2";
        default:
            return "lod0";
        }
    }

    [[nodiscard]] slicer_core::Json MakeBlobDescriptor(
        const std::string& identity,
        std::vector<std::uint8_t> bytes)
    {
        const std::size_t totalBytes = bytes.size();
        const std::string blobId = StoreBlob(identity, std::move(bytes));
        const std::size_t chunkCount = totalBytes == 0U
            ? 0U
            : (totalBytes + ChunkBytes - 1U) / ChunkBytes;
        return slicer_core::Json::object({
            {"blobId", blobId},
            {"totalBytes", static_cast<std::uint64_t>(totalBytes)},
            {"chunkBytes", static_cast<std::uint64_t>(ChunkBytes)},
            {"chunkCount", static_cast<std::uint64_t>(chunkCount)}});
    }

    [[nodiscard]] std::string StoreBlob(
        const std::string& identity,
        std::vector<std::uint8_t> bytes)
    {
        std::scoped_lock lock{m_mutex};
        while (!m_blobOrder.empty()
            && (m_blobs.size() >= MaxBlobCount
                || m_liveBytes + bytes.size() > MaxBlobBytes))
        {
            const std::string oldest = m_blobOrder.front();
            m_blobOrder.pop_front();
            const auto entry = m_blobs.find(oldest);
            if (entry != m_blobs.end())
            {
                m_liveBytes -= entry->second.size();
                m_blobs.erase(entry);
            }
        }
        if (bytes.size() > MaxBlobBytes)
        {
            throw CapabilityRequestError("view data blob exceeds the module budget");
        }
        const std::string blobId = "blob:" + identity + ":"
            + std::to_string(m_nextBlobId++);
        m_liveBytes += bytes.size();
        m_blobOrder.push_back(blobId);
        m_blobs.emplace(blobId, std::move(bytes));
        return blobId;
    }

    [[nodiscard]] slicer_core::Json SerializeAppearance(
        const slicer_core::api::ViewAppearance& appearance)
    {
        slicer_core::Json::Array materials;
        for (const auto& material : appearance.materials)
        {
            const bool blended = material.base_color.at(3U) < 1.0F;
            materials.emplace_back(slicer_core::Json::object({
                {"materialId", material.material_id},
                {"baseColorFactor", MakeNumberArray(material.base_color)},
                {"baseColorTextureId", material.texture_id.empty()
                    ? slicer_core::Json{nullptr}
                    : slicer_core::Json{material.texture_id}},
                {"alphaMode", blended ? "blend" : "opaque"},
                {"alphaCutoff", 0.5},
                {"doubleSided", material.double_sided},
                {"uvSet", material.uv_set},
                {"uvTransform", MakeNumberArray(material.uv_transform)}}));
        }
        slicer_core::Json::Array textures;
        for (const auto& texture : appearance.textures)
        {
            slicer_core::Json::Object fields{
                {"textureId", texture.texture_id},
                {"textureIdentity", texture.texture_identity},
                {"widthPx", texture.width_px},
                {"heightPx", texture.height_px},
                {"pixelFormat", "rgba8_unorm"},
                {"colorSpace", "srgb"},
                {"alphaMode", "straight"},
                {"rowOrigin", "top_left"}};
            const auto blob = MakeBlobDescriptor(
                texture.texture_identity,
                texture.rgba8).as_object();
            fields.insert(blob.begin(), blob.end());
            textures.emplace_back(slicer_core::Json{std::move(fields)});
        }
        return slicer_core::Json::object({
            {"appearanceIdentity", appearance.appearance_identity},
            {"uvConvention", "u_right_v_up"},
            {"materials", slicer_core::Json{std::move(materials)}},
            {"textures", slicer_core::Json{std::move(textures)}}});
    }

    [[nodiscard]] slicer_core::Json SerializeInstance(
        const slicer_core::api::ViewInstance& instance,
        const std::map<std::string, slicer_core::Json>& meshesByIdentity)
    {
        slicer_core::Json::Array loops;
        for (const auto& outline : instance.outlines)
        {
            slicer_core::Json::Array points;
            for (const auto& point : outline.points_mm)
            {
                points.emplace_back(MakeNumberArray(point));
            }
            loops.emplace_back(slicer_core::Json{std::move(points)});
        }
        slicer_core::Json::Object fields{
            {"instanceId", instance.instance_id},
            {"modelId", std::to_string(instance.model_id)},
            {"bboxLocalMm", MakeBounds(instance.local_bounds_mm)},
            {"worldMatrix", MakeMatrix(instance.world_matrix)},
            {"textureStatus", instance.texture_status
                    == slicer_core::api::TextureStatus::Available
                ? "available" : "not_provided"},
            {"meshIdentity", instance.mesh_identity},
            {"appearanceIdentity", instance.appearance_identity},
            {"previewIdentity", instance.preview_identity}};
        fields.emplace(
            "outline",
            slicer_core::Json::object({
                {"loops", slicer_core::Json{std::move(loops)}}}));
        if (instance.surface_preview)
        {
            fields.emplace(
                "surfacePreview",
                SerializePreview(*instance.surface_preview));
        }
        const auto mesh = meshesByIdentity.find(instance.mesh_identity);
        if (mesh != meshesByIdentity.end())
        {
            fields.emplace("mesh", mesh->second);
        }
        else if (instance.mesh)
        {
            fields.emplace("mesh", SerializeMesh(*instance.mesh));
        }
        return slicer_core::Json{std::move(fields)};
    }

    [[nodiscard]] slicer_core::Json SerializePreview(
        const slicer_core::api::SurfacePreview& preview)
    {
        slicer_core::Json::Object fields{
            {"previewIdentity", preview.preview_identity},
            {"appearanceIdentity", preview.appearance_identity},
            {"widthPx", preview.width_px},
            {"heightPx", preview.height_px},
            {"localBoundsMm", MakeBounds(preview.local_bounds_mm)},
            {"pixelFormat", "rgba8_unorm"},
            {"colorSpace", "srgb"},
            {"alphaMode", "straight"},
            {"rowOrigin", "top_left"}};
        const auto blob = MakeBlobDescriptor(
            preview.preview_identity,
            preview.rgba8).as_object();
        fields.insert(blob.begin(), blob.end());
        return slicer_core::Json{std::move(fields)};
    }

    [[nodiscard]] slicer_core::Json SerializeMesh(
        const slicer_core::api::ViewMesh& mesh)
    {
        std::vector<std::uint8_t> bytes;
        const bool useHalf = mesh.attribute_format
            == slicer_core::api::MeshAttributeFormat::Float16;
        const std::size_t positionOffset = bytes.size();
        if (useHalf)
        {
            AppendBytes(
                &bytes,
                slicer_core::api::viewdata_detail::
                    QuantizeMeshAttributesToHalf(mesh.positions));
        }
        else
        {
            AppendBytes(&bytes, mesh.positions);
        }
        const std::size_t normalOffset = bytes.size();
        if (useHalf)
        {
            AppendBytes(
                &bytes,
                slicer_core::api::viewdata_detail::
                    QuantizeMeshAttributesToHalf(mesh.normals));
        }
        else
        {
            AppendBytes(&bytes, mesh.normals);
        }
        const std::size_t textureOffset = bytes.size();
        if (useHalf)
        {
            AppendBytes(
                &bytes,
                slicer_core::api::viewdata_detail::
                    QuantizeMeshAttributesToHalf(mesh.texcoord0));
        }
        else
        {
            AppendBytes(&bytes, mesh.texcoord0);
        }
        const std::size_t indexOffset = bytes.size();
        AppendBytes(&bytes, mesh.indices);
        const std::string positionFormat = useHalf
            ? "float16x3" : "float32x3";
        const std::string textureFormat = useHalf
            ? "float16x2" : "float32x2";
        slicer_core::Json::Array submeshes;
        for (const auto& submesh : mesh.submeshes)
        {
            submeshes.emplace_back(slicer_core::Json::object({
                {"firstIndex", static_cast<std::uint64_t>(submesh.first_index)},
                {"indexCount", static_cast<std::uint64_t>(submesh.index_count)},
                {"materialId", submesh.material_id}}));
        }
        slicer_core::Json::Object fields{
            {"meshIdentity", mesh.mesh_identity},
            {"lod", LodName(mesh.lod)},
            {"vertexCount", static_cast<std::uint64_t>(mesh.positions.size() / 3U)},
            {"triangleCount", static_cast<std::uint64_t>(mesh.indices.size() / 3U)},
            {"meshTransform", mesh.mesh_transform
                    == slicer_core::api::MeshTransform::Local
                ? "local" : "world"},
            {"buffers", slicer_core::Json::object({
                {"position", MakeBuffer(positionFormat, positionOffset, normalOffset - positionOffset)},
                {"normal", MakeBuffer(positionFormat, normalOffset, textureOffset - normalOffset)},
                {"texcoord0", MakeBuffer(textureFormat, textureOffset, indexOffset - textureOffset)},
                {"index", MakeBuffer("uint32", indexOffset, bytes.size() - indexOffset)}})},
            {"submeshes", slicer_core::Json{std::move(submeshes)}}};
        const auto blob = MakeBlobDescriptor(
            mesh.mesh_identity,
            std::move(bytes)).as_object();
        fields.insert(blob.begin(), blob.end());
        return slicer_core::Json{std::move(fields)};
    }

    [[nodiscard]] static slicer_core::Json MakeBuffer(
        const std::string& format,
        const std::size_t offset,
        const std::size_t length)
    {
        return slicer_core::Json::object({
            {"format", format},
            {"byteOffset", static_cast<std::uint64_t>(offset)},
            {"byteLength", static_cast<std::uint64_t>(length)}});
    }

    std::mutex m_mutex;
    std::uint64_t m_nextBlobId{1U};
    std::size_t m_liveBytes{0U};
    std::deque<std::string> m_blobOrder;
    std::map<std::string, std::vector<std::uint8_t>> m_blobs;
};

SceneViewDataAdapter::SceneViewDataAdapter()
    : m_implementation(std::make_unique<Implementation>())
{
}

SceneViewDataAdapter::~SceneViewDataAdapter() = default;

slicer_core::Json SceneViewDataAdapter::Serialize(
    const slicer_core::api::SceneViewData& data)
{
    return m_implementation->Serialize(data);
}

CapabilityOutput SceneViewDataAdapter::ReadBlob(
    const slicer_core::Json& request)
{
    return m_implementation->ReadBlob(request);
}

}  // namespace slicesoft::module
