#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <vector>

namespace slicer::render
{

/**
 * @brief 将一个 float32 或 float16 网格属性解码为宿主本地浮点值。
 * @param blob 完整的小端网格 blob。
 * @param buffers ViewData 缓冲区描述符。
 * @param name 描述符中的属性名称。
 * @param componentCount 每个顶点的分量数。
 * @param vertexCount 预期顶点数。
 * @param values 目标宿主本地浮点值。
 * @param error 可选的本地化失败文本。
 * @return 描述符与解码值均有效时返回 true。
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
