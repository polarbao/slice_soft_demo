#include "SceneDocument.h"
#include "SceneSelectionModel.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

slicer_core::ModelInstance MakeInstance(
    const std::string& modelId,
    const std::string& instanceId)
{
    slicer_core::ModelInstance instance;
    instance.modelid = modelId;
    instance.instanceid = instanceId;
    instance.sourcetransformidentity = modelId + "-source";
    instance.sourcebboxmm = {{0.0, 0.0, 0.0}, {10.0, 5.0, 1.0}};
    instance.effectivebboxmm = instance.sourcebboxmm;
    return instance;
}

slicer_core::SceneViewGeometry MakeGeometry(
    const std::string& sceneId,
    const std::string& instanceId,
    const std::uint64_t sceneRevision)
{
    slicer_core::SceneViewGeometry geometry;
    geometry.sceneid = sceneId;
    geometry.instanceid = instanceId;
    geometry.scenerevision = sceneRevision;
    geometry.transformrevision = 0U;
    geometry.worldboundsmm = {{0.0, 0.0}, {10.0, 5.0}};
    geometry.triangles.push_back(
        {{0.0, 0.0}, {10.0, 0.0}, {0.0, 5.0}});
    geometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    return geometry;
}

void AddFirstInstance(SceneDocument& document)
{
    document.SetLoading(1U, QStringLiteral("first.obj"));
    Require(
        document.SetSceneContext(
            1U,
            QStringLiteral("scene-1"),
            1U,
            QStringLiteral("cache-first"),
            QStringLiteral("source-first"),
            QStringLiteral("resource-first"),
            MakeInstance("model-first", "instance-first")),
        "first scene context should be accepted");
    Require(
        document.SetGeometry(
            1U,
            MakeGeometry("scene-1", "instance-first", 1U)),
        "first geometry should be accepted");
}

void AddTwentyTwoAndRejectTwentyThird()
{
    SceneDocument document;
    AddFirstInstance(document);

    quint64 generation = 1U;
    for (int index = 2; index <= 22; ++index)
    {
        ++generation;
        const QString suffix = QString::number(index);
        document.SetAdding(
            generation,
            QStringLiteral("model-") + suffix + QStringLiteral(".obj"));
        Require(
            document.AddSceneContext(
                generation,
                QStringLiteral("scene-1"),
                document.SceneRevision() + 1U,
                QStringLiteral("cache-") + suffix,
                QStringLiteral("source-") + suffix,
                QStringLiteral("resource-") + suffix,
                MakeInstance(
                    "model-" + std::to_string(index),
                    "instance-" + std::to_string(index))),
            "instance within the 22-item limit should be accepted");
        Require(
            document.SetGeometry(
                generation,
                MakeGeometry(
                    "scene-1",
                    "instance-" + std::to_string(index),
                    document.SceneRevision())),
            "added geometry should be accepted");
        if (index == 11)
        {
            Require(
                document.InstanceCount() == 11U,
                "eleven-instance scene should remain editable");
        }
    }

    Require(document.InstanceCount() == 22U, "scene should contain 22 instances");
    Require(
        document.Items().front().instance.instanceid == "instance-first",
        "scene order should retain the first instance");
    Require(
        document.Items().back().instance.instanceid == "instance-22",
        "scene order should retain append order");

    const quint64 revisionBefore = document.SceneRevision();
    const std::size_t countBefore = document.InstanceCount();
    ++generation;
    document.SetAdding(generation, QStringLiteral("overflow.obj"));
    Require(
        !document.AddSceneContext(
            generation,
            QStringLiteral("scene-1"),
            revisionBefore + 1U,
            QStringLiteral("cache-overflow"),
            QStringLiteral("source-overflow"),
            QStringLiteral("resource-overflow"),
            MakeInstance("model-overflow", "instance-overflow")),
        "the twenty-third instance must be rejected");
    Require(
        document.SceneRevision() == revisionBefore
            && document.InstanceCount() == countBefore,
        "instance-limit rejection must not partially mutate the scene");
}

void DuplicateVisibilityLockAndDeleteAreAtomic()
{
    SceneDocument document;
    AddFirstInstance(document);

    const SceneDocumentOperationResult duplicated =
        document.DuplicateInstance(
            QStringLiteral("instance-first"),
            QStringLiteral("instance-copy"),
            document.SceneRevision());
    Require(duplicated.IsValid() && duplicated.changed, "duplicate should pass");
    Require(document.InstanceCount() == 2U, "duplicate should add one instance");
    Require(
        document.Items().at(0U).sourcecachekey
            == document.Items().at(1U).sourcecachekey,
        "duplicate must share the immutable source cache");
    Require(
        document.CurrentInstanceId() == QStringLiteral("instance-copy"),
        "duplicate should become current");

    const quint64 staleRevision = document.SceneRevision() - 1U;
    const SceneDocumentOperationResult stale =
        document.SetInstanceVisible(
            QStringLiteral("instance-copy"),
            false,
            staleRevision);
    Require(
        !stale.IsValid()
            && stale.error->code
                == SceneDocumentOperationErrorCode::SceneRevisionStale,
        "stale visibility command must fail closed");

    Require(
        document.SetInstanceVisible(
            QStringLiteral("instance-copy"),
            false,
            document.SceneRevision())
            .IsValid(),
        "visibility update should pass");
    Require(!document.Items().at(1U).instance.visible, "instance should be hidden");

    Require(
        document.SetInstanceLocked(
            QStringLiteral("instance-copy"),
            true,
            document.SceneRevision())
            .IsValid(),
        "lock should pass");
    const SceneDocumentOperationResult lockedDelete =
        document.DeleteInstance(
            QStringLiteral("instance-copy"),
            document.SceneRevision());
    Require(
        !lockedDelete.IsValid()
            && lockedDelete.error->code
                == SceneDocumentOperationErrorCode::InstanceLocked,
        "locked instance deletion must fail closed");

    Require(
        document.SetInstanceLocked(
            QStringLiteral("instance-copy"),
            false,
            document.SceneRevision())
            .IsValid(),
        "explicit unlock should pass");
    Require(
        document.DeleteInstance(
            QStringLiteral("instance-copy"),
            document.SceneRevision())
            .IsValid(),
        "unlocked deletion should pass");
    Require(document.InstanceCount() == 1U, "delete should remove one instance");
    Require(
        document.CurrentInstanceId() == QStringLiteral("instance-first"),
        "deleting current instance should select a deterministic neighbor");
}

void SelectionModelTracksCurrentInstance()
{
    SceneDocument document;
    SceneSelectionModel selection;
    AddFirstInstance(document);
    Require(
        document.DuplicateInstance(
            QStringLiteral("instance-first"),
            QStringLiteral("instance-copy"),
            document.SceneRevision())
            .IsValid(),
        "selection fixture duplicate should pass");

    selection.SetSelectedInstance(QStringLiteral("instance-first"));
    Require(
        document.SetCurrentInstance(selection.SelectedInstance()),
        "list selection should activate the matching document instance");
    Require(
        document.CurrentInstanceId() == QStringLiteral("instance-first"),
        "document should expose selected instance identity");
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    AddTwentyTwoAndRejectTwentyThird();
    DuplicateVisibilityLockAndDeleteAreAtomic();
    SelectionModelTracksCurrentInstance();
    std::cout << "scene_document_unit_tests passed\n";
    return 0;
}
