#include "PackageLoader.h"
#include "ProductionPackageResultValidator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool WriteJson(const QString& path, const QJsonObject& object)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    return file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) > 0;
}

struct packagefixture
{
    QTemporaryDir root;
    QString configpath;
    QString packagedir;
};

bool CreatePackage(
    packagefixture* fixture,
    const QString& requestedMode,
    const QString& effectiveMode,
    const bool fallbackApplied)
{
    if (!fixture->root.isValid())
    {
        return false;
    }
    fixture->configpath =
        QDir(fixture->root.path()).filePath(QStringLiteral("effective.json"));
    fixture->packagedir =
        QDir(fixture->root.path()).filePath(QStringLiteral("package"));
    QDir().mkpath(
        QDir(fixture->packagedir).filePath(QStringLiteral("reports")));
    QDir().mkpath(
        QDir(fixture->packagedir).filePath(QStringLiteral("preview")));
    if (!WriteJson(fixture->configpath, QJsonObject{}))
    {
        return false;
    }

    const QJsonObject identity{
        {QStringLiteral("requestedPipelineMode"), requestedMode},
        {QStringLiteral("effectivePipelineMode"), effectiveMode},
        {QStringLiteral("productionOutputWritten"), true},
        {QStringLiteral("fallbackApplied"), fallbackApplied},
    };
    QJsonObject manifest = identity;
    manifest.insert(
        QStringLiteral("schema"),
        QStringLiteral("p0.rgbwsv.2"));
    manifest.insert(
        QStringLiteral("source"),
        QJsonObject{
            {QStringLiteral("configPath"), fixture->configpath},
            {QStringLiteral("engine"), effectiveMode}});
    manifest.insert(
        QStringLiteral("reports"),
        QJsonObject{
            {QStringLiteral("slice"), QStringLiteral("reports/slice_report.json")},
            {QStringLiteral("preview"), QStringLiteral("reports/preview_report.json")}});
    const QJsonArray previewFiles{
        QJsonObject{
            {QStringLiteral("channel"), QStringLiteral("RGB")},
            {QStringLiteral("layerIndex"), 0},
            {QStringLiteral("path"), QStringLiteral("preview/rgb_000000.png")}}};
    manifest.insert(
        QStringLiteral("preview"),
        QJsonObject{{QStringLiteral("files"), previewFiles}});

    QFile preview(
        QDir(fixture->packagedir)
            .filePath(QStringLiteral("preview/rgb_000000.png")));
    return WriteJson(
               QDir(fixture->packagedir)
                   .filePath(QStringLiteral("manifest.json")),
               manifest)
        && WriteJson(
            QDir(fixture->packagedir)
                .filePath(QStringLiteral("reports/slice_report.json")),
            identity)
        && WriteJson(
            QDir(fixture->packagedir)
                .filePath(QStringLiteral("reports/preview_report.json")),
            QJsonObject{
                {QStringLiteral("schema"), QStringLiteral("p0.preview_report.1")},
                {QStringLiteral("files"), previewFiles}})
        && preview.open(QIODevice::WriteOnly)
        && preview.write("fixture") > 0;
}

ProductionPackageResult Validate(
    const packagefixture& fixture,
    const slicer_core::SlicePipelineMode mode)
{
    ProductionPackageResultRequest request;
    request.runrequest.mode = mode;
    request.runrequest.profileid =
        mode == slicer_core::SlicePipelineMode::GlobalSurfaceShell
        ? QStringLiteral("global_surface_shell_material_parity_candidate")
        : QString{};
    request.runrequest.sessionid = QStringLiteral("session-09b-05");
    request.runrequest.configpath = fixture.configpath;
    request.runrequest.packagedir = fixture.packagedir;
    request.package = PackageLoader().load(fixture.packagedir);
    request.measuredtotalms = 1234.5;
    request.measuredpeakworkingsetbytes = 64U * 1024U * 1024U;
    return ProductionPackageResultValidator().Validate(request);
}

bool TestValidCurrentSessionPackage()
{
    packagefixture fixture;
    if (!CreatePackage(
            &fixture,
            QStringLiteral("global_surface_shell"),
            QStringLiteral("global_surface_shell"),
            false))
    {
        return ExpectTrue(false, "valid fixture created");
    }
    const ProductionPackageResult result = Validate(
        fixture,
        slicer_core::SlicePipelineMode::GlobalSurfaceShell);
    return ExpectTrue(result.valid, "current session package accepted")
        && ExpectTrue(
            result.presentation.effectivemode
                == slicer_core::SlicePipelineMode::GlobalSurfaceShell,
            "effective mode preserved")
        && ExpectTrue(
            result.presentation.productionoutputwritten,
            "production output shown")
        && ExpectTrue(
            !result.presentation.fallbackapplied,
            "fallback remains false")
        && ExpectTrue(
            result.presentation.measuredtotalms.value_or(0.0) == 1234.5,
            "actual timing preserved")
        && ExpectTrue(
            result.presentation.measuredpeakworkingsetbytes.value_or(0U)
                == 64U * 1024U * 1024U,
            "actual memory preserved");
}

bool TestModeMismatchAndFallbackBlocked()
{
    packagefixture modeFixture;
    packagefixture fallbackFixture;
    if (!CreatePackage(
            &modeFixture,
            QStringLiteral("legacy"),
            QStringLiteral("legacy"),
            false)
        || !CreatePackage(
            &fallbackFixture,
            QStringLiteral("global_surface_shell"),
            QStringLiteral("global_surface_shell"),
            true))
    {
        return ExpectTrue(false, "negative fixtures created");
    }
    const ProductionPackageResult modeResult = Validate(
        modeFixture,
        slicer_core::SlicePipelineMode::GlobalSurfaceShell);
    const ProductionPackageResult fallbackResult = Validate(
        fallbackFixture,
        slicer_core::SlicePipelineMode::GlobalSurfaceShell);
    return ExpectTrue(!modeResult.valid, "mode mismatch blocked")
        && ExpectTrue(!fallbackResult.valid, "fallback package blocked")
        && ExpectTrue(
            fallbackResult.presentation.fallbackapplied,
            "actual fallback state exposed")
        && ExpectTrue(
            fallbackResult.presentation.admissionstate
                == ProductionAdmissionState::Blocked,
            "invalid package presentation is blocked");
}

bool TestPreviewAndReportMustSharePackage()
{
    packagefixture fixture;
    if (!CreatePackage(
            &fixture,
            QStringLiteral("legacy"),
            QStringLiteral("legacy"),
            false))
    {
        return ExpectTrue(false, "same-source fixture created");
    }
    ProductionPackageResultRequest request;
    request.runrequest.mode = slicer_core::SlicePipelineMode::Legacy;
    request.runrequest.sessionid = QStringLiteral("legacy-session");
    request.runrequest.configpath = fixture.configpath;
    request.runrequest.packagedir = fixture.packagedir;
    request.package = PackageLoader().load(fixture.packagedir);
    request.package.report_paths.push_back(
        QDir(fixture.root.path()).filePath(QStringLiteral("foreign.json")));
    const ProductionPackageResult result =
        ProductionPackageResultValidator().Validate(request);
    return ExpectTrue(!result.valid, "foreign report path blocked")
        && ExpectTrue(
            result.presentation.blockingcode
                == "production_package_identity_mismatch",
            "same-source blocking code preserved");
}

bool TestCompletionPreservesExactRequest()
{
    ProductionSliceRunSession session;
    ProductionSliceRunRequest request;
    request.mode = slicer_core::SlicePipelineMode::Legacy;
    request.sessionid = QStringLiteral("identity-session");
    request.configpath = QStringLiteral("identity-config.json");
    request.packagedir = QStringLiteral("identity-package");
    if (!session.Begin(request).isEmpty())
    {
        return ExpectTrue(false, "identity session begins");
    }
    const ProductionSliceRunCompletion completion = session.Complete(0);
    return ExpectTrue(completion.request.has_value(), "completion keeps request")
        && ExpectTrue(
            completion.request->sessionid == request.sessionid,
            "completion session identity")
        && ExpectTrue(
            completion.request->configpath == request.configpath,
            "completion config identity")
        && ExpectTrue(
            completion.request->packagedir == request.packagedir,
            "completion package identity");
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    const bool passed = TestValidCurrentSessionPackage()
        && TestModeMismatchAndFallbackBlocked()
        && TestPreviewAndReportMustSharePackage()
        && TestCompletionPreservesExactRequest();
    if (!passed)
    {
        return 1;
    }
    std::cout
        << "PASS production_package_result_unit_tests "
           "identity/mode/output/no-fallback/same-source/resources\n";
    return 0;
}
