#include "HostWorkspaceState.h"
#include "HostWorkspaceMatvolState.h"
#include "HostWorkspaceTextureState.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QVariantList>

#include <cmath>

namespace
{
const QString kSettingsGroup = QStringLiteral("hostflow/workspace");
const QString kSchemaId = QStringLiteral("hostflow.workspace.1");

QString MaterialStrategyId(const HostMaterialStrategy strategy)
{
    switch (strategy)
    {
    case HostMaterialStrategy::RgbSolid:
        return QStringLiteral("rgb_solid");
    case HostMaterialStrategy::RgbWhite:
        return QStringLiteral("rgb_white");
    case HostMaterialStrategy::RgbVarnish:
        return QStringLiteral("rgb_varnish");
    case HostMaterialStrategy::RgbWhiteVarnish:
        return QStringLiteral("rgb_white_varnish");
    case HostMaterialStrategy::WhiteSolid:
        return QStringLiteral("white_solid");
    case HostMaterialStrategy::VarnishSolid:
        return QStringLiteral("varnish_solid");
    }
    return {};
}

bool ParseMaterialStrategy(
    const QString& identifier,
    HostMaterialStrategy* strategy)
{
    if (strategy == nullptr)
    {
        return false;
    }
    if (identifier == QStringLiteral("rgb_solid"))
    {
        *strategy = HostMaterialStrategy::RgbSolid;
        return true;
    }
    if (identifier == QStringLiteral("white_solid"))
    {
        *strategy = HostMaterialStrategy::WhiteSolid;
        return true;
    }
    if (identifier == QStringLiteral("rgb_white"))
    {
        *strategy = HostMaterialStrategy::RgbWhite;
        return true;
    }
    if (identifier == QStringLiteral("rgb_varnish"))
    {
        *strategy = HostMaterialStrategy::RgbVarnish;
        return true;
    }
    if (identifier == QStringLiteral("rgb_white_varnish"))
    {
        *strategy = HostMaterialStrategy::RgbWhiteVarnish;
        return true;
    }
    if (identifier == QStringLiteral("varnish_solid"))
    {
        *strategy = HostMaterialStrategy::VarnishSolid;
        return true;
    }
    return false;
}

bool ParseMaterialRole(
    const QString& identifier,
    HostMaterialRole* role)
{
    if (role == nullptr)
    {
        return false;
    }
    for (const HostMaterialRole candidate : {
             HostMaterialRole::Rgb,
             HostMaterialRole::White,
             HostMaterialRole::Varnish,
             HostMaterialRole::Ignore,
             HostMaterialRole::SupportCandidate})
    {
        if (HostEffectiveProfileBuilder::MaterialRoleId(candidate)
            == identifier)
        {
            *role = candidate;
            return true;
        }
    }
    return false;
}

bool ParseSupportMode(
    const QString& identifier,
    HostSupportMode* mode)
{
    if (mode == nullptr)
    {
        return false;
    }
    for (const HostSupportMode candidate : {
             HostSupportMode::None,
             HostSupportMode::BottomProjection,
             HostSupportMode::UnsupportedOnly,
             HostSupportMode::BottomProjectionPlusUnsupported,
             HostSupportMode::FullVerticalProjection})
    {
        if (HostEffectiveProfileBuilder::SupportModeId(candidate)
            == identifier)
        {
            *mode = candidate;
            return true;
        }
    }
    return false;
}

bool ParseGeometrySamplingStrategy(
    const QString& identifier,
    HostGeometrySamplingStrategy* strategy)
{
    if (strategy == nullptr)
    {
        return false;
    }
    for (const HostGeometrySamplingStrategy candidate : {
             HostGeometrySamplingStrategy::LegacyCenterSample,
             HostGeometrySamplingStrategy::
                 LayerSlabSupersample2x2AtLeastTwoCandidate})
    {
        if (HostEffectiveProfileBuilder::GeometrySamplingStrategyId(candidate)
            == identifier)
        {
            *strategy = candidate;
            return true;
        }
    }
    return false;
}

bool ParseTiffCompression(
    const QString& identifier,
    HostTiffCompression* compression)
{
    if (compression == nullptr)
    {
        return false;
    }
    if (identifier == QStringLiteral("none"))
    {
        *compression = HostTiffCompression::None;
        return true;
    }
    if (identifier == QStringLiteral("packbits"))
    {
        *compression = HostTiffCompression::PackBits;
        return true;
    }
    return false;
}

bool IsFiniteInRange(
    const double value,
    const double minimum,
    const double maximum)
{
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
}

int HostWorkspaceState::SchemaVersion()
{
    return 7;
}

QString HostWorkspaceState::OrganizationName()
{
    return QStringLiteral("SliceSoft");
}

QString HostWorkspaceState::ApplicationName()
{
    return QStringLiteral("SliceSoftHostReference");
}

bool HostWorkspaceState::PersistenceEnabled()
{
    for (const QString& argument : QCoreApplication::arguments())
    {
        if (argument.contains(
                QStringLiteral("self-test"), Qt::CaseInsensitive))
        {
            return false;
        }
    }
    return true;
}

bool HostWorkspaceState::Save(
    QSettings& settings,
    QMainWindow* window,
    QSplitter* workspaceSplitter,
    QTabWidget* workspaceTabs,
    QTabWidget* inspectorTabs,
    const hostslicesettings& sliceSettings)
{
    if (window == nullptr || workspaceSplitter == nullptr
        || workspaceTabs == nullptr || inspectorTabs == nullptr)
    {
        return false;
    }
    QVariantList splitterSizes;
    for (const int size : workspaceSplitter->sizes())
    {
        splitterSizes.push_back(size);
    }

    settings.beginGroup(kSettingsGroup);
    settings.remove(QString{});
    settings.setValue(QStringLiteral("schema"), kSchemaId);
    settings.setValue(QStringLiteral("schemaVersion"), SchemaVersion());
    settings.setValue(QStringLiteral("geometry"), window->saveGeometry());
    settings.setValue(QStringLiteral("splitterSizes"), splitterSizes);
    settings.setValue(
        QStringLiteral("workspaceTab"), workspaceTabs->currentIndex());
    settings.setValue(
        QStringLiteral("inspectorTab"), inspectorTabs->currentIndex());
    settings.setValue(QStringLiteral("profileId"), sliceSettings.profileid);
    settings.setValue(
        QStringLiteral("processPresetId"),
        sliceSettings.processpresetid);
    settings.setValue(QStringLiteral("dpiX"), sliceSettings.dpix);
    settings.setValue(QStringLiteral("dpiY"), sliceSettings.dpiy);
    settings.setValue(
        QStringLiteral("layerThicknessMm"),
        sliceSettings.layerthicknessmm);
    settings.setValue(
        QStringLiteral("geometrySamplingStrategy"),
        HostEffectiveProfileBuilder::GeometrySamplingStrategyId(
            sliceSettings.geometrysamplingstrategy));
    settings.setValue(
        QStringLiteral("tiffCompression"),
        HostEffectiveProfileBuilder::TiffCompressionId(
            sliceSettings.tiffcompression));
    settings.setValue(
        QStringLiteral("outputDirectory"), sliceSettings.outputdirectory);
    settings.setValue(
        QStringLiteral("materialStrategy"),
        MaterialStrategyId(sliceSettings.materialstrategy));
    settings.setValue(
        QStringLiteral("materialProcess/roleMappingEnabled"),
        sliceSettings.materialprocess.rolemappingenabled);
    settings.setValue(
        QStringLiteral("materialProcess/defaultRole"),
        HostEffectiveProfileBuilder::MaterialRoleId(
            sliceSettings.materialprocess.defaultrole));
    settings.setValue(
        QStringLiteral("materialProcess/mapWhiteNames"),
        sliceSettings.materialprocess.mapwhitenames);
    settings.setValue(
        QStringLiteral("materialProcess/mapVarnishNames"),
        sliceSettings.materialprocess.mapvarnishnames);
    settings.setValue(
        QStringLiteral("materialProcess/allowInputSupportMaterial"),
        sliceSettings.materialprocess.allowinputsupportmaterial);
    settings.setValue(
        QStringLiteral("materialProcess/whiteExpandPx"),
        sliceSettings.materialprocess.whiteexpandpx);
    settings.setValue(
        QStringLiteral("materialProcess/whiteShrinkPx"),
        sliceSettings.materialprocess.whiteshrinkpx);
    settings.setValue(
        QStringLiteral("materialProcess/varnishTopLayers"),
        sliceSettings.materialprocess.varnishtoplayers);
    settings.setValue(
        QStringLiteral("materialProcess/maxUnexpectedOverlapPixels"),
        sliceSettings.materialprocess.maxunexpectedoverlappixels);
    settings.setValue(
        QStringLiteral("buildVolume/widthMm"),
        sliceSettings.buildvolume.widthmm);
    settings.setValue(
        QStringLiteral("buildVolume/heightMm"),
        sliceSettings.buildvolume.heightmm);
    settings.setValue(
        QStringLiteral("buildVolume/zLimitMm"),
        sliceSettings.buildvolume.zlimitmm);
    settings.setValue(
        QStringLiteral("buildVolume/origin"),
        sliceSettings.buildvolume.origin);
    settings.setValue(
        QStringLiteral("buildVolume/xDirection"),
        sliceSettings.buildvolume.xdirection);
    settings.setValue(
        QStringLiteral("buildVolume/yDirection"),
        sliceSettings.buildvolume.ydirection);
    settings.setValue(
        QStringLiteral("support/enabled"), sliceSettings.support.enabled);
    settings.setValue(
        QStringLiteral("support/mode"),
        HostEffectiveProfileBuilder::SupportModeId(
            sliceSettings.support.mode));
    settings.setValue(
        QStringLiteral("support/offsetMm"),
        sliceSettings.support.offsetmm);
    settings.setValue(
        QStringLiteral("support/minAreaPx"),
        sliceSettings.support.minareapx);
    settings.setValue(
        QStringLiteral("support/internalVoid/enabled"),
        sliceSettings.support.internalvoid.enabled);
    settings.setValue(
        QStringLiteral("support/internalVoid/minAreaPx"),
        sliceSettings.support.internalvoid.minareapx);
    settings.setValue(
        QStringLiteral("support/baseProjection/enabled"),
        sliceSettings.support.baseprojection.enabled);
    settings.setValue(
        QStringLiteral("support/baseProjection/layerCount"),
        sliceSettings.support.baseprojection.layercount);
    HostWorkspaceTextureState::Save(settings, sliceSettings.texture);
    HostWorkspaceMatvolState::Save(settings, sliceSettings.materialvolume);
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool HostWorkspaceState::Restore(
    QSettings& settings,
    QMainWindow* window,
    QSplitter* workspaceSplitter,
    QTabWidget* workspaceTabs,
    QTabWidget* inspectorTabs,
    hostworkspacepreferences* preferences)
{
    if (window == nullptr || workspaceSplitter == nullptr
        || workspaceTabs == nullptr || inspectorTabs == nullptr
        || preferences == nullptr)
    {
        return false;
    }
    *preferences = {};
    settings.beginGroup(kSettingsGroup);
    const QString schema = settings.value(
        QStringLiteral("schema")).toString();
    const int version = settings.value(
        QStringLiteral("schemaVersion"), -1).toInt();
    const QByteArray geometry = settings.value(
        QStringLiteral("geometry")).toByteArray();
    const QVariantList splitterValues = settings.value(
        QStringLiteral("splitterSizes")).toList();
    const int workspaceTab = settings.value(
        QStringLiteral("workspaceTab"), -1).toInt();
    const int inspectorTab = settings.value(
        QStringLiteral("inspectorTab"), -1).toInt();
    hostslicesettings restored;
    restored.profileid = settings.value(
        QStringLiteral("profileId")).toString();
    restored.processpresetid = settings.value(
        QStringLiteral("processPresetId")).toString();
    restored.dpix = settings.value(QStringLiteral("dpiX"), -1).toInt();
    restored.dpiy = settings.value(QStringLiteral("dpiY"), -1).toInt();
    restored.layerthicknessmm = settings.value(
        QStringLiteral("layerThicknessMm"), -1.0).toDouble();
    const QString geometrySamplingId = settings.value(
        QStringLiteral("geometrySamplingStrategy")).toString();
    const QString tiffCompressionId = settings.value(
        QStringLiteral("tiffCompression")).toString();
    restored.outputdirectory = settings.value(
        QStringLiteral("outputDirectory")).toString();
    const QString materialId = settings.value(
        QStringLiteral("materialStrategy")).toString();
    restored.materialprocess.rolemappingenabled = settings.value(
        QStringLiteral("materialProcess/roleMappingEnabled")).toBool();
    const QString materialRoleId = settings.value(
        QStringLiteral("materialProcess/defaultRole")).toString();
    restored.materialprocess.mapwhitenames = settings.value(
        QStringLiteral("materialProcess/mapWhiteNames")).toBool();
    restored.materialprocess.mapvarnishnames = settings.value(
        QStringLiteral("materialProcess/mapVarnishNames")).toBool();
    restored.materialprocess.allowinputsupportmaterial = settings.value(
        QStringLiteral("materialProcess/allowInputSupportMaterial")).toBool();
    restored.materialprocess.whiteexpandpx = settings.value(
        QStringLiteral("materialProcess/whiteExpandPx"), -1).toInt();
    restored.materialprocess.whiteshrinkpx = settings.value(
        QStringLiteral("materialProcess/whiteShrinkPx"), -1).toInt();
    restored.materialprocess.varnishtoplayers = settings.value(
        QStringLiteral("materialProcess/varnishTopLayers"), -1).toInt();
    restored.materialprocess.maxunexpectedoverlappixels = settings.value(
        QStringLiteral("materialProcess/maxUnexpectedOverlapPixels"),
        -1).toInt();
    restored.buildvolume.widthmm = settings.value(
        QStringLiteral("buildVolume/widthMm"), -1.0).toDouble();
    restored.buildvolume.heightmm = settings.value(
        QStringLiteral("buildVolume/heightMm"), -1.0).toDouble();
    restored.buildvolume.zlimitmm = settings.value(
        QStringLiteral("buildVolume/zLimitMm"), -1.0).toDouble();
    restored.buildvolume.origin = settings.value(
        QStringLiteral("buildVolume/origin")).toString();
    restored.buildvolume.xdirection = settings.value(
        QStringLiteral("buildVolume/xDirection")).toString();
    restored.buildvolume.ydirection = settings.value(
        QStringLiteral("buildVolume/yDirection")).toString();
    restored.support.enabled = settings.value(
        QStringLiteral("support/enabled")).toBool();
    const QString supportModeId = settings.value(
        QStringLiteral("support/mode")).toString();
    restored.support.offsetmm = settings.value(
        QStringLiteral("support/offsetMm"), -1.0).toDouble();
    restored.support.minareapx = settings.value(
        QStringLiteral("support/minAreaPx"), -1).toInt();
    restored.support.internalvoid.enabled = settings.value(
        QStringLiteral("support/internalVoid/enabled")).toBool();
    restored.support.internalvoid.minareapx = settings.value(
        QStringLiteral("support/internalVoid/minAreaPx"), -1).toInt();
    restored.support.baseprojection.enabled = settings.value(
        QStringLiteral("support/baseProjection/enabled")).toBool();
    restored.support.baseprojection.layercount = settings.value(
        QStringLiteral("support/baseProjection/layerCount"), -1).toInt();
    const bool textureValid = HostWorkspaceTextureState::Restore(
        settings, &restored.texture);
    const bool matvolValid = HostWorkspaceMatvolState::Restore(
        settings, &restored.materialvolume);
    settings.endGroup();

    QList<int> splitterSizes;
    int splitterTotal = 0;
    bool splitterValid = splitterValues.size() == workspaceSplitter->count();
    for (const QVariant& value : splitterValues)
    {
        const int size = value.toInt();
        splitterValid = splitterValid && size >= 0;
        splitterTotal += size;
        splitterSizes.push_back(size);
    }
    splitterValid = splitterValid && splitterTotal > 0;

    HostMaterialStrategy materialStrategy{};
    HostMaterialRole materialRole{};
    HostSupportMode supportMode{};
    HostGeometrySamplingStrategy geometrySamplingStrategy{};
    HostTiffCompression tiffCompression{};
    const bool preferencesValid = !restored.profileid.trimmed().isEmpty()
        && !restored.processpresetid.trimmed().isEmpty()
        && restored.dpix >= 72 && restored.dpix <= 2400
        && restored.dpiy >= 72 && restored.dpiy <= 2400
        && IsFiniteInRange(restored.layerthicknessmm, 0.001, 10.0)
        && ParseGeometrySamplingStrategy(
            geometrySamplingId, &geometrySamplingStrategy)
        && ParseTiffCompression(tiffCompressionId, &tiffCompression)
        && !restored.outputdirectory.trimmed().isEmpty()
        && ParseMaterialStrategy(materialId, &materialStrategy)
        && ParseMaterialRole(materialRoleId, &materialRole)
        && restored.materialprocess.whiteexpandpx >= 0
        && restored.materialprocess.whiteexpandpx <= 100000
        && restored.materialprocess.whiteshrinkpx >= 0
        && restored.materialprocess.whiteshrinkpx <= 100000
        && restored.materialprocess.varnishtoplayers > 0
        && restored.materialprocess.varnishtoplayers <= 100000
        && restored.materialprocess.maxunexpectedoverlappixels >= 0
        && restored.materialprocess.maxunexpectedoverlappixels <= 1000000
        && IsFiniteInRange(restored.buildvolume.widthmm, 1.0, 10000.0)
        && IsFiniteInRange(restored.buildvolume.heightmm, 1.0, 10000.0)
        && IsFiniteInRange(restored.buildvolume.zlimitmm, 1.0, 10000.0)
        && restored.buildvolume.origin == QStringLiteral("lower_left")
        && restored.buildvolume.xdirection == QStringLiteral("positive")
        && restored.buildvolume.ydirection == QStringLiteral("positive")
        && ParseSupportMode(supportModeId, &supportMode)
        && ((restored.support.enabled
             && supportMode != HostSupportMode::None)
            || (!restored.support.enabled
                && supportMode == HostSupportMode::None))
        && IsFiniteInRange(restored.support.offsetmm, 0.0, 10.0)
        && restored.support.minareapx >= 0
        && restored.support.minareapx <= 1000000
        && restored.support.internalvoid.minareapx >= 0
        && restored.support.internalvoid.minareapx <= 1000000
        && restored.support.baseprojection.layercount >= 0
        && restored.support.baseprojection.layercount <= 1000
        && textureValid && matvolValid
        && (restored.support.enabled
            || (!restored.support.internalvoid.enabled
                && !restored.support.baseprojection.enabled));
    const bool layoutValid = schema == kSchemaId
        && version == SchemaVersion() && !geometry.isEmpty()
        && splitterValid && workspaceTab >= 0
        && workspaceTab < workspaceTabs->count() && inspectorTab >= 0
        && inspectorTab < inspectorTabs->count();
    if (!preferencesValid || !layoutValid
        || !window->restoreGeometry(geometry))
    {
        settings.remove(kSettingsGroup);
        Reset(window, workspaceSplitter, workspaceTabs, inspectorTabs);
        return false;
    }

    restored.materialstrategy = materialStrategy;
    restored.materialprocess.defaultrole = materialRole;
    restored.support.mode = supportMode;
    restored.geometrysamplingstrategy = geometrySamplingStrategy;
    restored.tiffcompression = tiffCompression;
    workspaceSplitter->setSizes(splitterSizes);
    workspaceTabs->setCurrentIndex(workspaceTab);
    inspectorTabs->setCurrentIndex(inspectorTab);
    if (!IsOnAvailableScreen(window))
    {
        settings.remove(kSettingsGroup);
        Reset(window, workspaceSplitter, workspaceTabs, inspectorTabs);
        return false;
    }
    preferences->slicesettings = restored;
    return true;
}

void HostWorkspaceState::Reset(
    QMainWindow* window,
    QSplitter* workspaceSplitter,
    QTabWidget* workspaceTabs,
    QTabWidget* inspectorTabs)
{
    if (window == nullptr || workspaceSplitter == nullptr
        || workspaceTabs == nullptr || inspectorTabs == nullptr)
    {
        return;
    }
    window->resize(1080, 720);
    workspaceTabs->setCurrentIndex(0);
    inspectorTabs->setCurrentIndex(0);
    workspaceSplitter->setSizes(QList<int>{760, 320});
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen != nullptr)
    {
        const QRect available = screen->availableGeometry();
        window->move(
            available.center()
            - QPoint(window->width() / 2, window->height() / 2));
    }
}

bool HostWorkspaceState::IsOnAvailableScreen(const QMainWindow* window)
{
    if (window == nullptr)
    {
        return false;
    }
    const QRect frame = window->frameGeometry();
    for (QScreen* screen : QGuiApplication::screens())
    {
        if (screen != nullptr
            && screen->availableGeometry().intersects(frame))
        {
            return true;
        }
    }
    return false;
}
