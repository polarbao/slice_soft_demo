#pragma once

#include "UiSmokeTestRunner.h"

#include "../MainWindow.h"
#include "../controllers/SceneBatchImportController.h"
#include "../controllers/SceneTransformController.h"
#include "../models/SceneDocument.h"
#include "../models/SceneSelectionModel.h"
#include "../widgets/ChannelChartPanel.h"
#include "../widgets/ConfigEditorPanel.h"
#include "../widgets/ContextInspector.h"
#include "../widgets/DiagnosticsDock.h"
#include "../widgets/DiagnosticSemanticPreviewPanel.h"
#include "../widgets/LayerPreviewPanel.h"
#include "../widgets/MaterialClosurePanel.h"
#include "../widgets/ModelListPanel.h"
#include "../widgets/ModelPreflightPanel.h"
#include "../widgets/ModelTopViewWidget.h"
#include "../widgets/ModelTransformPanel.h"
#include "../widgets/PreviewOverlayPanel.h"
#include "../widgets/PreviewPanel.h"
#include "../widgets/PreviewWorkspace.h"
#include "../widgets/ProductionModePanel.h"
#include "../widgets/ProductionTextureSettingsPanel.h"
#include "../widgets/ProjectToolsDock.h"
#include "../widgets/QuickConfigPanel.h"
#include "../widgets/ReportPanel.h"
#include "../widgets/SceneLayoutPanel.h"
#include "../widgets/SettingHelpPanel.h"
#include "../widgets/SliceTimingPanel.h"
#include "ConfigDocument.h"
#include "EffectiveConfigGenerator.h"
#include "HelpTextProvider.h"
#include "ModelPreflightController.h"
#include "ModelPreflightPresenter.h"
#include "ModelTopViewLoader.h"
#include "PackageLoader.h"
#include "PreviewReportIndex.h"
#include "ReportLoader.h"
#include "ScenarioRegistry.h"
#include "SceneModelRepository.h"
#include "SliceProgressProtocolParser.h"
#include "SliceSettingsModel.h"
#include "SlicePreflightCoordinator.h"
#include "ToolPaths.h"
#include "TransformedModelPreflightLoader.h"
#include "WorkspaceLayoutState.h"
#include "slicer_core/config.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QRect>
#include <QSet>
#include <QSettings>
#include <QSize>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>

namespace ui_smoke_test_support
{

QJsonObject BuildExperimentalReportFixture();
QJsonObject BuildOpenVdbUtilityReportFixture(bool openVdbAvailable);
QJsonObject BuildMaterialClosureReportFixture(
    const QString& confidence,
    const QString& closureStatus,
    const QString& productionAcceptance,
    int totalGapPixels,
    int repairedPixels,
    int remainingGapPixels,
    const QString& gapPreviewPath);
bool WriteJsonFixture(const QString& path, const QJsonObject& object);
bool ReadJsonObject(const QString& path, QJsonObject* object);
bool ContainsAll(const QString& text, const QStringList& expected);
QByteArray ClosedBoxObjFixture();
QByteArray OpenTriangleObjFixture();
QString WritePreflightFixture(
    const QString& directory,
    const QString& name,
    const QByteArray& objContent);
bool WaitForCondition(
    const std::function<bool()>& condition,
    int timeoutMs = 30000);
QRect GlobalRect(const QWidget* widget);

}  // namespace ui_smoke_test_support
