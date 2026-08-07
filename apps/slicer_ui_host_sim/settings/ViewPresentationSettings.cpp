#include "ViewPresentationSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

ViewPresentationSettings::ViewPresentationSettings(
    const QString& sessionConfigPath)
    : m_sessionConfigPath(sessionConfigPath)
{
}

bool ViewPresentationSettings::Load(QString* error)
{
    QFile file(m_sessionConfigPath);
    if (!file.exists())
    {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法读取视图会话配置：%1")
                         .arg(m_sessionConfigPath);
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("视图会话配置不是有效 JSON：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }
    const QJsonObject view = document.object().value(
        QStringLiteral("viewPresentation")).toObject();
    const QString mode = view.value(
        QStringLiteral("defaultViewMode")).toString(QStringLiteral("top"));
    const QString projection = view.value(
        QStringLiteral("threeDProjection"))
        .toString(QStringLiteral("orthographic"));
    if (mode != QStringLiteral("top") && mode != QStringLiteral("three_d"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("defaultViewMode 仅允许 top/three_d。");
        }
        return false;
    }
    if (projection != QStringLiteral("orthographic")
        && projection != QStringLiteral("perspective"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "threeDProjection 仅允许 orthographic/perspective。");
        }
        return false;
    }
    m_defaultViewMode = mode == QStringLiteral("three_d")
        ? HostViewMode::ThreeD : HostViewMode::Top;
    m_threeDProjection = projection == QStringLiteral("perspective")
        ? slicer::render::Projection::Perspective
        : slicer::render::Projection::Orthographic;
    return true;
}

bool ViewPresentationSettings::Save(QString* error) const
{
    QJsonObject root;
    QFile existing(m_sessionConfigPath);
    if (existing.exists() && existing.open(QIODevice::ReadOnly))
    {
        const QJsonDocument current = QJsonDocument::fromJson(
            existing.readAll());
        if (current.isObject())
        {
            root = current.object();
        }
    }
    root.insert(QStringLiteral("viewPresentation"), QJsonObject{
        {QStringLiteral("defaultViewMode"),
         m_defaultViewMode == HostViewMode::ThreeD
             ? QStringLiteral("three_d") : QStringLiteral("top")},
        {QStringLiteral("threeDProjection"),
         m_threeDProjection == slicer::render::Projection::Perspective
             ? QStringLiteral("perspective")
             : QStringLiteral("orthographic")}});
    const QFileInfo info(m_sessionConfigPath);
    if (!info.absoluteDir().mkpath(QStringLiteral(".")))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法创建视图配置目录：%1")
                         .arg(info.absolutePath());
        }
        return false;
    }
    QSaveFile output(m_sessionConfigPath);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !output.commit())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法保存视图会话配置：%1")
                         .arg(m_sessionConfigPath);
        }
        return false;
    }
    return true;
}

HostViewMode ViewPresentationSettings::DefaultViewMode() const
{
    return m_defaultViewMode;
}

void ViewPresentationSettings::SetDefaultViewMode(const HostViewMode mode)
{
    m_defaultViewMode = mode;
}

slicer::render::Projection
ViewPresentationSettings::ThreeDProjection() const
{
    return m_threeDProjection;
}

void ViewPresentationSettings::SetThreeDProjection(
    const slicer::render::Projection projection)
{
    m_threeDProjection = projection;
}

QString ViewPresentationSettings::SessionConfigPath() const
{
    return m_sessionConfigPath;
}
