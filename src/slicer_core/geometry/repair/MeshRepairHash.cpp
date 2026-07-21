#include "slicer_core/geometry/repair/MeshRepairHash.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <tuple>

namespace slicer_core
{
namespace
{

constexpr std::array<std::uint32_t, 64> k_roundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

class Sha256Hasher
{
public:
    void Update(const std::uint8_t* data, const std::size_t size)
    {
        for (std::size_t index{0U}; index < size; ++index)
        {
            m_buffer.at(m_bufferSize++) = data[index];
            ++m_totalBytes;
            if (m_bufferSize == m_buffer.size())
            {
                ProcessBlock(m_buffer.data());
                m_bufferSize = 0U;
            }
        }
    }

    std::array<std::uint8_t, 32> Finalize()
    {
        const std::uint64_t totalBits = m_totalBytes * 8U;
        m_buffer.at(m_bufferSize++) = 0x80U;
        if (m_bufferSize > 56U)
        {
            while (m_bufferSize < m_buffer.size())
            {
                m_buffer.at(m_bufferSize++) = 0U;
            }
            ProcessBlock(m_buffer.data());
            m_bufferSize = 0U;
        }
        while (m_bufferSize < 56U)
        {
            m_buffer.at(m_bufferSize++) = 0U;
        }
        for (int shift{56}; shift >= 0; shift -= 8)
        {
            m_buffer.at(m_bufferSize++) = static_cast<std::uint8_t>(totalBits >> shift);
        }
        ProcessBlock(m_buffer.data());

        std::array<std::uint8_t, 32> digest{};
        for (std::size_t index{0U}; index < m_state.size(); ++index)
        {
            digest.at(index * 4U) = static_cast<std::uint8_t>(m_state.at(index) >> 24U);
            digest.at(index * 4U + 1U) = static_cast<std::uint8_t>(m_state.at(index) >> 16U);
            digest.at(index * 4U + 2U) = static_cast<std::uint8_t>(m_state.at(index) >> 8U);
            digest.at(index * 4U + 3U) = static_cast<std::uint8_t>(m_state.at(index));
        }
        return digest;
    }

private:
    static std::uint32_t RotateRight(const std::uint32_t value, const int bits)
    {
        return (value >> bits) | (value << (32 - bits));
    }

    void ProcessBlock(const std::uint8_t* block)
    {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index{0U}; index < 16U; ++index)
        {
            words.at(index) =
                (static_cast<std::uint32_t>(block[index * 4U]) << 24U)
                | (static_cast<std::uint32_t>(block[index * 4U + 1U]) << 16U)
                | (static_cast<std::uint32_t>(block[index * 4U + 2U]) << 8U)
                | static_cast<std::uint32_t>(block[index * 4U + 3U]);
        }
        for (std::size_t index{16U}; index < words.size(); ++index)
        {
            const std::uint32_t s0 = RotateRight(words.at(index - 15U), 7)
                ^ RotateRight(words.at(index - 15U), 18)
                ^ (words.at(index - 15U) >> 3U);
            const std::uint32_t s1 = RotateRight(words.at(index - 2U), 17)
                ^ RotateRight(words.at(index - 2U), 19)
                ^ (words.at(index - 2U) >> 10U);
            words.at(index) = words.at(index - 16U) + s0
                + words.at(index - 7U) + s1;
        }

        std::uint32_t a = m_state.at(0U);
        std::uint32_t b = m_state.at(1U);
        std::uint32_t c = m_state.at(2U);
        std::uint32_t d = m_state.at(3U);
        std::uint32_t e = m_state.at(4U);
        std::uint32_t f = m_state.at(5U);
        std::uint32_t g = m_state.at(6U);
        std::uint32_t h = m_state.at(7U);

        for (std::size_t index{0U}; index < words.size(); ++index)
        {
            const std::uint32_t sum1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
            const std::uint32_t choose = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + sum1 + choose + k_roundConstants.at(index) + words.at(index);
            const std::uint32_t sum0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        m_state.at(0U) += a;
        m_state.at(1U) += b;
        m_state.at(2U) += c;
        m_state.at(3U) += d;
        m_state.at(4U) += e;
        m_state.at(5U) += f;
        m_state.at(6U) += g;
        m_state.at(7U) += h;
    }

    std::array<std::uint32_t, 8> m_state{
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U};
    std::array<std::uint8_t, 64> m_buffer{};
    std::size_t m_bufferSize{0U};
    std::uint64_t m_totalBytes{0U};
};

class CanonicalEncoder
{
public:
    void AppendBytes(const std::uint8_t* data, const std::size_t size)
    {
        m_hasher.Update(data, size);
    }

    void AppendString(const std::string_view value)
    {
        AppendUnsigned(static_cast<std::uint64_t>(value.size()));
        AppendBytes(
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size());
    }

    void AppendUtf8String(const std::u8string& value)
    {
        AppendUnsigned(static_cast<std::uint64_t>(value.size()));
        AppendBytes(
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size());
    }

    void AppendUnsigned(const std::uint64_t value)
    {
        std::array<std::uint8_t, 8> bytes{};
        for (std::size_t index{0U}; index < bytes.size(); ++index)
        {
            bytes.at(index) = static_cast<std::uint8_t>(value >> (index * 8U));
        }
        AppendBytes(bytes.data(), bytes.size());
    }

    void AppendSigned(const std::int64_t value)
    {
        AppendUnsigned(std::bit_cast<std::uint64_t>(value));
    }

    void AppendBoolean(const bool value)
    {
        const std::uint8_t byte = value ? 1U : 0U;
        AppendBytes(&byte, 1U);
    }

    void AppendDouble(const double value)
    {
        if (!std::isfinite(value))
        {
            throw MeshRepairError(
                MeshRepairErrorCode::InputInvalid,
                "mesh repair canonical hash requires finite floating-point values");
        }
        const double normalized = value == 0.0 ? 0.0 : value;
        AppendUnsigned(std::bit_cast<std::uint64_t>(normalized));
    }

    std::string FinalizeHex()
    {
        const std::array<std::uint8_t, 32> digest = m_hasher.Finalize();
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (const std::uint8_t byte : digest)
        {
            stream << std::setw(2) << static_cast<unsigned int>(byte);
        }
        return stream.str();
    }

private:
    Sha256Hasher m_hasher;
};

void AppendIds(CanonicalEncoder& encoder, const std::vector<std::uint64_t>& ids)
{
    encoder.AppendUnsigned(static_cast<std::uint64_t>(ids.size()));
    for (const std::uint64_t id : ids)
    {
        encoder.AppendUnsigned(id);
    }
}

}  // namespace

std::string ComputeMeshRepairSha256(const std::string_view canonicalPayload)
{
    CanonicalEncoder encoder;
    encoder.AppendBytes(
        reinterpret_cast<const std::uint8_t*>(canonicalPayload.data()),
        canonicalPayload.size());
    return encoder.FinalizeHex();
}

std::string ComputeMeshRepairGeometryHash(const TriangleMeshData& mesh)
{
    CanonicalEncoder encoder;
    encoder.AppendString("mesh_repair_canonical.1/geometry");
    encoder.AppendUnsigned(static_cast<std::uint64_t>(mesh.vertices.size()));
    for (const Vec3& vertex : mesh.vertices)
    {
        encoder.AppendDouble(vertex.x);
        encoder.AppendDouble(vertex.y);
        encoder.AppendDouble(vertex.z);
    }
    encoder.AppendUnsigned(static_cast<std::uint64_t>(mesh.triangles.size()));
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        for (const int index : triangle)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= mesh.vertices.size())
            {
                throw MeshRepairError(
                    MeshRepairErrorCode::InputInvalid,
                    "mesh repair geometry hash received an invalid triangle index");
            }
            encoder.AppendSigned(index);
        }
    }
    return encoder.FinalizeHex();
}

std::string ComputeMeshRepairAttributeHash(const AdaptedTriangleMesh& mesh)
{
    if (mesh.triangle_attributes.size() != mesh.mesh.triangles.size())
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "mesh repair attribute hash requires one attribute record per triangle");
    }

    CanonicalEncoder encoder;
    encoder.AppendString("mesh_repair_canonical.1/attributes");
    encoder.AppendUnsigned(static_cast<std::uint64_t>(mesh.triangle_attributes.size()));
    for (const SurfaceTriangleAttributes& attributes : mesh.triangle_attributes)
    {
        encoder.AppendUnsigned(static_cast<std::uint64_t>(attributes.source_triangle_index));
        encoder.AppendBoolean(attributes.has_uv);
        for (const TexCoord& uv : attributes.uv)
        {
            encoder.AppendDouble(uv.u);
            encoder.AppendDouble(uv.v);
        }
        encoder.AppendString(attributes.material_name);
    }

    std::vector<MaterialInfo> materials = mesh.material_infos;
    std::sort(
        materials.begin(),
        materials.end(),
        [](const MaterialInfo& left, const MaterialInfo& right)
        {
            return std::tuple{
                       left.name,
                       left.diffuse_rgb,
                       left.has_diffuse,
                       left.diffuse_texture_path.generic_u8string(),
                       left.has_texture,
                       left.texture_exists,
                       left.texture_source}
                < std::tuple{
                       right.name,
                       right.diffuse_rgb,
                       right.has_diffuse,
                       right.diffuse_texture_path.generic_u8string(),
                       right.has_texture,
                       right.texture_exists,
                       right.texture_source};
        });
    encoder.AppendUnsigned(static_cast<std::uint64_t>(materials.size()));
    for (const MaterialInfo& material : materials)
    {
        encoder.AppendString(material.name);
        for (const std::uint8_t channel : material.diffuse_rgb)
        {
            encoder.AppendUnsigned(channel);
        }
        encoder.AppendBoolean(material.has_diffuse);
        encoder.AppendUtf8String(material.diffuse_texture_path.generic_u8string());
        encoder.AppendBoolean(material.has_texture);
        encoder.AppendBoolean(material.texture_exists);
        encoder.AppendString(material.texture_source);
    }
    return encoder.FinalizeHex();
}

std::string ComputeMeshRepairOptionsHash(const MeshRepairOptions& options)
{
    CanonicalEncoder encoder;
    encoder.AppendString("mesh_repair_canonical.1/options");
    encoder.AppendBoolean(options.enabled);
    encoder.AppendString(options.mode);
    encoder.AppendBoolean(options.allowVertexWeld);
    encoder.AppendDouble(options.weldToleranceMm);
    encoder.AppendBoolean(options.allowWindingRepair);
    encoder.AppendBoolean(options.allowBoundaryFill);
    encoder.AppendUnsigned(options.maxBoundaryLoopEdges);
    encoder.AppendDouble(options.maxBoundaryLoopDiameterMm);
    encoder.AppendDouble(options.maxBoundaryLoopPerimeterMm);
    encoder.AppendDouble(options.maxBoundaryPlanarityErrorMm);
    encoder.AppendDouble(options.maxHoleAreaMm2);
    encoder.AppendDouble(options.maxAffectedFaceRatio);
    encoder.AppendBoolean(options.allowNewFaces);
    encoder.AppendString(options.newFaceAttributePolicy);
    encoder.AppendBoolean(options.validatePostRepairEvidence);
    return encoder.FinalizeHex();
}

std::string ComputeMeshRepairOperationsHash(
    const std::vector<MeshRepairOperation>& operations)
{
    CanonicalEncoder encoder;
    encoder.AppendString("mesh_repair_canonical.1/operations");
    encoder.AppendUnsigned(static_cast<std::uint64_t>(operations.size()));
    for (const MeshRepairOperation& operation : operations)
    {
        encoder.AppendUnsigned(operation.operationId);
        encoder.AppendString(MeshRepairOperationTypeName(operation.type));
        encoder.AppendString(operation.reasonCode);
        AppendIds(encoder, operation.inputElementIds);
        AppendIds(encoder, operation.outputElementIds);
        encoder.AppendString(operation.parameters.dump(0));
        encoder.AppendString(MeshRepairAttributeDecisionName(operation.attributeDecision));
        encoder.AppendUnsigned(operation.affectedVertices);
        encoder.AppendUnsigned(operation.affectedEdges);
        encoder.AppendUnsigned(operation.affectedFaces);
    }
    return encoder.FinalizeHex();
}

MeshRepairHashes ComputeMeshRepairPreHashes(
    const AdaptedTriangleMesh& mesh,
    const MeshRepairOptions& options,
    const std::vector<MeshRepairOperation>& operations)
{
    MeshRepairHashes hashes;
    hashes.preRepairGeometryHash = ComputeMeshRepairGeometryHash(mesh.mesh);
    hashes.preRepairAttributeHash = ComputeMeshRepairAttributeHash(mesh);
    hashes.repairOperationHash = ComputeMeshRepairOperationsHash(operations);
    hashes.optionsHash = ComputeMeshRepairOptionsHash(options);
    return hashes;
}

}  // namespace slicer_core
