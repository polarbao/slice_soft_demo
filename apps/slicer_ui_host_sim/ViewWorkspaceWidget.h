#pragma once

#include "camera/CameraController.h"
#include "camera/ViewModeSwitch.h"

#include <QImage>
#include <QSize>
#include <QStringList>
#include <QWidget>

class QLabel;
class QComboBox;
class QStackedWidget;
class QToolButton;
class ThreeDCanvasWidget;
class TopViewCanvasWidget;

/**
 * @brief 支持俯视/三维即时切换的中央宿主工作区。
 */
class ViewWorkspaceWidget final : public QWidget
{
public:
    /** @brief 创建仅显示的双视图工作区。 */
    explicit ViewWorkspaceWidget(QWidget* parent = nullptr);

    /** @brief 选择视图而不更改场景或作业状态。 */
    void SetMode(HostViewMode mode);

    /** @brief 返回当前可见的演示模式。 */
    [[nodiscard]] HostViewMode Mode() const;

    /** @brief 显示明确的失败即拒绝视图诊断。 */
    void ShowViewError(const QString& message);

    /**
     * @brief 无需模块调用即可更新宿主本地模型选择。
     * @param instanceIds 模型列表中选中的稳定实例标识。
     */
    void SetSelectedInstances(const QStringList& instanceIds);

    /**
     * @brief 用渲染图像替换可见的俯视帧。
     * @param image 完成仅显示的顶部 ViewData 渲染。
     */
    void SetTopImage(const QImage& image);

    /** @brief 更新瞬态俯视像素而不重置导航。 */
    void UpdateTopImage(const QImage& image);

    /** @brief 在明确刷新失败后清除俯视图像。 */
    void ClearTopImage();

    /** @brief 返回俯视渲染的稳定目标尺寸。 */
    [[nodiscard]] QSize TopRenderSize() const;

    /** @brief 返回保留的俯视画布，供宿主本地导航使用。 */
    [[nodiscard]] TopViewCanvasWidget* TopCanvas() const;

    /**
     * @brief 替换可见的三维框架。
     * @param image 完整的宿主本地渲染输出。
     */
    void SetThreeDImage(const QImage& image);

    /** @brief 关闭失败后清除三维图像。 */
    void ClearThreeDImage();

    /**
     * @brief 使三维相机适应权威的世界范围。
     * @param bounds 场景边界以毫米为单位。
     */
    void SetThreeDSceneBounds(const CameraBounds& bounds);

    /** @brief 返回稳定的三维光栅目标大小。 */
    [[nodiscard]] QSize ThreeDRenderSize() const;

    /** @brief 返回保留的三维相机画布。 */
    [[nodiscard]] ThreeDCanvasWidget* ThreeDCanvas() const;

    /** @brief 将宿主显示设置应用到本地 3D 相机。 */
    void SetThreeDProjection(slicer::render::Projection projection);

private:
    void ApplyMode();

    ViewModeSwitch m_switch;
    QToolButton* m_topButton{nullptr};
    QToolButton* m_threeDButton{nullptr};
    QWidget* m_threeDControls{nullptr};
    QComboBox* m_presetCombo{nullptr};
    QComboBox* m_projectionCombo{nullptr};
    QLabel* m_selectionLabel{nullptr};
    QStackedWidget* m_canvasStack{nullptr};
    TopViewCanvasWidget* m_topCanvas{nullptr};
    ThreeDCanvasWidget* m_threeDCanvas{nullptr};
    QLabel* m_errorLabel{nullptr};
};
