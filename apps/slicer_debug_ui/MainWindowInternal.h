#pragma once

#include "MainWindow.h"

#include "services/ProductionModeCatalog.h"
#include "services/ProductionProfileSourceResolver.h"
#include "services/ProductionTextureSettingsModel.h"
#include "widgets/MaterialClosurePanel.h"
#include "slicer_core/config.h"
#include "slicer_core/pipeline/DiagnosticEffectiveConfig.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace slicer_debug_ui_internal
{

QJsonObject ApplyStoredGlobalTextureOverride(
    const QJsonObject& sceneDraft,
    const QString& profileId,
    QJsonObject globalConfig);
QJsonObject StoreGlobalTextureOverride(
    QJsonObject sceneDraft,
    const QString& profileId,
    const ProductionTextureControlState& state);
QPushButton* makeButton(const QString& text, QWidget* parent);
QLineEdit* makePathEdit(const QString& text, QWidget* parent);
void addPathRow(
    QVBoxLayout* layout,
    const QString& label,
    QLineEdit* edit,
    QPushButton* browse);
QString MakeScenarioDisplayLabel(const ScenarioEntry& scenario);
QString ResolveEffectiveProfileId(
    const QJsonObject& profileRoot,
    const QString& requestedProfileId);
QString ProductionSafetyLabel(const QString& value);
QString MaterialCapabilityLabel(const QString& value);
bool IsSlicingAction(const QString& action);
QString ProductionModeSessionTag(slicer_core::SlicePipelineMode mode);
QString SessionIdFromConfigPath(const QString& configPath);
QString MakeScenarioToolTip(const ScenarioEntry& scenario);
void ConfigureLongTextCombo(QComboBox* combo, int minimumContentsLength);
void UpdateComboPopupWidth(QComboBox* combo);
QJsonArray MakeNumberArray(std::initializer_list<double> values);
QJsonArray MakeIntArray(std::initializer_list<int> values);
QJsonArray MakeStringArray(std::initializer_list<QString> values);
QJsonObject MakeDefaultModelFillConfig();
QJsonObject MakeDefaultSupportConfig();
QJsonObject MakeDefaultOuterVarnishConfig();
QJsonObject MakeDefaultSurfaceVarnishConfig();
QJsonObject MakeDefaultPreviewPseudoColors();
QString SanitizeSessionName(const QString& name);
QString BuildProductionSessionName(
    const QString& modelName,
    const QString& profileId,
    const QString& sessionTag,
    const QString& timestamp);
SupportPlacement ParseSupportPlacement(const QString& value);
QString ResolveModelPath(
    const QString& modelPath,
    const QString& configPath);

}  // namespace slicer_debug_ui_internal
