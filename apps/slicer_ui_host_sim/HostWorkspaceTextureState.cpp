#include "HostWorkspaceTextureState.h"

#include <QSettings>

namespace
{
template <typename Enum>
bool ReadEnum(
    QSettings& settings,
    const QString& key,
    const int maximum,
    Enum* value)
{
    const int raw = settings.value(key, -1).toInt();
    if (value == nullptr || raw < 0 || raw > maximum)
    {
        return false;
    }
    *value = static_cast<Enum>(raw);
    return true;
}
}

void HostWorkspaceTextureState::Save(
    QSettings& settings,
    const hosttexturesettings& texture)
{
    settings.setValue(QStringLiteral("texture/enabled"), texture.enabled);
    settings.setValue(
        QStringLiteral("texture/applyMode"),
        static_cast<int>(texture.applymode));
    settings.setValue(
        QStringLiteral("texture/topSurfaceLayers"),
        texture.topsurfacelayers);
    settings.setValue(
        QStringLiteral("texture/sampler"),
        static_cast<int>(texture.sampler));
    settings.setValue(
        QStringLiteral("texture/uvAddressMode"),
        static_cast<int>(texture.uvaddressmode));
    settings.setValue(QStringLiteral("texture/flipV"), texture.flipv);
    settings.setValue(
        QStringLiteral("texture/fallbackRed"), texture.fallbackred);
    settings.setValue(
        QStringLiteral("texture/fallbackGreen"), texture.fallbackgreen);
    settings.setValue(
        QStringLiteral("texture/fallbackBlue"), texture.fallbackblue);
    settings.setValue(
        QStringLiteral("texture/missingPolicy"),
        static_cast<int>(texture.missingpolicy));
    settings.setValue(
        QStringLiteral("texture/nonSurfacePolicy"),
        static_cast<int>(texture.nonsurfacepolicy));
    settings.setValue(
        QStringLiteral("texture/whitePolicy"),
        static_cast<int>(texture.whitepolicy));
    settings.setValue(
        QStringLiteral("texture/whiteInkThreshold"),
        texture.whiteinkthreshold);
    settings.setValue(
        QStringLiteral("texture/whiteValue"), texture.whitevalue);
}

bool HostWorkspaceTextureState::Restore(
    QSettings& settings,
    hosttexturesettings* texture)
{
    if (texture == nullptr)
    {
        return false;
    }
    hosttexturesettings restored;
    restored.enabled = settings.value(
        QStringLiteral("texture/enabled")).toBool();
    restored.topsurfacelayers = settings.value(
        QStringLiteral("texture/topSurfaceLayers"), -1).toInt();
    restored.flipv = settings.value(
        QStringLiteral("texture/flipV")).toBool();
    restored.fallbackred = settings.value(
        QStringLiteral("texture/fallbackRed"), -1).toInt();
    restored.fallbackgreen = settings.value(
        QStringLiteral("texture/fallbackGreen"), -1).toInt();
    restored.fallbackblue = settings.value(
        QStringLiteral("texture/fallbackBlue"), -1).toInt();
    restored.whiteinkthreshold = settings.value(
        QStringLiteral("texture/whiteInkThreshold"), -1).toInt();
    restored.whitevalue = settings.value(
        QStringLiteral("texture/whiteValue"), -1).toInt();
    const bool enumsValid = ReadEnum(
            settings, QStringLiteral("texture/applyMode"), 2,
            &restored.applymode)
        && ReadEnum(
            settings, QStringLiteral("texture/sampler"), 1,
            &restored.sampler)
        && ReadEnum(
            settings, QStringLiteral("texture/uvAddressMode"), 1,
            &restored.uvaddressmode)
        && ReadEnum(
            settings, QStringLiteral("texture/missingPolicy"), 1,
            &restored.missingpolicy)
        && ReadEnum(
            settings, QStringLiteral("texture/nonSurfacePolicy"), 1,
            &restored.nonsurfacepolicy)
        && ReadEnum(
            settings, QStringLiteral("texture/whitePolicy"), 1,
            &restored.whitepolicy);
    const bool valuesValid = restored.topsurfacelayers > 0
        && restored.topsurfacelayers <= 100000
        && restored.fallbackred >= 0 && restored.fallbackred <= 255
        && restored.fallbackgreen >= 0 && restored.fallbackgreen <= 255
        && restored.fallbackblue >= 0 && restored.fallbackblue <= 255
        && restored.whiteinkthreshold >= 0
        && restored.whiteinkthreshold <= 255
        && restored.whitevalue >= 0 && restored.whitevalue <= 255;
    if (!enumsValid || !valuesValid)
    {
        return false;
    }
    *texture = restored;
    return true;
}
