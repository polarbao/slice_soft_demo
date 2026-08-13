#include "apps/slicer_ui_host_sim/HostWorkspaceState.h"

#include <QApplication>
#include <QDir>
#include <QMainWindow>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace
{
bool Check(const bool condition, const QString& message, QTextStream& errors)
{
    if (!condition)
    {
        errors << message << Qt::endl;
    }
    return condition;
}

struct workspacefixture
{
    QMainWindow window;
    QSplitter* splitter{nullptr};
    QTabWidget* workspacetabs{nullptr};
    QTabWidget* inspectortabs{nullptr};
};

void BuildFixture(workspacefixture* fixture)
{
    auto* central = new QWidget(&fixture->window);
    auto* layout = new QVBoxLayout(central);
    fixture->workspacetabs = new QTabWidget(central);
    fixture->workspacetabs->addTab(new QWidget(), QStringLiteral("工作区"));
    fixture->workspacetabs->addTab(new QWidget(), QStringLiteral("结果"));
    fixture->workspacetabs->addTab(new QWidget(), QStringLiteral("设置"));
    layout->addWidget(fixture->workspacetabs);
    fixture->splitter = new QSplitter(Qt::Horizontal, central);
    fixture->splitter->addWidget(new QWidget());
    auto* inspectorHost = new QWidget();
    auto* inspectorLayout = new QVBoxLayout(inspectorHost);
    fixture->inspectortabs = new QTabWidget(inspectorHost);
    for (const QString& name : {
             QStringLiteral("模型"), QStringLiteral("Profile"),
             QStringLiteral("切片设置"), QStringLiteral("切片作业")})
    {
        fixture->inspectortabs->addTab(new QWidget(), name);
    }
    inspectorLayout->addWidget(fixture->inspectortabs);
    fixture->splitter->addWidget(inspectorHost);
    layout->addWidget(fixture->splitter);
    fixture->window.setCentralWidget(central);
    fixture->window.resize(960, 640);
    fixture->window.move(40, 40);
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QTextStream errors(stderr);
    QTemporaryDir temporaryRoot;
    if (!Check(temporaryRoot.isValid(),
               QStringLiteral("H-B-08 临时设置目录不可用。"), errors))
    {
        return 2;
    }
    const QString settingsPath = QDir(temporaryRoot.path()).filePath(
        QStringLiteral("host-workspace.ini"));

    workspacefixture source;
    BuildFixture(&source);
    source.window.show();
    application.processEvents();
    source.workspacetabs->setCurrentIndex(2);
    source.inspectortabs->setCurrentIndex(3);
    source.splitter->setSizes(QList<int>{620, 280});
    hostslicesettings expected;
    expected.profileid = QStringLiteral("host-reference-white");
    expected.outputdirectory = temporaryRoot.path();
    expected.dpix = 720;
    expected.dpiy = 600;
    expected.layerthicknessmm = 0.05;
    expected.geometrysamplingstrategy = HostGeometrySamplingStrategy::
        LayerSlabSupersample2x2AtLeastTwoCandidate;
    expected.materialstrategy = HostMaterialStrategy::RgbWhiteVarnish;
    expected.materialprocess.rolemappingenabled = true;
    expected.materialprocess.defaultrole = HostMaterialRole::Ignore;
    expected.materialprocess.mapwhitenames = true;
    expected.materialprocess.mapvarnishnames = false;
    expected.materialprocess.allowinputsupportmaterial = true;
    expected.materialprocess.whiteexpandpx = 2;
    expected.materialprocess.whiteshrinkpx = 1;
    expected.materialprocess.varnishtoplayers = 3;
    expected.materialprocess.maxunexpectedoverlappixels = 4;
    expected.buildvolume.widthmm = 260.0;
    expected.buildvolume.heightmm = 120.0;
    expected.buildvolume.zlimitmm = 80.0;
    expected.support.mode = HostSupportMode::BottomProjectionPlusUnsupported;
    expected.support.offsetmm = 0.3;
    expected.support.minareapx = 24;
    expected.support.internalvoid.enabled = true;
    expected.support.internalvoid.minareapx = 32;
    expected.support.baseprojection.enabled = true;
    expected.support.baseprojection.layercount = 40;
    expected.texture.enabled = true;
    expected.texture.applymode = HostTextureApplyMode::TopSurfaceBand;
    expected.texture.topsurfacelayers = 7;
    expected.texture.sampler = HostTextureSampler::Nearest;
    expected.texture.uvaddressmode = HostTextureUvAddressMode::Repeat;
    expected.texture.flipv = false;
    expected.texture.fallbackred = 10;
    expected.texture.fallbackgreen = 20;
    expected.texture.fallbackblue = 30;
    expected.texture.missingpolicy = HostTextureMissingPolicy::FailFast;
    expected.texture.nonsurfacepolicy = HostTextureNonSurfacePolicy::Empty;
    expected.texture.whitepolicy = HostTextureWhitePolicy::FailClosed;
    expected.texture.whiteinkthreshold = 5;
    expected.texture.whitevalue = 6;
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        HostWorkspaceState::Save(
            settings,
            &source.window,
            source.splitter,
            source.workspacetabs,
            source.inspectortabs,
            expected);
    }

    workspacefixture restored;
    BuildFixture(&restored);
    hostworkspacepreferences preferences;
    bool loaded = false;
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        loaded = HostWorkspaceState::Restore(
            settings,
            &restored.window,
            restored.splitter,
            restored.workspacetabs,
            restored.inspectortabs,
            &preferences);
    }
    restored.window.show();
    application.processEvents();
    const hostslicesettings& actual = preferences.slicesettings;
    if (!Check(loaded, QStringLiteral("合法工作区状态恢复失败。"), errors)
        || !Check(restored.workspacetabs->currentIndex() == 2
                      && restored.inspectortabs->currentIndex() == 3,
                  QStringLiteral("工作区或业务页索引未恢复。"), errors)
        || !Check(restored.splitter->sizes().size() == 2
                      && restored.splitter->sizes().at(0) > 0
                      && restored.splitter->sizes().at(1) > 0,
                  QStringLiteral("工作区分栏尺寸未恢复。"), errors)
        || !Check(actual.profileid == expected.profileid
                      && actual.dpix == expected.dpix
                      && actual.dpiy == expected.dpiy
                      && std::abs(actual.layerthicknessmm
                                   - expected.layerthicknessmm) < 1.0e-9
                      && actual.geometrysamplingstrategy
                          == expected.geometrysamplingstrategy
                      && actual.outputdirectory == expected.outputdirectory
                      && actual.materialstrategy
                          == HostMaterialStrategy::RgbWhiteVarnish,
                  QStringLiteral("切片操作偏好未完整恢复。"), errors)
        || !Check(
            actual.materialprocess.rolemappingenabled
                && actual.materialprocess.defaultrole
                    == HostMaterialRole::Ignore
                && actual.materialprocess.mapwhitenames
                && !actual.materialprocess.mapvarnishnames
                && actual.materialprocess.allowinputsupportmaterial
                && actual.materialprocess.whiteexpandpx == 2
                && actual.materialprocess.whiteshrinkpx == 1
                && actual.materialprocess.varnishtoplayers == 3
                && actual.materialprocess.maxunexpectedoverlappixels == 4,
            QStringLiteral("宿主材料工艺 Profile 草稿未完整恢复。"),
            errors)
        || !Check(actual.modelpath.isEmpty() && actual.modelformat.isEmpty(),
                  QStringLiteral("运行时模型身份不得进入持久化状态。"), errors)
        || !Check(
            HostEffectiveProfileBuilder::BuildVolumesEqual(
                actual.buildvolume, expected.buildvolume),
            QStringLiteral("设备 buildVolume 未完整恢复。"), errors)
        || !Check(
            actual.support.enabled
                && actual.support.mode
                    == HostSupportMode::BottomProjectionPlusUnsupported
                && std::abs(actual.support.offsetmm - 0.3) < 1.0e-9
                && actual.support.minareapx == 24
                && actual.support.internalvoid.enabled
                && actual.support.internalvoid.minareapx == 32
                && actual.support.baseprojection.enabled
                && actual.support.baseprojection.layercount == 40,
            QStringLiteral("宿主支撑 Profile 草稿未完整恢复。"), errors)
        || !Check(
            actual.texture.enabled
                && actual.texture.applymode
                    == HostTextureApplyMode::TopSurfaceBand
                && actual.texture.topsurfacelayers == 7
                && actual.texture.sampler == HostTextureSampler::Nearest
                && actual.texture.uvaddressmode
                    == HostTextureUvAddressMode::Repeat
                && !actual.texture.flipv
                && actual.texture.fallbackred == 10
                && actual.texture.fallbackgreen == 20
                && actual.texture.fallbackblue == 30
                && actual.texture.missingpolicy
                    == HostTextureMissingPolicy::FailFast
                && actual.texture.nonsurfacepolicy
                    == HostTextureNonSurfacePolicy::Empty
                && actual.texture.whitepolicy
                    == HostTextureWhitePolicy::FailClosed
                && actual.texture.whiteinkthreshold == 5
                && actual.texture.whitevalue == 6,
            QStringLiteral("宿主生产纹理 Profile 草稿未完整恢复。"),
            errors))
    {
        return 3;
    }

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("hostflow/workspace"));
        settings.setValue(QStringLiteral("schemaVersion"), 99);
        settings.endGroup();
        settings.sync();
    }
    workspacefixture invalid;
    BuildFixture(&invalid);
    invalid.workspacetabs->setCurrentIndex(2);
    invalid.inspectortabs->setCurrentIndex(2);
    hostworkspacepreferences ignored;
    bool invalidLoaded = true;
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        invalidLoaded = HostWorkspaceState::Restore(
            settings,
            &invalid.window,
            invalid.splitter,
            invalid.workspacetabs,
            invalid.inspectortabs,
            &ignored);
    }
    if (!Check(!invalidLoaded,
               QStringLiteral("旧 schema 必须 fail-safe。"), errors)
        || !Check(invalid.workspacetabs->currentIndex() == 0
                      && invalid.inspectortabs->currentIndex() == 0,
                  QStringLiteral("非法状态未回退安全默认页。"), errors))
    {
        return 4;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HE05_PERSISTENCE_PASS schema="
        << HostWorkspaceState::SchemaVersion()
        << " runtimeHandles=persisted:false" << Qt::endl;
    return 0;
}
