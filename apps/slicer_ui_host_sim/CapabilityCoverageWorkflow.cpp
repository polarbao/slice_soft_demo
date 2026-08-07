#include "CapabilityCoverageRunner.h"

#include "CapabilityCoverageRequests.h"
#include "render/TopViewRenderPolicy.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace
{
constexpr int kSyncTimeoutMs{30000};
constexpr int kWorkerTimeoutMs{120000};

QString NewIdentity(const QString& prefix)
{
    return QStringLiteral("%1-%2-%3")
        .arg(prefix)
        .arg(QCoreApplication::applicationPid())
        .arg(QDateTime::currentMSecsSinceEpoch());
}

QString EnsureSha256Prefix(const QString& value)
{
    return value.startsWith(QStringLiteral("sha256:"))
        ? value
        : QStringLiteral("sha256:%1").arg(value);
}

bool IsSuccessful(const QJsonObject& value)
{
    return value.value(QStringLiteral("ok")).toBool();
}

QString ResultSummary(const QJsonObject& value)
{
    return QString::fromUtf8(
        QJsonDocument(value).toJson(QJsonDocument::Compact));
}
}

bool CapabilityCoverageRunner::Run(
    const QString& repositoryRoot,
    const QString& evidenceRoot,
    QByteArray* reportBytes,
    QString* error)
{
    m_entries = {};
    const QString runRoot = QDir(evidenceRoot).filePath(NewIdentity(
        QStringLiteral("run")));
    capabilitycoveragefixture fixture;
    if (!m_client.IsOpen()
        || !CapabilityCoverageRequests::InitializePaths(
            repositoryRoot, runRoot, &fixture, error))
    {
        return false;
    }

    auto invoke = [this, error](
                      const QString& tier,
                      const QString& capability,
                      const QString& carrier,
                      const QString& requirement,
                      const QJsonObject& request,
                      const int timeoutMs,
                      const bool mandatory,
                      invocationresult* output)
    {
        invocationresult current;
        QString invocationError;
        const bool completed = ExecuteJob(
            request, timeoutMs, &current, &invocationError);
        const bool succeeded = completed && current.terminal
            && current.terminalstate == QStringLiteral("succeeded")
            && IsSuccessful(current.payload);
        const bool recorded = completed && current.terminal
            && !current.code.isEmpty();
        const bool satisfied = mandatory ? succeeded : recorded;
        Record(tier, capability, carrier, requirement, current, satisfied);
        if (output != nullptr)
        {
            *output = current;
        }
        if (mandatory && !satisfied && error != nullptr)
        {
            *error = QStringLiteral("%1 能力门禁失败：%2 %3")
                         .arg(capability, invocationError,
                              ResultSummary(current.payload));
        }
        return !mandatory || satisfied;
    };

    invocationresult imported;
    const QJsonObject importRequest{
        {QStringLiteral("capability"), QStringLiteral("model.import")},
        {QStringLiteral("modelPath"), fixture.modelpath},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("computeBBox"), true},
             {QStringLiteral("extractMaterials"), false}}}};
    if (!invoke(QStringLiteral("P0"), QStringLiteral("model.import"),
            QStringLiteral("DLL"), QStringLiteral("end_to_end"),
            importRequest, kSyncTimeoutMs, true, &imported)
        || !CapabilityCoverageRequests::BindImportedModel(
            imported.payload, &fixture, error))
    {
        return false;
    }

    invocationresult ignored;
    (void)invoke(QStringLiteral("P2"), QStringLiteral("model.get_metadata"),
        QStringLiteral("DLL"), QStringLiteral("return_recorded"),
        QJsonObject{{QStringLiteral("capability"),
                     QStringLiteral("model.get_metadata")},
                    {QStringLiteral("modelId"), fixture.modelid}},
        kSyncTimeoutMs, false, &ignored);

    const QJsonObject transformRequest{
        {QStringLiteral("capability"),
         QStringLiteral("scene.apply_operation")},
        {QStringLiteral("operationId"), NewIdentity(
             QStringLiteral("operation-stage14e04b"))},
        {QStringLiteral("scene"), fixture.initialscene},
        {QStringLiteral("currentSceneRevision"), 0},
        {QStringLiteral("expectedSceneRevision"), 0},
        {QStringLiteral("operations"), QJsonArray{QJsonObject{
             {QStringLiteral("type"), QStringLiteral("translate")},
             {QStringLiteral("instanceId"),
              QStringLiteral("instance-stage14e01")},
             {QStringLiteral("deltaMm"), QJsonArray{1.0, 2.0, 0.0}}}}}};
    invocationresult committed;
    if (!invoke(QStringLiteral("P0"),
            QStringLiteral("scene.apply_operation"), QStringLiteral("DLL"),
            QStringLiteral("end_to_end"), transformRequest,
            kSyncTimeoutMs, true, &committed))
    {
        return false;
    }
    fixture.scenehash = EnsureSha256Prefix(committed.payload.value(
        QStringLiteral("sceneHash")).toString());
    fixture.scenerevision = static_cast<quint64>(committed.payload.value(
        QStringLiteral("newSceneRevision")).toDouble());

    invocationresult snapshot;
    if (!invoke(QStringLiteral("P0"),
            QStringLiteral("scene.get_snapshot"), QStringLiteral("DLL"),
            QStringLiteral("end_to_end"),
            QJsonObject{{QStringLiteral("capability"),
                         QStringLiteral("scene.get_snapshot")},
                        {QStringLiteral("sceneId"),
                         QStringLiteral("scene-stage14e01")}},
            kSyncTimeoutMs, true, &snapshot))
    {
        return false;
    }
    bool handleOk = false;
    fixture.scenehandle = snapshot.payload.value(QStringLiteral("scene"))
                              .toObject()
                              .value(QStringLiteral("sceneId"))
                              .toString()
                              .toULongLong(&handleOk);
    if (!handleOk || fixture.scenehandle == 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("scene.get_snapshot 未返回内部场景句柄。");
        }
        return false;
    }

    TopViewRenderPolicy topView(m_client);
    TopViewFrame frame;
    QString viewError;
    invocationresult viewResult;
    viewResult.transportok = topView.Refresh(
        fixture.scenehandle, fixture.scenerevision, &frame, &viewError);
    viewResult.terminal = true;
    viewResult.terminalstate = viewResult.transportok
        ? QStringLiteral("succeeded") : QStringLiteral("failed");
    viewResult.code = viewResult.transportok
        ? QStringLiteral("PM-SLICER-OK")
        : QStringLiteral("PM-SLICER-VIEWDATA-FAILED");
    viewResult.payload = QJsonObject{
        {QStringLiteral("ok"), viewResult.transportok},
        {QStringLiteral("instanceCount"), frame.instances.size()},
        {QStringLiteral("blobReadCount"),
         static_cast<qint64>(topView.BlobReadCount())},
        {QStringLiteral("detail"), viewError}};
    const bool viewPassed = viewResult.transportok
        && !frame.instances.isEmpty() && topView.BlobReadCount() > 0;
    Record(QStringLiteral("P1"), QStringLiteral("scene.get_viewdata"),
        QStringLiteral("DLL+blob"), QStringLiteral("end_to_end"),
        viewResult, viewPassed);
    if (!viewPassed)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("scene.get_viewdata 门禁失败：%1")
                         .arg(viewError);
        }
        return false;
    }

    if (!invoke(QStringLiteral("P1"),
            QStringLiteral("geometry.collision"), QStringLiteral("DLL"),
            QStringLiteral("end_to_end"),
            QJsonObject{{QStringLiteral("capability"),
                         QStringLiteral("geometry.collision")},
                        {QStringLiteral("scene"), fixture.committedscene},
                        {QStringLiteral("expectedSceneRevision"), 1},
                        {QStringLiteral("buildVolume"),
                         fixture.committedscene.value(
                             QStringLiteral("buildVolume")).toObject()}},
            kSyncTimeoutMs, true, &ignored)
        || !invoke(QStringLiteral("P1"),
            QStringLiteral("geometry.preflight.fast"),
            QStringLiteral("DLL"), QStringLiteral("end_to_end"),
            QJsonObject{{QStringLiteral("capability"),
                         QStringLiteral("geometry.preflight")},
                        {QStringLiteral("mode"), QStringLiteral("fast")},
                        {QStringLiteral("modelId"), fixture.modelid}},
            kSyncTimeoutMs, true, &ignored))
    {
        return false;
    }

    (void)invoke(QStringLiteral("P2"),
        QStringLiteral("geometry.preflight.full"), QStringLiteral("Worker"),
        QStringLiteral("return_recorded"),
        QJsonObject{{QStringLiteral("capability"),
                     QStringLiteral("geometry.preflight")},
                    {QStringLiteral("jobId"), NewIdentity(
                         QStringLiteral("job-preflight"))},
                    {QStringLiteral("correlationId"), NewIdentity(
                         QStringLiteral("correlation-preflight"))},
                    {QStringLiteral("mode"), QStringLiteral("full")},
                    {QStringLiteral("scene"), fixture.committedscene},
                    {QStringLiteral("sceneHash"), fixture.scenehash},
                    {QStringLiteral("expectedSceneRevision"), 1},
                    {QStringLiteral("profile"), fixture.profile},
                    {QStringLiteral("profileHash"), fixture.profilehash},
                    {QStringLiteral("targetMode"), QStringLiteral("legacy")},
                    {QStringLiteral("buildVolume"),
                     fixture.committedscene.value(
                         QStringLiteral("buildVolume")).toObject()}},
        kWorkerTimeoutMs, false, &ignored);

    const QJsonObject sliceRequest{
        {QStringLiteral("capability"), QStringLiteral("slice.rgbwsv")},
        {QStringLiteral("jobId"), NewIdentity(QStringLiteral("job-slice"))},
        {QStringLiteral("correlationId"), NewIdentity(
             QStringLiteral("correlation-slice"))},
        {QStringLiteral("sceneHash"), fixture.scenehash},
        {QStringLiteral("scene"), fixture.committedscene},
        {QStringLiteral("profile"), fixture.profile},
        {QStringLiteral("output"), QJsonObject{
             {QStringLiteral("contract"), QStringLiteral("p0.rgbwsv.2")},
             {QStringLiteral("packageDir"), fixture.packagedirectory}}},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("backend"), QStringLiteral("worker")}}}};
    if (!invoke(QStringLiteral("P0"), QStringLiteral("slice.rgbwsv"),
            QStringLiteral("Worker"), QStringLiteral("end_to_end"),
            sliceRequest, kWorkerTimeoutMs, true, &ignored)
        || !invoke(QStringLiteral("P0"), QStringLiteral("package.verify"),
            QStringLiteral("DLL"), QStringLiteral("end_to_end"),
            QJsonObject{{QStringLiteral("capability"),
                         QStringLiteral("package.verify")},
                        {QStringLiteral("packageDir"),
                         fixture.packagedirectory}},
            kSyncTimeoutMs, true, &ignored))
    {
        return false;
    }

    if (!invoke(QStringLiteral("P1"),
            QStringLiteral("package.get_layer_descriptor"),
            QStringLiteral("DLL"), QStringLiteral("end_to_end"),
            QJsonObject{{QStringLiteral("capability"),
                         QStringLiteral("package.get_layer_descriptor")},
                        {QStringLiteral("packageDir"),
                         fixture.packagedirectory},
                        {QStringLiteral("layerIndex"), 0}},
            kSyncTimeoutMs, true, &ignored)
        || !invoke(QStringLiteral("P1"),
            QStringLiteral("package.render_layer_preview"),
            QStringLiteral("DLL"), QStringLiteral("end_to_end"),
            QJsonObject{{QStringLiteral("capability"),
                         QStringLiteral("package.render_layer_preview")},
                        {QStringLiteral("packageDir"),
                         fixture.packagedirectory},
                        {QStringLiteral("layerIndex"), 0},
                        {QStringLiteral("mode"),
                         QStringLiteral("composite")},
                        {QStringLiteral("channels"), QJsonArray{
                             QStringLiteral("R"), QStringLiteral("G"),
                             QStringLiteral("B"), QStringLiteral("W"),
                             QStringLiteral("S"), QStringLiteral("V")}},
                        {QStringLiteral("maxWidthPx"), 512},
                        {QStringLiteral("outputPath"), fixture.previewpath}},
            kSyncTimeoutMs, true, &ignored))
    {
        return false;
    }

    (void)invoke(QStringLiteral("P2"),
        QStringLiteral("package.get_summary"), QStringLiteral("DLL"),
        QStringLiteral("return_recorded"),
        QJsonObject{{QStringLiteral("capability"),
                     QStringLiteral("package.get_summary")},
                    {QStringLiteral("packageDir"), fixture.packagedirectory}},
        kSyncTimeoutMs, false, &ignored);
    (void)invoke(QStringLiteral("P2"),
        QStringLiteral("package.read_report"), QStringLiteral("DLL"),
        QStringLiteral("return_recorded"),
        QJsonObject{{QStringLiteral("capability"),
                     QStringLiteral("package.read_report")},
                    {QStringLiteral("packageDir"), fixture.packagedirectory},
                    {QStringLiteral("reportName"), QStringLiteral("slice")}},
        kSyncTimeoutMs, false, &ignored);

    const QString repairSource = QDir(runRoot).filePath(
        QStringLiteral("repair_source.obj"));
    QFile repairFile(repairSource);
    if (repairFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        repairFile.write(
            "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
            "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n");
        repairFile.close();
        (void)invoke(QStringLiteral("P2"),
            QStringLiteral("geometry.repair"), QStringLiteral("Worker"),
            QStringLiteral("return_recorded"),
            QJsonObject{{QStringLiteral("capability"),
                         QStringLiteral("geometry.repair")},
                        {QStringLiteral("jobId"), NewIdentity(
                             QStringLiteral("job-repair"))},
                        {QStringLiteral("correlationId"), NewIdentity(
                             QStringLiteral("correlation-repair"))},
                        {QStringLiteral("modelId"), fixture.modelid},
                        {QStringLiteral("modelPath"), repairSource},
                        {QStringLiteral("modelFormat"), QStringLiteral("obj")},
                        {QStringLiteral("outputPath"), QDir(runRoot).filePath(
                             QStringLiteral("repair_output.obj"))},
                        {QStringLiteral("profile"), fixture.profile},
                        {QStringLiteral("profileHash"), fixture.profilehash},
                        {QStringLiteral("sourceResourceScope"), QJsonObject{
                             {QStringLiteral("rootPath"), runRoot}}},
                        {QStringLiteral("repairOutputFormat"),
                         QStringLiteral("obj")},
                        {QStringLiteral("policy"),
                         QStringLiteral("conservative")},
                        {QStringLiteral("requireStrictPass"), true}},
            kWorkerTimeoutMs, false, &ignored);
    }

    capabilitycoveragefixture cancelFixture = fixture;
    cancelFixture.packagedirectory = QDir(runRoot).filePath(
        QStringLiteral("cancel-package"));
    QJsonObject cancelProfile;
    QString cancelProfileHash;
    QJsonObject cancelEvidence;
    if (!CapabilityCoverageRequests::BuildProfile(
            cancelFixture, 0.0001, &cancelProfile, &cancelProfileHash, error))
    {
        return false;
    }
    const QJsonObject cancelRequest{
        {QStringLiteral("capability"), QStringLiteral("slice.rgbwsv")},
        {QStringLiteral("jobId"), NewIdentity(QStringLiteral("job-cancel"))},
        {QStringLiteral("correlationId"), NewIdentity(
             QStringLiteral("correlation-cancel"))},
        {QStringLiteral("sceneHash"), fixture.scenehash},
        {QStringLiteral("scene"), fixture.committedscene},
        {QStringLiteral("profile"), cancelProfile},
        {QStringLiteral("output"), QJsonObject{
             {QStringLiteral("contract"), QStringLiteral("p0.rgbwsv.2")},
             {QStringLiteral("packageDir"),
              cancelFixture.packagedirectory}}},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("backend"), QStringLiteral("worker")}}}};
    if (!RunCancellationGate(
            cancelRequest, runRoot, &cancelEvidence, error))
    {
        return false;
    }

    (void)invoke(QStringLiteral("P2"), QStringLiteral("model.release"),
        QStringLiteral("DLL"), QStringLiteral("return_recorded"),
        QJsonObject{{QStringLiteral("capability"),
                     QStringLiteral("model.release")},
                    {QStringLiteral("modelId"), fixture.modelid}},
        kSyncTimeoutMs, false, &ignored);

    int p0Passed = 0;
    int p1Passed = 0;
    int p2Recorded = 0;
    for (const QJsonValue& entryValue : m_entries)
    {
        const QJsonObject entry = entryValue.toObject();
        if (!entry.value(QStringLiteral("satisfied")).toBool())
        {
            continue;
        }
        const QString tier = entry.value(QStringLiteral("tier")).toString();
        p0Passed += tier == QStringLiteral("P0") ? 1 : 0;
        p1Passed += tier == QStringLiteral("P1") ? 1 : 0;
        p2Recorded += tier == QStringLiteral("P2") ? 1 : 0;
    }
    const bool coveragePassed = p0Passed == 5 && p1Passed == 5
        && p2Recorded == 6;
    const QJsonObject report{
        {QStringLiteral("schema"),
         QStringLiteral("slicesoft.host_capability_coverage.14e04b.1")},
        {QStringLiteral("task"), QStringLiteral("14E-04b")},
        {QStringLiteral("runRoot"), QDir::fromNativeSeparators(runRoot)},
        {QStringLiteral("moduleCallCount"),
         static_cast<qint64>(m_client.CallCount())},
        {QStringLiteral("entries"), m_entries},
        {QStringLiteral("uiM5"), cancelEvidence},
        {QStringLiteral("uiM6"), QJsonObject{
             {QStringLiteral("coveredBy"),
              QStringLiteral("slicer_stage14e02_qt_host_missing_module_test")},
             {QStringLiteral("required"), true}}},
        {QStringLiteral("summary"), QJsonObject{
             {QStringLiteral("p0Passed"), p0Passed},
             {QStringLiteral("p0Required"), 5},
             {QStringLiteral("p1Passed"), p1Passed},
             {QStringLiteral("p1Required"), 5},
             {QStringLiteral("p2Recorded"), p2Recorded},
             {QStringLiteral("p2Required"), 6},
             {QStringLiteral("passed"), coveragePassed}}}};
    if (reportBytes != nullptr)
    {
        *reportBytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    }
    if (!WriteEvidence(runRoot, report, error))
    {
        return false;
    }
    if (!coveragePassed && error != nullptr)
    {
        *error = QStringLiteral("14E-04b 能力计数未达标：P0=%1 P1=%2 P2=%3")
                     .arg(p0Passed).arg(p1Passed).arg(p2Recorded);
    }
    return coveragePassed;
}
