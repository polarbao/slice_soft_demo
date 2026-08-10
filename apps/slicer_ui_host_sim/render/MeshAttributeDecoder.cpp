#include "MeshAttributeDecoder.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace slicer::render
{
namespace
{

float DecodeHalf(const std::uint16_t value)
{
    const std::uint32_t sign = static_cast<std::uint32_t>(value & 0x8000U)
        << 16U;
    std::uint32_t exponent = (value >> 10U) & 0x1FU;
    std::uint32_t mantissa = value & 0x03FFU;
    std::uint32_t bits{sign};
    if (exponent == 0U && mantissa != 0U)
    {
        int normalizedExponent{-14};
        while ((mantissa & 0x0400U) == 0U)
        {
            mantissa <<= 1U;
            --normalizedExponent;
        }
        mantissa &= 0x03FFU;
        bits |= static_cast<std::uint32_t>(normalizedExponent + 127) << 23U;
        bits |= mantissa << 13U;
    }
    else if (exponent == 0x1FU)
    {
        bits |= 0x7F800000U | (mantissa << 13U);
    }
    else if (exponent != 0U)
    {
        exponent += 127U - 15U;
        bits |= exponent << 23U;
        bits |= mantissa << 13U;
    }
    return std::bit_cast<float>(bits);
}

bool SetError(QString* error, const QString& message)
{
    if (error != nullptr)
    {
        *error = message;
    }
    return false;
}

}  // namespace

bool DecodeMeshAttribute(
    const QByteArray& blob,
    const QJsonObject& buffers,
    const QString& name,
    const int componentCount,
    const int vertexCount,
    std::vector<float>* values,
    QString* error)
{
    if (values == nullptr || componentCount <= 0 || vertexCount <= 0)
    {
        return SetError(error, QStringLiteral("three_d 网格属性参数无效。"));
    }
    const QJsonObject descriptor = buffers.value(name).toObject();
    const QString format = descriptor.value(QStringLiteral("format")).toString();
    const qint64 offset = static_cast<qint64>(descriptor.value(
        QStringLiteral("byteOffset")).toDouble(-1.0));
    const qint64 length = static_cast<qint64>(descriptor.value(
        QStringLiteral("byteLength")).toDouble(-1.0));
    const bool useHalf = format == QStringLiteral("float16x%1").arg(
        componentCount);
    const bool useFloat = format == QStringLiteral("float32x%1").arg(
        componentCount);
    const qint64 scalarBytes = useHalf
        ? static_cast<qint64>(sizeof(std::uint16_t))
        : static_cast<qint64>(sizeof(float));
    const qint64 valueCount = static_cast<qint64>(vertexCount)
        * componentCount;
    if ((!useHalf && !useFloat) || offset < 0 || length <= 0
        || offset > blob.size() || length > blob.size() - offset
        || length != valueCount * scalarBytes)
    {
        return SetError(error, QStringLiteral("three_d 网格属性 buffer 合同无效。"));
    }

    values->resize(static_cast<std::size_t>(valueCount));
    const auto* source = reinterpret_cast<const std::uint8_t*>(
        blob.constData() + offset);
    if (useFloat)
    {
        std::memcpy(
            values->data(),
            source,
            static_cast<std::size_t>(length));
    }
    else
    {
        for (qint64 index{0}; index < valueCount; ++index)
        {
            const std::size_t byteOffset = static_cast<std::size_t>(index) * 2U;
            const std::uint16_t half = static_cast<std::uint16_t>(
                source[byteOffset])
                | (static_cast<std::uint16_t>(source[byteOffset + 1U]) << 8U);
            values->at(static_cast<std::size_t>(index)) = DecodeHalf(half);
        }
    }
    if (std::any_of(
            values->begin(),
            values->end(),
            [](const float value)
            {
                return !std::isfinite(value);
            }))
    {
        values->clear();
        return SetError(error, QStringLiteral("three_d 网格属性包含非有限值。"));
    }
    return true;
}

}  // namespace slicer::render
