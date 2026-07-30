#include "SceneSliceActionController.h"

#include <QCoreApplication>

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

SceneSliceActionSceneState ReadyScene()
{
    SceneSliceActionSceneState state;
    state.sceneid = QStringLiteral("scene-controller-fixture");
    state.scenerevision = 7U;
    state.visibleinstancecount = 3U;
    state.allvisibleinstancesadmitted = true;
    return state;
}

SceneSliceSnapshotResult ValidSnapshot()
{
    SceneSliceSnapshotResult result;
    SceneSliceActionSnapshot snapshot;
    snapshot.sceneid = QStringLiteral("scene-controller-fixture");
    snapshot.scenerevision = 7U;
    snapshot.scenehash = QStringLiteral("scene-hash");
    snapshot.effectiveconfighash =
        QStringLiteral("config-hash");
    snapshot.effectiveconfigpath =
        QStringLiteral("scene_config.effective.json");
    snapshot.profileid = QStringLiteral("fixture-profile");
    snapshot.sessionid = QStringLiteral("fixture-session");
    snapshot.packagedir = QStringLiteral("fixture-package");
    result.snapshot = snapshot;
    return result;
}

bool SuccessFlowPublishesOnePackage()
{
    SceneSliceActionSceneState current = ReadyScene();
    SceneSliceActionController controller;
    bool processStarted{false};
    QString loadedPackage;
    QObject::connect(
        &controller,
        &SceneSliceActionController::SigPackageReady,
        [&loadedPackage](const QString& packageDir)
        {
            loadedPackage = packageDir;
        });
    controller.Configure(
        [&current]()
        {
            return current;
        },
        [](const SceneSliceActionRequest&)
        {
            return ValidSnapshot();
        },
        [&processStarted](
            const SceneSliceActionSnapshot& snapshot)
        {
            processStarted =
                snapshot.effectiveconfigpath
                == QStringLiteral(
                    "scene_config.effective.json");
            return processStarted;
        },
        []()
        {
        },
        [](const SceneSliceActionSnapshot& snapshot, const qint64)
        {
            SceneSlicePackageValidationResult result;
            result.valid = true;
            result.packagedir = snapshot.packagedir;
            return result;
        });

    const bool started =
        controller.Start(SceneSliceActionRequest{});
    controller.OnProcessFinished(0, 123);
    return ExpectTrue(
               started && processStarted,
               "ready scene launches explicit process")
        && ExpectTrue(
            controller.State()
                == SceneSliceActionState::Completed,
            "valid process and package complete")
        && ExpectTrue(
            loadedPackage == QStringLiteral("fixture-package"),
            "one validated package is published");
}

bool StaleAndCancelNeverPublish()
{
    SceneSliceActionSceneState current = ReadyScene();
    bool cancelled{false};
    int packageCount{0};
    SceneSliceActionController controller;
    QObject::connect(
        &controller,
        &SceneSliceActionController::SigPackageReady,
        [&packageCount](const QString&)
        {
            ++packageCount;
        });
    controller.Configure(
        [&current]()
        {
            return current;
        },
        [](const SceneSliceActionRequest&)
        {
            return ValidSnapshot();
        },
        [](const SceneSliceActionSnapshot&)
        {
            return true;
        },
        [&cancelled]()
        {
            cancelled = true;
        },
        [](const SceneSliceActionSnapshot&, const qint64)
        {
            SceneSlicePackageValidationResult result;
            result.valid = true;
            result.packagedir = QStringLiteral("fixture-package");
            return result;
        });

    if (!controller.Start(SceneSliceActionRequest{}))
    {
        return false;
    }
    ++current.scenerevision;
    controller.OnProcessFinished(0, 50);
    const bool staleBlocked =
        controller.State() == SceneSliceActionState::Blocked
        && controller.ErrorCode()
            == SceneSliceActionErrorCode::SceneStale
        && packageCount == 0;

    current = ReadyScene();
    if (!controller.Start(SceneSliceActionRequest{}))
    {
        return false;
    }
    controller.Cancel();
    const bool cancellationPending =
        cancelled
        && controller.IsRunning()
        && controller.State()
            == SceneSliceActionState::Cancelling;
    controller.OnProcessFinished(0, 50);
    return ExpectTrue(
               staleBlocked,
               "stale revision never loads package")
        && ExpectTrue(
            cancellationPending,
            "cancellation remains active until process exit")
        && ExpectTrue(
            controller.State()
                    == SceneSliceActionState::Cancelled
                && packageCount == 0,
            "cancelled process never loads package");
}

bool AdmissionAndModeFailClosed()
{
    SceneSliceActionSceneState current = ReadyScene();
    bool launched{false};
    SceneSliceActionController controller;
    controller.Configure(
        [&current]()
        {
            return current;
        },
        [](const SceneSliceActionRequest&)
        {
            return ValidSnapshot();
        },
        [&launched](const SceneSliceActionSnapshot&)
        {
            launched = true;
            return true;
        },
        []()
        {
        },
        [](const SceneSliceActionSnapshot&, const qint64)
        {
            return SceneSlicePackageValidationResult{};
        });

    current.allvisibleinstancesadmitted = false;
    const bool blocked =
        !controller.Start(SceneSliceActionRequest{})
        && controller.ErrorCode()
            == SceneSliceActionErrorCode::InstanceBlocked;
    current = ReadyScene();
    SceneSliceActionRequest global;
    global.mode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    const bool noFallback =
        !controller.Start(global)
        && controller.ErrorCode()
            == SceneSliceActionErrorCode::
                PipelineModeNotAdmitted;
    return ExpectTrue(
               blocked,
               "unadmitted instance blocks before snapshot")
        && ExpectTrue(
            noFallback && !launched,
            "Global scene mode does not launch or fall back");
}

bool PackageFailureIsStable()
{
    SceneSliceActionSceneState current = ReadyScene();
    SceneSliceActionController controller;
    controller.Configure(
        [&current]()
        {
            return current;
        },
        [](const SceneSliceActionRequest&)
        {
            return ValidSnapshot();
        },
        [](const SceneSliceActionSnapshot&)
        {
            return true;
        },
        []()
        {
        },
        [](const SceneSliceActionSnapshot&, const qint64)
        {
            SceneSlicePackageValidationResult result;
            result.errors.push_back(
                QStringLiteral("scene identity mismatch"));
            return result;
        });
    if (!controller.Start(SceneSliceActionRequest{}))
    {
        return false;
    }
    controller.OnProcessFinished(0, 10);
    return ExpectTrue(
        controller.State() == SceneSliceActionState::Failed
            && controller.ErrorCode()
                == SceneSliceActionErrorCode::PackageInvalid,
        "invalid package fails with stable error");
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const bool ok =
        SuccessFlowPublishesOnePackage()
        && StaleAndCancelNeverPublish()
        && AdmissionAndModeFailClosed()
        && PackageFailureIsStable();
    if (!ok)
    {
        return 1;
    }
    std::cout
        << "scene_slice_action_controller_unit_tests: PASS\n";
    return 0;
}
