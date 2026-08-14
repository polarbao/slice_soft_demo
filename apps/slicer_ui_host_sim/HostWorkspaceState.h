#pragma once

#include "HostSliceSettings.h"

#include <QString>

class QMainWindow;
class QSettings;
class QSplitter;
class QTabWidget;

/** @brief 在创建新的运行时场景之前恢复宿主偏好。 */
struct hostworkspacepreferences
{
    hostslicesettings slicesettings;
};

/** @brief 保留版本化的宿主偏好和安全的工作区布局。 */
class HostWorkspaceState final
{
public:
    /**
     * @brief 返回当前宿主工作区架构版本。
     * @return 稳定的整数模式版本。
     */
    static int SchemaVersion();

    /**
     * @brief 返回生产 QSettings 组织名称。
     * @return 稳定的组织标识符。
     */
    static QString OrganizationName();

    /**
     * @brief 返回生产 QSettings 应用程序名称。
     * @return 稳定的应用程序标识符。
     */
    static QString ApplicationName();

    /**
     * @brief 报告此进程是否可以访问真实的用户设置。
     * @return 对于所有自测试过程都是错误的。
     */
    static bool PersistenceEnabled();

    /**
     * @brief 保存经过验证的宿主偏好和工作区几何形状。
     * @param settings 目的地设置存储。
     * @param window 参考宿主主窗口。
     * @param workspaceSplitter 主工作区分配器。
     * @param workspaceTabs 顶级工作区选项卡。
     * @param inspectorTabs 托管业务检查器选项卡。
     * @param sliceSettings 当前由宿主持有的切片设置。
     * @return 该函数不返回值。
     */
    static void Save(
        QSettings& settings,
        QMainWindow* window,
        QSplitter* workspaceSplitter,
        QTabWidget* workspaceTabs,
        QTabWidget* inspectorTabs,
        const hostslicesettings& sliceSettings);

    /**
     * @brief 恢复有效的工作区或应用安全默认值。
     * @param settings 源设置存储。
     * @param window 参考宿主主窗口。
     * @param workspaceSplitter 主工作区分配器。
     * @param workspaceTabs 顶级工作区选项卡。
     * @param inspectorTabs 托管业务检查器选项卡。
     * @param preferences 接收经过验证的由宿主持有的首选项。
     * @return 仅当完整恢复已保存状态时返回 true。
     */
    static bool Restore(
        QSettings& settings,
        QMainWindow* window,
        QSplitter* workspaceSplitter,
        QTabWidget* workspaceTabs,
        QTabWidget* inspectorTabs,
        hostworkspacepreferences* preferences);

    /**
     * @brief 应用安全参考宿主工作区布局。
     * @param window 参考宿主主窗口。
     * @param workspaceSplitter 主工作区分配器。
     * @param workspaceTabs 顶级工作区选项卡。
     * @param inspectorTabs 托管业务检查器选项卡。
     * @return 该函数不返回值。
     */
    static void Reset(
        QMainWindow* window,
        QSplitter* workspaceSplitter,
        QTabWidget* workspaceTabs,
        QTabWidget* inspectorTabs);

private:
    static bool IsOnAvailableScreen(const QMainWindow* window);
};
