#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

// Rehydrate only the host-owned profile selection from the authoritative scene.
// Geometry, instance identities, transforms and resource bindings are copied unchanged.
inline bool BuildHostProfileRebindRequest(const QJsonObject& snapshot,
    quint64 revision, const QString& profileId, QJsonObject* request, QString* error)
{
    auto scene=snapshot.value(QStringLiteral("scene")).toObject();
    auto instances=scene.value(QStringLiteral("instances")).toArray();
    if (!snapshot.value(QStringLiteral("ok")).toBool() || instances.isEmpty()
        || scene.value(QStringLiteral("sceneRevision")).toVariant().toULongLong()!=revision)
    {
        if(error) *error=QStringLiteral("工艺切换缺少有效的当前场景快照。");
        return false;
    }
    const QString identity=QUuid::createUuid().toString(QUuid::WithoutBraces);
    scene.insert(QStringLiteral("sceneId"),QStringLiteral("scene-profile-%1").arg(identity));
    scene.insert(QStringLiteral("resolvedProfileId"),profileId);
    for(int i=0;i<instances.size();++i)
    {
        auto instance=instances.at(i).toObject();
        instance.insert(QStringLiteral("resolvedProfileId"),profileId);
        instances[i]=instance;
    }
    scene.insert(QStringLiteral("instances"),instances);
    // The existing inline-scene API requires an operation. A zero translation
    // commits the new authority without resetting layout or reimporting a model.
    *request=QJsonObject{
        {QStringLiteral("capability"),QStringLiteral("scene.apply_operation")},
        {QStringLiteral("operationId"),QStringLiteral("profile-%1").arg(identity)},
        {QStringLiteral("scene"),scene},
        {QStringLiteral("currentSceneRevision"),static_cast<qint64>(revision)},
        {QStringLiteral("expectedSceneRevision"),static_cast<qint64>(revision)},
        {QStringLiteral("operations"),QJsonArray{QJsonObject{
            {QStringLiteral("type"),QStringLiteral("translate")},
            {QStringLiteral("instanceId"),instances.first().toObject().value(QStringLiteral("instanceId"))},
            {QStringLiteral("deltaMm"),QJsonArray{0,0,0}}}}}};
    return true;
}
