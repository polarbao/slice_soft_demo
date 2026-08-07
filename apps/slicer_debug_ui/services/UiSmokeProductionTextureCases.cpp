#include "UiSmokeTestInternal.h"

using ui_smoke_test_support::BuildExperimentalReportFixture;
using ui_smoke_test_support::BuildMaterialClosureReportFixture;
using ui_smoke_test_support::BuildOpenVdbUtilityReportFixture;
using ui_smoke_test_support::ClosedBoxObjFixture;
using ui_smoke_test_support::ContainsAll;
using ui_smoke_test_support::GlobalRect;
using ui_smoke_test_support::OpenTriangleObjFixture;
using ui_smoke_test_support::ReadJsonObject;
using ui_smoke_test_support::WaitForCondition;
using ui_smoke_test_support::WriteJsonFixture;
using ui_smoke_test_support::WritePreflightFixture;

int UiSmokeTestRunner::ProductionTextureControls(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QWidget* sliceSettingsPage =
        window.findChild<QWidget*>(
            QStringLiteral("contextSliceSettingsPage"));
    QWidget* diagnosticPanel =
        window.findChild<QWidget*>(
            QStringLiteral("diagnosticSettingsPanel"));
    QSpinBox* legacyLayers =
        window.findChild<QSpinBox*>(
            QStringLiteral("productionLegacyTopLayersSpin"));
    QDoubleSpinBox* globalWidth =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("productionGlobalTextureWidthSpin"));
    QComboBox* globalMode =
        window.findChild<QComboBox*>(
            QStringLiteral("productionGlobalTextureModeCombo"));
    QComboBox* singleMaterial =
        window.findChild<QComboBox*>(
            QStringLiteral("productionSingleMaterialCombo"));
    QLabel* stateLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionSettingsStateLabel"));
    QLabel* noticeLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionSettingsNoticeLabel"));
    if (inspector == nullptr
        || sliceSettingsPage == nullptr
        || diagnosticPanel == nullptr
        || legacyLayers == nullptr
        || globalWidth == nullptr
        || globalMode == nullptr
        || singleMaterial == nullptr
        || stateLabel == nullptr
        || noticeLabel == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 production controls missing"));
    }
    if (sliceSettingsPage->isAncestorOf(diagnosticPanel)
        || !noticeLabel->text().contains(QStringLiteral("诊断宽度")))
    {
        return fail(QStringLiteral(
            "12E-09D-04 diagnostic and production controls are not separated"));
    }

    const ScenarioEntry* legacyScenario =
        window.m_scenarioRegistry.FindById(
            QStringLiteral("textured_nail_rgb_white_lower_support"));
    if (legacyScenario == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 legacy scenario missing"));
    }
    window.ApplyScenario(*legacyScenario);
    QApplication::processEvents();
    legacyLayers->setValue(3);
    QApplication::processEvents();
    const SliceSettingsState legacySettings =
        window.BuildCurrentSettings(
            {},
            {},
            SliceEngineRole::LegacyProduction);
    if (!legacyLayers->isEnabled()
        || window.config_document_.value(
               {"texture", "topSurfaceLayers"})
               .toInt()
            != 3
        || !legacySettings.productiontextureoverrideenabled
        || legacySettings.productiontexture.effectivetoplayers != 3
        || std::abs(
               legacySettings.productiontexture
                       .effectivetopthicknessmm
                   - 3.0 * legacySettings.layerthicknessmm)
            > 1.0e-9
        || !window.config_document_.isDirty()
        || !stateLabel->text().contains(QStringLiteral("stale")))
    {
        return fail(QStringLiteral(
            "12E-09D-04 legacy edit/effective/stale mismatch"));
    }
    const EffectiveConfigResult legacyEffective =
        window.GenerateEffectiveConfig(
            {},
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("ui_smoke_09d_legacy"));
    if (!legacyEffective.IsValid()
        || legacyEffective.document.object()
               .value(QStringLiteral("texture"))
               .toObject()
               .value(QStringLiteral("topSurfaceLayers"))
               .toInt()
            != 3
        || !legacyEffective.summary.contains(
            QStringLiteral("生产纹理：legacy_top_band / 3 层")))
    {
        return fail(QStringLiteral(
            "12E-09D-05 legacy one-click effective config mismatch: ")
            + legacyEffective.errors.join(QStringLiteral("；")));
    }

    QTemporaryDir saveDirectory;
    const QString savedPath = saveDirectory.filePath(
        QStringLiteral("production-settings.json"));
    SaveOptions saveOptions;
    saveOptions.allowOverwriteWithoutPrompt = true;
    if (!window.config_document_.saveAs(
            savedPath,
            nullptr,
            saveOptions))
    {
        return fail(QStringLiteral(
            "12E-09D-04 save failed: ")
            + window.config_document_.errorString());
    }
    ConfigDocument reloaded;
    if (!reloaded.load(savedPath)
        || reloaded.value(
               {"texture", "topSurfaceLayers"})
               .toInt()
            != 3)
    {
        return fail(QStringLiteral(
            "12E-09D-04 save/readback mismatch"));
    }

    const ScenarioEntry* whiteOnDemandScenario =
        window.m_scenarioRegistry.FindById(
            QStringLiteral(
                "textured_nail_rgb_white_ondemand_lower_support"));
    if (whiteOnDemandScenario == nullptr)
    {
        return fail(QStringLiteral(
            "Stage 15 production scenario missing"));
    }
    window.ApplyScenario(*whiteOnDemandScenario);
    QApplication::processEvents();
    const EffectiveConfigResult whiteOnDemandEffective =
        window.GenerateEffectiveConfig(
            {},
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("scene_legacy"));
    const QString compactSessionName = QFileInfo(
        whiteOnDemandEffective.generatedconfigpath)
        .absoluteDir()
        .dirName();
    if (!whiteOnDemandEffective.IsValid()
        || compactSessionName.size() > 72)
    {
        return fail(QStringLiteral(
            "Stage 15 production session path was not compacted: ")
            + compactSessionName);
    }

    const ScenarioEntry* singleScenario =
        window.m_scenarioRegistry.FindById(
            QStringLiteral("single_material_relief"));
    if (singleScenario == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 single-material scenario missing"));
    }
    window.ApplyScenario(*singleScenario);
    QApplication::processEvents();
    const int varnishIndex = singleMaterial->findData(
        static_cast<int>(SingleMaterialReliefMaterial::Varnish));
    singleMaterial->setCurrentIndex(varnishIndex);
    QApplication::processEvents();
    const SliceSettingsState singleSettings =
        window.BuildCurrentSettings(
            {},
            {},
            SliceEngineRole::LegacyProduction);
    if (!singleMaterial->isEnabled()
        || window.config_document_.value(
               {"modelMaterial", "materialChannel"})
               .toString()
            != QStringLiteral("V")
        || window.config_document_.value(
               {"modelMaterial", "whiteValue"})
               .toInt()
            != 255
        || window.config_document_.value(
               {"modelMaterial", "varnishValue"})
               .toInt()
            != 0
        || !singleSettings.singlematerialreliefoverrideenabled
        || singleSettings.singlematerialrelief.effectivechannel
            != QStringLiteral("V"))
    {
        return fail(QStringLiteral(
            "12E-09D-04 single-material atomic edit mismatch"));
    }
    const EffectiveConfigResult singleEffective =
        window.GenerateEffectiveConfig(
            {},
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("ui_smoke_09d_single"));
    if (!singleEffective.IsValid()
        || singleEffective.document.object()
               .value(QStringLiteral("modelMaterial"))
               .toObject()
               .value(QStringLiteral("materialChannel"))
               .toString()
            != QStringLiteral("V")
        || !singleEffective.summary.contains(
            QStringLiteral("单材料浮雕：varnish / channel=V")))
    {
        return fail(QStringLiteral(
            "12E-09D-05 single-material one-click effective config mismatch: ")
            + singleEffective.errors.join(QStringLiteral("；")));
    }

    QComboBox* productionMode =
        window.findChild<QComboBox*>(
            QStringLiteral("productionModeCombo"));
    QComboBox* productionProfile =
        window.findChild<QComboBox*>(
            QStringLiteral("productionProfileCombo"));
    if (productionMode == nullptr
        || productionProfile == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 production selector missing"));
    }
    const int globalModeIndex = productionMode->findData(
        QStringLiteral("global_surface_shell"));
    productionMode->setCurrentIndex(globalModeIndex);
    QApplication::processEvents();
    const int globalProfileIndex = productionProfile->findData(
        QStringLiteral(
            "global_surface_shell_material_parity_candidate"));
    productionProfile->setCurrentIndex(globalProfileIndex);
    QApplication::processEvents();
    globalWidth->setValue(0.37);
    QApplication::processEvents();
    const int allTextureIndex = globalMode->findData(
        static_cast<int>(ProductionTexturePartitionMode::AllTexture));
    globalMode->setCurrentIndex(allTextureIndex);
    QApplication::processEvents();
    const SliceSettingsState globalSettings =
        window.BuildCurrentSettings(
            {},
            {},
            SliceEngineRole::LegacyProduction);
    const QJsonObject storedGlobal =
        window.config_document_.document()
            .object()
            .value(QStringLiteral("uiProductionSettings"))
            .toObject()
            .value(QStringLiteral("globalSurfaceShellOverrides"))
            .toObject()
            .value(QStringLiteral(
                "global_surface_shell_material_parity_candidate"))
            .toObject();
    if (!globalMode->isEnabled()
        || globalWidth->isEnabled()
        || storedGlobal.value(QStringLiteral("mode")).toString()
            != QStringLiteral("all_texture")
        || std::abs(
               storedGlobal.value(QStringLiteral("widthMm")).toDouble()
                   - 0.37)
            > 1.0e-9
        || !globalSettings.productiontextureoverrideenabled
        || globalSettings.productiontexture.partitionmode
            != ProductionTexturePartitionMode::AllTexture)
    {
        return fail(QStringLiteral(
            "12E-09D-04 global edit/persistence mismatch"));
    }
    const EffectiveConfigResult globalEffective =
        window.GenerateEffectiveConfig(
            {},
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("ui_smoke_09d_global"));
    const QJsonObject globalSurfaceShell = globalEffective.document
        .object()
        .value(QStringLiteral("texture"))
        .toObject()
        .value(QStringLiteral("surfaceShell"))
        .toObject();
    if (!globalEffective.IsValid()
        || globalSurfaceShell.value(QStringLiteral("mode")).toString()
            != QStringLiteral("all_texture")
        || std::abs(
               globalSurfaceShell.value(QStringLiteral("widthMm")).toDouble()
                   - 0.37)
            > 1.0e-9
        || !globalEffective.summary.contains(
            QStringLiteral(
                "生产纹理：global_surface_shell / all_texture")))
    {
        return fail(QStringLiteral(
            "12E-09D-05 Global one-click effective config mismatch: ")
            + globalEffective.errors.join(QStringLiteral("；")));
    }

    return pass(QStringLiteral(
        "production-texture-controls legacy/global/single/"
        "diagnostic-separated/stale/save-readback/one-click-effective/"
        "compact-session-path"));
}
