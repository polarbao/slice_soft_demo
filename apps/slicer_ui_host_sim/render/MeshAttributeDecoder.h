#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <vector>

namespace slicer::render
{

/**
 * @brief Decodes one float32 or float16 mesh attribute into host-local floats.
 * @param blob Complete little-endian mesh blob.
 * @param buffers ViewData buffers descriptor.
 * @param name Attribute name in the descriptor.
 * @param componentCount Components per vertex.
 * @param vertexCount Expected vertex count.
 * @param values Destination host-local float values.
 * @param error Optional localized failure text.
 * @return True when the descriptor and decoded values are valid.
 */
[[nodiscard]] bool DecodeMeshAttribute(
    const QByteArray& blob,
    const QJsonObject& buffers,
    const QString& name,
    int componentCount,
    int vertexCount,
    std::vector<float>* values,
    QString* error);

}  // namespace slicer::render
