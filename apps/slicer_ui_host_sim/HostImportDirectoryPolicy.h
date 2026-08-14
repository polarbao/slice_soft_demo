#pragma once

#include <QString>

/** @brief 解析参考宿主的初始模型目录。 */
class HostImportDirectoryPolicy final
{
public:
    /**
     * @brief 按稳定优先级选择已存在的模型导入目录。
     * @param applicationDirectory 当前可执行文件所在目录。
     * @param workingDirectory 当前进程工作目录。
     * @param previousDirectory 本会话最近使用的目录。
     * @return 适用于模型文件对话框且已存在的绝对目录。
     */
    [[nodiscard]] static QString Resolve(
        const QString& applicationDirectory,
        const QString& workingDirectory,
        const QString& previousDirectory = {});
};
