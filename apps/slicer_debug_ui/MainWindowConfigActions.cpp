#include "MainWindowInternal.h"

using slicer_debug_ui_internal::ApplyStoredGlobalTextureOverride;
using slicer_debug_ui_internal::BuildProductionSessionName;
using slicer_debug_ui_internal::ConfigureLongTextCombo;
using slicer_debug_ui_internal::IsSlicingAction;
using slicer_debug_ui_internal::MakeDefaultModelFillConfig;
using slicer_debug_ui_internal::MakeDefaultOuterVarnishConfig;
using slicer_debug_ui_internal::MakeDefaultPreviewPseudoColors;
using slicer_debug_ui_internal::MakeDefaultSupportConfig;
using slicer_debug_ui_internal::MakeDefaultSurfaceVarnishConfig;
using slicer_debug_ui_internal::MakeIntArray;
using slicer_debug_ui_internal::MakeNumberArray;
using slicer_debug_ui_internal::MakeScenarioDisplayLabel;
using slicer_debug_ui_internal::MakeScenarioToolTip;
using slicer_debug_ui_internal::MakeStringArray;
using slicer_debug_ui_internal::MaterialCapabilityLabel;
using slicer_debug_ui_internal::ParseSupportPlacement;
using slicer_debug_ui_internal::ProductionModeSessionTag;
using slicer_debug_ui_internal::ProductionSafetyLabel;
using slicer_debug_ui_internal::ResolveEffectiveProfileId;
using slicer_debug_ui_internal::ResolveModelPath;
using slicer_debug_ui_internal::SanitizeSessionName;
using slicer_debug_ui_internal::SessionIdFromConfigPath;
using slicer_debug_ui_internal::StoreGlobalTextureOverride;
using slicer_debug_ui_internal::UpdateComboPopupWidth;
using slicer_debug_ui_internal::addPathRow;
using slicer_debug_ui_internal::makeButton;
using slicer_debug_ui_internal::makePathEdit;

QString MainWindow::CreateOneClickConfig(const QString& modelPath, QString* packageDir)
{
    const slicer_core::SlicePipelineMode selectedMode =
        config_editor_panel_->SelectedProductionMode();
    const EffectiveConfigResult result = GenerateEffectiveConfig(
        modelPath,
        QString{},
        SliceEngineRole::LegacyProduction,
        ProductionModeSessionTag(selectedMode));
    if (!result.IsValid())
    {
        status_label_->setText("一键切片配置校验失败。");
        log_panel_->appendError(result.errors.join("\n"));
        return {};
    }

    if (packageDir != nullptr)
    {
        *packageDir = absoluteFromRepo(
            result.document.object().value("output").toObject().value("packageDir").toString());
    }
    return result.generatedconfigpath;
}

QString MainWindow::CreateOpenVdbCandidateConfig(const QString& modelPath, QString* packageDir) const
{
    const QFileInfo modelInfo(modelPath);
    if (!modelInfo.exists() || !modelInfo.isFile())
    {
        QMessageBox::warning(nullptr, "模型文件不存在", "无法找到模型文件：\n" + modelPath);
        return {};
    }

    const QString suffix = modelInfo.suffix().toLower();
    if (suffix != "obj" && suffix != "3mf")
    {
        QMessageBox::warning(nullptr, "模型格式不支持", "OpenVDB 候选切片当前只接受 OBJ / 3MF 模型。");
        return {};
    }

    const QString sessionName = SanitizeSessionName(modelInfo.completeBaseName())
        + "_openvdb_candidate_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString sessionRoot = "output/ui_sessions/" + sessionName;
    const QString relativePackageDir = sessionRoot + "/package";
    const int dpiX = config_document_.value({"output", "dpiX"})
                         .toInt(slicer_core::kDefaultOutputDpiX);
    const int dpiY = config_document_.value({"output", "dpiY"})
                         .toInt(slicer_core::kDefaultOutputDpiY);

    QDir repo(paths_.repo_root);
    if (!repo.mkpath(sessionRoot))
    {
        QMessageBox::warning(nullptr, "无法创建会话目录", "无法创建目录：\n" + repo.filePath(sessionRoot));
        return {};
    }

    QJsonObject root;
    root.insert("slicingMode", "relief_heightfield");
    root.insert("input",
                QJsonObject{{"modelPath", QDir::fromNativeSeparators(modelInfo.absoluteFilePath())}, {"format", "auto"}});
    root.insert("output",
                QJsonObject{{"packageDir", relativePackageDir},
                            {"dpiX", dpiX},
                            {"dpiY", dpiY},
                            {"layerThicknessMm",
                             slicer_core::kDefaultLayerThicknessMm},
                            {"channelOrder", MakeStringArray({"R", "G", "B", "W", "S", "V"})},
                            {"bitDepth", 8},
                            {"planarConfig", "contiguous"},
                            {"storageMode", "stripped"},
                            {"rowsPerStrip", 64}});
    root.insert("modelTransform",
                QJsonObject{{"unit", "mm"},
                            {"scale", MakeNumberArray({1.0, 1.0, 1.0})},
                            {"rotationDeg", MakeNumberArray({0.0, 0.0, 0.0})},
                            {"translationMm", MakeNumberArray({0.0, 0.0, 0.0})}});
    root.insert("autoOrient",
                QJsonObject{{"enabled", true},
                            {"maxHeightMm", 9.0},
                            {"strategy", "minimize_height_by_right_angle_rotation"}});
    root.insert("background", QJsonObject{{"value", 255}});
    root.insert("modelMaterial",
                QJsonObject{{"materialChannel", "RGB"},
                            {"applyMode", "solid_volume"},
                            {"rgb", MakeIntArray({0, 0, 0})},
                            {"whiteValue", 255},
                            {"varnishValue", 255}});
    root.insert("texture",
                QJsonObject{{"enabled", true},
                            {"applyMode", "surface_shell_from_sdf"},
                            {"sampler", "nearest"},
                            {"uvAddressMode", "clamp"},
                            {"flipV", true},
                            {"fallbackRgb", MakeIntArray({255, 0, 255})},
                            {"missingTexturePolicy", "fail_fast"},
                            {"nonSurfaceRgbPolicy", "empty"}});
    root.insert("modelFill", MakeDefaultModelFillConfig());
    root.insert("support", MakeDefaultSupportConfig());
    root.insert("surfaceVarnish", MakeDefaultSurfaceVarnishConfig());
    root.insert("outerVarnish", MakeDefaultOuterVarnishConfig());
    root.insert("relief", QJsonObject{{"fillMode", "intersection_range"}, {"baseZMm", 0.0}});
    root.insert("preview",
                QJsonObject{{"enabled", true},
                            {"format", "ppm"},
                            {"interval", 1},
                            {"channels", MakeStringArray({"texture_rgb", "rgb", "support", "white", "varnish"})},
                            {"onlyNonEmptyLayers", false},
                            {"pseudoColors", MakeDefaultPreviewPseudoColors()}});
    root.insert("experimental",
                QJsonObject{{"openvdbPipeline",
                             QJsonObject{{"enabled", true},
                                         {"engine", "openvdb"},
                                         {"admissionMode", "strict_closed"},
                                         {"failurePolicy", "non_production_only"},
                                         {"allowNonProductionOutput", true},
                                         {"writeProductionRgbwsv", true}}}});

    const QString configPath = repo.filePath(sessionRoot + "/slice_config.openvdb_candidate.json");
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QMessageBox::warning(nullptr, "无法写入配置", "无法写入配置文件：\n" + configPath);
        return {};
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

    if (packageDir != nullptr)
    {
        *packageDir = repo.filePath(relativePackageDir);
    }
    return configPath;
}

QString MainWindow::CreateOpenVdbReportPath(const QString& modelPath) const
{
    const QFileInfo modelInfo(modelPath);
    const QString sessionName = SanitizeSessionName(modelInfo.completeBaseName())
        + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString reportDir = "output/ui_sessions/" + sessionName + "_openvdb/reports";
    QDir repo(paths_.repo_root);
    repo.mkpath(reportDir);
    return repo.filePath(reportDir + "/experimental_openvdb_shell_report.json");
}

void MainWindow::RunGeneratedConfig(const SlicePreflightAction& action)
{
    ProductionSliceRunRequest request;
    request.mode = action.productionmode;
    request.profileid = action.productionprofileid;
    request.sessionid = action.sessionid;
    request.configpath = action.configpath;
    request.packagedir = action.packagedir;
    const QStringList errors = m_productionRunSession.Begin(request);
    if (!errors.isEmpty())
    {
        status_label_->setText(QStringLiteral("生产切片会话身份无效，进程未启动。"));
        log_panel_->appendError(errors.join(QStringLiteral("\n")));
        return;
    }

    config_editor_panel_->ClearProductionResult();
    config_edit_->setText(action.configpath);
    package_edit_->setText(action.packagedir);
    pending_package_.clear();
    runCommand(
        "运行切片",
        paths_.slicer_cli,
        QStringList{"--config", action.configpath});
}

void MainWindow::RunOpenVdbDiagnostic(const QString& configPath, const QString& reportPath)
{
    config_edit_->setText(configPath);
    compare_output_ = reportPath;
    pending_package_.clear();
    runCommand("OpenVDB 实验诊断",
               paths_.slicer_cli,
               QStringList{"--config",
                           configPath,
                           "--experimental-openvdb-shell",
                           "--admission-mode",
                           "diagnostic_only",
                           "--experimental-report",
                           reportPath});
}

void MainWindow::RunOpenVdbCandidate(const QString& configPath, const QString& packageDir)
{
    config_edit_->setText(configPath);
    package_edit_->setText(packageDir);
    m_suppressPreflightStale = true;
    config_editor_panel_->loadConfig(configPath);
    m_suppressPreflightStale = false;
    pending_package_ = packageDir;
    runCommand("OpenVDB 候选切片",
               paths_.openvdb_slicer_cli,
               QStringList{"--config", configPath, "--openvdb-candidate-slice"});
}

void MainWindow::RequestSlicePreflight(const SlicePreflightAction& action)
{
    config_edit_->setText(action.configpath);
    if (!action.packagedir.isEmpty())
    {
        package_edit_->setText(action.packagedir);
    }
    pending_package_.clear();
    m_productionRunSession.Invalidate();
    if (action.kind == SlicePreflightActionKind::Legacy
        || action.kind == SlicePreflightActionKind::GlobalProduction)
    {
        config_editor_panel_->ShowProductionAdmissionState(
            ProductionAdmissionState::Running,
            QStringLiteral("正在执行当前模型和所选生产模式的预检。"));
    }
    status_label_->setText(QStringLiteral("正在执行模型预检，尚未启动切片进程。"));
    m_slicePreflightCoordinator.RequestAction(action);
}

void MainWindow::UpdateModelPreflightUi()
{
    ModelPreflightPresentation presentation =
        ModelPreflightPresenter::Present(
            m_modelPreflightController.CurrentExecution(),
            m_modelPreflightController.CurrentMode());
    if (!m_modelPreflightController.LastCapabilityDiagnostic().isEmpty())
    {
        presentation.detail += QStringLiteral("；capability=")
            + m_modelPreflightController.LastCapabilityDiagnostic();
    }
    if (m_modelPreflightPanel != nullptr)
    {
        m_modelPreflightPanel->ShowPresentation(presentation);
    }
    if (m_modelPreflightCompactState != nullptr)
    {
        m_modelPreflightCompactState->setText(
            QStringLiteral("预检：") + presentation.state);
        m_modelPreflightCompactState->setToolTip(
            presentation.admission + QStringLiteral("\n") + presentation.detail);
    }
    if (m_modelPreflightCompactMode != nullptr)
    {
        m_modelPreflightCompactMode->setText(presentation.mode);
    }
    if (m_modelPreflightRecheckButton != nullptr)
    {
        m_modelPreflightRecheckButton->setEnabled(
            presentation.canrecheck && !m_processBusy);
    }
    if (m_modelPreflightCancelButton != nullptr)
    {
        m_modelPreflightCancelButton->setEnabled(presentation.cancancel);
    }
}

QStringList MainWindow::CurrentProfileCapabilities() const
{
    const ScenarioEntry* scenario =
        m_scenarioRegistry.FindById(m_currentProfileId);
    return scenario != nullptr
        ? scenario->materialcapabilities
        : QStringList{};
}

QString MainWindow::BuildTextureWhitePreflightContentHash() const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    bool hasVisibleTexture{false};
    for (const SceneDocumentItem& item : m_sceneDocument.Items())
    {
        if (!item.instance.visible || !item.geometry.has_value())
        {
            continue;
        }

        hash.addData(item.sourcehash.toUtf8());
        hash.addData("\0", 1);
        hash.addData(item.resourcehash.toUtf8());
        hash.addData("\0", 1);
        hash.addData(
            QByteArray::fromStdString(
                item.geometry->surfacepreview.contenthash));
        hash.addData("\0", 1);
        for (const slicer_core::SceneViewMaterialAppearance& appearance :
             item.geometry->materialappearances)
        {
            if (!appearance.hastexture
                || !appearance.textureexists
                || appearance.texturepath.empty())
            {
                continue;
            }
            hasVisibleTexture = true;
            hash.addData(
                QDir::fromNativeSeparators(
                    QString::fromStdWString(
                        appearance.texturepath.wstring()))
                    .toUtf8());
            hash.addData("\0", 1);
        }
    }
    return hasVisibleTexture
        ? QString::fromLatin1(hash.result().toHex())
        : QString{};
}

void MainWindow::RequestTextureWhitePreflight()
{
    if (m_sceneDocument.State() != SceneDocumentState::Ready
        && m_sceneDocument.State() != SceneDocumentState::Blocked)
    {
        if (m_textureWhitePreflightService.IsRunning())
        {
            m_textureWhitePreflightService.Cancel();
        }
        m_lastTextureWhitePreflightResult.reset();
        m_textureWhitePreflightRequestKey.clear();
        return;
    }

    QStringList texturePaths;
    for (const SceneDocumentItem& item : m_sceneDocument.Items())
    {
        if (!item.instance.visible || !item.geometry.has_value())
        {
            continue;
        }
        for (const slicer_core::SceneViewMaterialAppearance& appearance :
             item.geometry->materialappearances)
        {
            if (appearance.hastexture
                && appearance.textureexists
                && !appearance.texturepath.empty())
            {
                texturePaths.push_back(
                    QDir::fromNativeSeparators(
                        QString::fromStdWString(
                            appearance.texturepath.wstring())));
            }
        }
    }
    texturePaths.removeDuplicates();

    const QString contentHash =
        BuildTextureWhitePreflightContentHash();
    if (texturePaths.isEmpty() || contentHash.isEmpty())
    {
        if (m_textureWhitePreflightService.IsRunning())
        {
            m_textureWhitePreflightService.Cancel();
        }
        m_lastTextureWhitePreflightResult.reset();
        m_textureWhitePreflightRequestKey.clear();
        return;
    }

    const QString requestKey = QStringLiteral("%1|%2|%3|%4")
        .arg(
            m_sceneDocument.SceneId(),
            QString::number(m_sceneDocument.SceneRevision()),
            contentHash,
            m_currentProfileId);
    if (requestKey == m_textureWhitePreflightRequestKey)
    {
        return;
    }

    m_textureWhitePreflightRequestKey = requestKey;
    m_lastTextureWhitePreflightResult.reset();
    TextureWhitePreflightRequest request;
    request.sceneid = m_sceneDocument.SceneId();
    request.scenerevision = m_sceneDocument.SceneRevision();
    request.contenthash = contentHash;
    request.profileid = m_currentProfileId;
    request.texturepaths = texturePaths;
    request.profilecapabilities =
        CurrentProfileCapabilities();
    m_textureWhitePreflightService.RequestScan(request);
}
