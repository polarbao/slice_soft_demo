#include "ProductionPackageResultValidator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{

QString NormalizedPath(const QString& path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return QDir::cleanPath(
        canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath);
}

bool PathsEqual(const QString& left, const QString& right)
{
#ifdef Q_OS_WIN
    return NormalizedPath(left).compare(NormalizedPath(right), Qt::CaseInsensitive) == 0;
#else
    return NormalizedPath(left) == NormalizedPath(right);
#endif
}

bool IsWithinPackage(const QString& packageDir, const QString& path)
{
    const QString relativePath =
        QDir(NormalizedPath(packageDir)).relativeFilePath(NormalizedPath(path));
    return relativePath != QStringLiteral("..")
        && !relativePath.startsWith(QStringLiteral("../"))
        && !QDir::isAbsolutePath(relativePath);
}

QString ModeValue(const slicer_core::SlicePipelineMode mode)
{
    const ProductionModeCapability* capability =
        ProductionModeCatalog::FindMode(mode);
    return capability == nullptr
        ? QString{}
        : QString::fromStdString(capability->stablevalue);
}

std::optional<slicer_core::SlicePipelineMode> ParseMode(const QString& value)
{
    for (const ProductionModeCapability& capability : ProductionModeCatalog::Modes())
    {
        if (QString::fromStdString(capability.stablevalue) == value)
        {
            return capability.mode;
        }
    }
    return std::nullopt;
}

QJsonObject ReadObject(
    const QString& path,
    const QString& description,
    QStringList* errors)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        errors->push_back(
            QStringLiteral("无法读取%1：%2").arg(description, path));
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        errors->push_back(
            QStringLiteral("%1不是有效 JSON 对象：%2")
                .arg(description, parseError.errorString()));
        return {};
    }
    return document.object();
}

bool ReadRequiredBool(
    const QJsonObject& object,
    const QString& key,
    const bool expected,
    QStringList* errors)
{
    const QJsonValue value = object.value(key);
    if (!value.isBool() || value.toBool() != expected)
    {
        errors->push_back(
            QStringLiteral("生产包字段 %1 必须为 %2。")
                .arg(key, expected ? QStringLiteral("true") : QStringLiteral("false")));
        return false;
    }
    return true;
}

void ValidateChildPaths(
    const QString& packageDir,
    const QStringList& paths,
    const QString& description,
    QStringList* errors)
{
    for (const QString& path : paths)
    {
        if (!IsWithinPackage(packageDir, path))
        {
            errors->push_back(
                QStringLiteral("%1不属于当前 session package：%2")
                    .arg(description, path));
        }
    }
}

void ValidatePreviewEntries(
    const QString& packageDir,
    const QJsonArray& entries,
    QStringList* errors)
{
    for (const QJsonValue& entry : entries)
    {
        const QString relativePath = entry.toObject()
            .value(QStringLiteral("path"))
            .toString();
        const QString absolutePath =
            QDir(packageDir).filePath(relativePath);
        if (relativePath.isEmpty()
            || !IsWithinPackage(packageDir, absolutePath)
            || !QFileInfo::exists(absolutePath))
        {
            errors->push_back(
                QStringLiteral("预览清单包含无效或跨 package 路径：%1")
                    .arg(relativePath));
        }
    }
}

}  // namespace

ProductionPackageResult ProductionPackageResultValidator::Validate(
    const ProductionPackageResultRequest& request) const
{
    ProductionPackageResult result;
    result.presentation.requestedmode = request.runrequest.mode;
    result.presentation.requestedprofileid =
        request.runrequest.profileid.toStdString();
    result.presentation.admissionstate = ProductionAdmissionState::Admitted;
    result.presentation.fallbackapplied = false;
    result.presentation.measuredtotalms = request.measuredtotalms;
    result.presentation.measuredpeakworkingsetbytes =
        request.measuredpeakworkingsetbytes;
    result.presentation.sessionid = request.runrequest.sessionid.toStdString();
    result.presentation.configpath = request.runrequest.configpath.toStdString();
    result.presentation.packagepath = request.runrequest.packagedir.toStdString();

    const ProductionModeCapability* capability =
        ProductionModeCatalog::FindMode(request.runrequest.mode);
    result.presentation.resourcecost =
        capability == nullptr
        ? ProductionResourceCostLevel::NotEvaluated
        : capability->resourcecost;

    if (!PathsEqual(
            request.runrequest.packagedir,
            request.package.package_dir))
    {
        result.errors.push_back(
            QStringLiteral("当前加载包与本次生产 session 的输出包身份不一致。"));
    }
    if (request.package.manifest_path.isEmpty()
        || !IsWithinPackage(
            request.package.package_dir,
            request.package.manifest_path))
    {
        result.errors.push_back(
            QStringLiteral("manifest 不属于当前 session package。"));
    }

    ValidateChildPaths(
        request.package.package_dir,
        request.package.report_paths,
        QStringLiteral("报告文件"),
        &result.errors);
    ValidateChildPaths(
        request.package.package_dir,
        request.package.preview_paths,
        QStringLiteral("预览文件"),
        &result.errors);

    const QString sliceReportPath =
        QDir(request.package.package_dir)
            .filePath(QStringLiteral("reports/slice_report.json"));
    const QString previewReportPath =
        QDir(request.package.package_dir)
            .filePath(QStringLiteral("reports/preview_report.json"));
    if (!QFileInfo::exists(sliceReportPath))
    {
        result.errors.push_back(
            QStringLiteral("当前 session package 缺少 slice_report.json。"));
    }
    if (!QFileInfo::exists(previewReportPath))
    {
        result.errors.push_back(
            QStringLiteral("当前 session package 缺少 preview_report.json。"));
    }

    if (!result.errors.isEmpty())
    {
        result.presentation.admissionstate = ProductionAdmissionState::Blocked;
        result.presentation.blockingcode = "production_package_identity_mismatch";
        result.presentation.blockingmessage =
            result.errors.join(QStringLiteral(" ")).toStdString();
        return result;
    }

    const QJsonObject manifest = ReadObject(
        request.package.manifest_path,
        QStringLiteral("manifest"),
        &result.errors);
    const QJsonObject sliceReport = ReadObject(
        sliceReportPath,
        QStringLiteral("slice report"),
        &result.errors);
    const QJsonObject previewReport = ReadObject(
        previewReportPath,
        QStringLiteral("preview report"),
        &result.errors);
    QJsonObject sceneReport;
    if (!request.expectedsceneid.isEmpty())
    {
        const QString sceneReportPath =
            QDir(request.package.package_dir)
                .filePath(
                    QStringLiteral(
                        "reports/multimodel_scene_report.json"));
        sceneReport = ReadObject(
            sceneReportPath,
            QStringLiteral("multi-model scene report"),
            &result.errors);
    }
    if (!result.errors.isEmpty())
    {
        result.presentation.admissionstate = ProductionAdmissionState::Blocked;
        result.presentation.blockingcode = "production_package_json_invalid";
        result.presentation.blockingmessage =
            result.errors.join(QStringLiteral(" ")).toStdString();
        return result;
    }

    const QString expectedMode = ModeValue(request.runrequest.mode);
    const QString requestedMode =
        manifest.value(QStringLiteral("requestedPipelineMode")).toString();
    const QString effectiveMode =
        manifest.value(QStringLiteral("effectivePipelineMode")).toString();
    result.presentation.effectivemode = ParseMode(effectiveMode);
    result.presentation.productionoutputwritten =
        manifest.value(QStringLiteral("productionOutputWritten"))
            .toBool(false);
    result.presentation.fallbackapplied =
        manifest.value(QStringLiteral("fallbackApplied")).toBool(false);
    if (manifest.value(QStringLiteral("schema")).toString()
        != QStringLiteral("p0.rgbwsv.2"))
    {
        result.errors.push_back(
            QStringLiteral("生产包 schema 必须为 p0.rgbwsv.2。"));
    }
    const QJsonObject reports =
        manifest.value(QStringLiteral("reports")).toObject();
    if (reports.value(QStringLiteral("slice")).toString()
            != QStringLiteral("reports/slice_report.json")
        || reports.value(QStringLiteral("preview")).toString()
            != QStringLiteral("reports/preview_report.json"))
    {
        result.errors.push_back(
            QStringLiteral("manifest 的 slice/preview 报告引用不属于固定当前包路径。"));
    }
    if (previewReport.value(QStringLiteral("schema")).toString()
        != QStringLiteral("p0.preview_report.1"))
    {
        result.errors.push_back(
            QStringLiteral("preview report schema 必须为 p0.preview_report.1。"));
    }
    ValidatePreviewEntries(
        request.package.package_dir,
        previewReport.value(QStringLiteral("files")).toArray(),
        &result.errors);
    ValidatePreviewEntries(
        request.package.package_dir,
        manifest.value(QStringLiteral("preview"))
            .toObject()
            .value(QStringLiteral("files"))
            .toArray(),
        &result.errors);
    if (requestedMode != expectedMode || effectiveMode != expectedMode)
    {
        result.errors.push_back(
            QStringLiteral(
                "生产包 requested/effective 模式与本次请求不一致：期望 %1，实际 %2/%3。")
                .arg(expectedMode, requestedMode, effectiveMode));
    }
    if (sliceReport.value(QStringLiteral("requestedPipelineMode")).toString()
            != expectedMode
        || sliceReport.value(QStringLiteral("effectivePipelineMode")).toString()
            != expectedMode)
    {
        result.errors.push_back(
            QStringLiteral("slice report 与 manifest 的生产模式不同源。"));
    }
    ReadRequiredBool(
        manifest,
        QStringLiteral("productionOutputWritten"),
        true,
        &result.errors);
    ReadRequiredBool(
        manifest,
        QStringLiteral("fallbackApplied"),
        false,
        &result.errors);
    ReadRequiredBool(
        sliceReport,
        QStringLiteral("productionOutputWritten"),
        true,
        &result.errors);
    ReadRequiredBool(
        sliceReport,
        QStringLiteral("fallbackApplied"),
        false,
        &result.errors);

    const QString manifestConfigPath =
        manifest.value(QStringLiteral("source"))
            .toObject()
            .value(QStringLiteral("configPath"))
            .toString();
    if (manifestConfigPath.isEmpty()
        || !PathsEqual(manifestConfigPath, request.runrequest.configpath))
    {
        result.errors.push_back(
            QStringLiteral("manifest source.configPath 与本次生效配置不同源。"));
    }

    if (!request.expectedsceneid.isEmpty())
    {
        const QJsonObject manifestScene =
            manifest.value(QStringLiteral("scene")).toObject();
        const QString manifestSceneId =
            manifestScene.value(QStringLiteral("sceneId")).toString();
        const std::uint64_t manifestSceneRevision =
            static_cast<std::uint64_t>(
                manifestScene
                    .value(QStringLiteral("sceneRevision"))
                    .toDouble());
        const QString manifestSceneHash =
            manifestScene.value(QStringLiteral("sceneHash")).toString();
        const std::uint64_t reportSceneRevision =
            static_cast<std::uint64_t>(
                sceneReport
                    .value(QStringLiteral("sceneRevision"))
                    .toDouble());
        if (manifestSceneId != request.expectedsceneid
            || sceneReport.value(QStringLiteral("sceneId")).toString()
                != request.expectedsceneid
            || (request.expectedscenerevision.has_value()
                && (manifestSceneRevision
                        != *request.expectedscenerevision
                    || reportSceneRevision
                        != *request.expectedscenerevision))
            || (!request.expectedscenehash.isEmpty()
                && (manifestSceneHash
                        != request.expectedscenehash
                    || sceneReport
                            .value(QStringLiteral("sceneHash"))
                            .toString()
                        != request.expectedscenehash)))
        {
            result.errors.push_back(
                QStringLiteral(
                    "manifest、scene report 与冻结场景身份不一致。"));
        }
    }

    if (!result.errors.isEmpty())
    {
        result.presentation.admissionstate = ProductionAdmissionState::Blocked;
        result.presentation.blockingcode = "production_package_contract_mismatch";
        result.presentation.blockingmessage =
            result.errors.join(QStringLiteral(" ")).toStdString();
        return result;
    }

    result.valid = true;
    result.presentation.effectiveprofileid =
        request.runrequest.profileid.isEmpty()
        ? std::optional<std::string>{}
        : std::optional<std::string>{
              request.runrequest.profileid.toStdString()};
    return result;
}
