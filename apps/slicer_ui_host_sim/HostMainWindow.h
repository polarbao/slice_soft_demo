#pragma once

#include "HostModelListPanel.h"
#include "HostModelImportWorkflow.h"
#include "HostProfileCatalog.h"
#include "HostProfilePanel.h"
#include "HostPackageReviewController.h"
#include "HostPackageReviewPanel.h"
#include "HostRipJobController.h"
#include "HostRipSettingsPanel.h"
#include "HostSliceJobController.h"
#include "HostSliceJobPanel.h"
#include "HostSliceSettingsPanel.h"
#include "HostTransformLayoutPanel.h"
#include "ModuleClient.h"

#include <QMainWindow>
#include <QPointF>

#include <memory>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class QTableWidget;
class QTabWidget;
class CpuRasterBackend;
class MoveOptimizationPolicy;
class SceneRenderPolicy;
class SceneInteractionController;
class HostTextureWhitePreflightService;
struct hosttexturewhitepreflightresult;
struct ThreeDFrame;
struct TopViewFrame;
class TopViewRenderPolicy;
class ViewPresentationSettings;
class ViewWorkspaceWidget;

/**
 * @brief 仅使用公共模块 ABI 的最小 Qt 参考宿主外壳。
 */
class HostMainWindow final : public QMainWindow
{
public:
    /**
     * @brief 创建参考宿主并尝试加载模块。
     * @param modulePath slicer_module.dll 的运行时路径。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostMainWindow(
        const QString& modulePath,
        QWidget* parent = nullptr);

    /** @brief 释放宿主 UI 与会话设置资源。 */
    ~HostMainWindow() override;

private:
    void BuildInterface();
    void LoadModule(const QString& modulePath);
    void ConfigureProfiles();
    void SaveViewSettings();
    void RestoreWorkspaceState();
    bool SaveWorkspaceState();
    void OnImportModel();
    void OnRemoveModels(const QStringList& instanceIds);
    void OnModelSelectionChanged(const QStringList& instanceIds);
    void OnProfileChanged(const QString& profileId);
    bool ProfileSupportsSlice(const QString& profileId) const;
    void OnSliceSettingsChanged();
    void OnStartSlice();
    void OnCancelSlice();
    void OnRipSettingsChanged();
    void OnRunRip();
    void OnCancelRip();
    void OnRipStateChanged(const QString& state, const QString& message);
    void OnRipCompleted(
        bool success,
        bool cancelled,
        const QString& code,
        const QString& message,
        const QString& outputDirectory,
        qint64 elapsedMs);
    void OnOpenRipOutputRequested(const QString& outputDirectory);
    void RefreshRipRuntimeStatus();
    void RefreshRipRequestStatus();
    bool StartRipForPackage(
        const QString& packageDirectory,
        bool automatic);
    void OnSliceJobProgress(
        const QString& state,
        const QString& phase,
        int current,
        int total,
        int percent,
        qint64 elapsedMs);
    void OnSliceJobCompleted(
        bool success,
        bool cancelled,
        const QString& code,
        const QString& message,
        const QString& detail,
        const QString& packageDirectory,
        const QJsonObject& timing,
        qint64 elapsedMs,
        qint64 cancelLatencyMs);
    void LoadSliceResult(const QString& packageDirectory);
    void OnResultLayerRequested(
        int layerIndex,
        const QStringList& channels);
    void OnResultReportRequested(const QString& reportName);
    void OnOpenPackageDirectoryRequested(const QString& packageDirectory);
    bool ApplyPendingSceneContext(QString* error);
    void RefreshSliceSettings();
    void RefreshSliceJobReadiness();
    void OnTransformRequested(
        const QStringList& instanceIds,
        double deltaXMm,
        double deltaYMm,
        double rotateXDegrees,
        double rotateYDegrees,
        double rotateZDegrees,
        double uniformScaleFactor,
        bool mirrorX,
        bool mirrorY,
        bool landOnBuildPlate);
    void OnLandOnBuildPlateRequested(const QStringList& instanceIds);
    void OnLayoutRequested(
        int maxColumns,
        int maxRows,
        double columnGapMm,
        double rowGapMm);
    void SetSceneCommandsEnabled(bool enabled);
    void SetWorkflowEditingEnabled(bool enabled);
    void ShowSceneEditResult(
        const QString& action,
        const hostsceneeditresult& result);
    void ShowSceneEditError(const QString& action, const QString& error);
    void ShowImportResult(const hostmodelimportresult& result);
    void ShowImportError(const QString& error);
    void InitializeViewWorkspace();
    void RefreshTopView();
    void RefreshThreeDView();
    void RefreshSceneViews();
    void RenderThreeDView();
    bool BeginTopViewDrag(const QPointF& imagePoint);
    void UpdateTopViewDrag(const QPointF& imagePoint);
    void FinishTopViewDrag();
    void RenderTransientTopView();
    void RefreshTextureWhitePreflight();
    void OnTextureWhitePreflightFinished(
        const hosttexturewhitepreflightresult& result);
    void OnTextureWhitePreflightDiscarded(quint64 generation);

    ModuleClient m_client;
    std::unique_ptr<HostModelImportWorkflow> m_importWorkflow;
    std::unique_ptr<HostSliceJobController> m_sliceJobController;
    std::unique_ptr<HostRipJobController> m_ripJobController;
    std::unique_ptr<HostPackageReviewController> m_packageReviewController;
    std::unique_ptr<IHostProfileCatalog> m_profileCatalog;
    std::unique_ptr<ViewPresentationSettings> m_viewSettings;
    std::unique_ptr<TopViewRenderPolicy> m_topViewPolicy;
    std::unique_ptr<TopViewFrame> m_topViewFrame;
    std::unique_ptr<MoveOptimizationPolicy> m_movePolicy;
    std::unique_ptr<SceneInteractionController> m_interactionController;
    std::unique_ptr<CpuRasterBackend> m_threeDBackend;
    std::unique_ptr<SceneRenderPolicy> m_threeDPolicy;
    std::unique_ptr<ThreeDFrame> m_threeDFrame;
    std::unique_ptr<HostTextureWhitePreflightService>
        m_textureWhitePreflightService;
    ViewWorkspaceWidget* m_workspace{nullptr};
    QSplitter* m_workspaceSplitter{nullptr};
    QTabWidget* m_workspaceTabs{nullptr};
    QTabWidget* m_inspectorTabs{nullptr};
    QComboBox* m_defaultViewCombo{nullptr};
    QComboBox* m_projectionCombo{nullptr};
    QLabel* m_versionLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_pathLabel{nullptr};
    HostModelListPanel* m_modelListPanel{nullptr};
    HostProfilePanel* m_profilePanel{nullptr};
    HostSliceSettingsPanel* m_sliceSettingsPanel{nullptr};
    HostSliceJobPanel* m_sliceJobPanel{nullptr};
    HostRipSettingsPanel* m_ripSettingsPanel{nullptr};
    HostPackageReviewPanel* m_packageReviewPanel{nullptr};
    HostTransformLayoutPanel* m_transformLayoutPanel{nullptr};
    QLabel* m_importSummaryLabel{nullptr};
    QTableWidget* m_preflightTable{nullptr};
    QPlainTextEdit* m_moduleInfoView{nullptr};
    QString m_selectedProfileId;
    QString m_restoredProfileId;
    QString m_modelImportDirectory;
    QString m_ripModuleDirectory;
    QPointF m_dragStartWorld;
    quint64 m_dragCallCount{0U};
    bool m_textureWhiteWarning{false};
    bool m_resultLoadActive{false};
};
