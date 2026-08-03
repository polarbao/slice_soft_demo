#include "ConfigValidator.h"
#include "SingleMaterialReliefResolver.h"
#include "slicer_core/config.h"

#include <QJsonArray>
#include <QSet>

#include <cmath>

namespace {

bool hasObject(const QJsonObject& object, const QString& key) {
    return object.contains(key) && object.value(key).isObject();
}

QString stringAt(const QJsonObject& object, const QString& key) {
    return object.value(key).toString().trimmed();
}

void checkNonNegativeInt(const QJsonObject& object,
                         const QString& key,
                         const QString& label,
                         ConfigValidationResult& result) {
    if (!object.contains(key)) {
        return;
    }
    const QJsonValue value = object.value(key);
    if (!value.isDouble() || value.toInt(-1) < 0) {
        result.errors.push_back(label + " 必须是非负整数。");
    }
}

void checkNonNegativeDouble(const QJsonObject& object,
                            const QString& key,
                            const QString& label,
                            ConfigValidationResult& result) {
    if (!object.contains(key)) {
        return;
    }
    const QJsonValue value = object.value(key);
    if (!value.isDouble() || value.toDouble(-1.0) < 0.0) {
        result.errors.push_back(label + " 必须是非负数。");
    }
}

void checkPositiveDouble(const QJsonObject& object,
                         const QString& key,
                         const QString& label,
                         ConfigValidationResult& result) {
    if (!object.contains(key)) {
        return;
    }
    const QJsonValue value = object.value(key);
    if (!value.isDouble() || value.toDouble(0.0) <= 0.0) {
        result.errors.push_back(label + " 必须大于 0。");
    }
}

bool isAllowed(const QString& value, const QSet<QString>& allowed) {
    return value.isEmpty() || allowed.contains(value);
}

void CheckOutputDpi(
    const QJsonObject& output,
    const QString& key,
    ConfigValidationResult& result)
{
    if (!output.contains(key))
    {
        return;
    }
    const QJsonValue value = output.value(key);
    const double numericValue = value.toDouble(-1.0);
    if (!value.isDouble()
        || std::floor(numericValue) != numericValue
        || numericValue < static_cast<double>(slicer_core::kMinimumOutputDpi)
        || numericValue > static_cast<double>(slicer_core::kMaximumOutputDpi))
    {
        result.errors.push_back(
            QStringLiteral("output.%1 必须是 %2..%3 范围内的整数。").arg(
                key,
                QString::number(slicer_core::kMinimumOutputDpi),
                QString::number(slicer_core::kMaximumOutputDpi)));
    }
}

void CheckTiffCompression(
    const QJsonObject& output,
    ConfigValidationResult& result)
{
    if (!output.contains("tiffCompression"))
    {
        return;
    }
    const QJsonValue value = output.value("tiffCompression");
    if (!value.isObject())
    {
        result.errors.push_back(
            "output.tiffCompression 必须是包含 algorithm 的对象。");
        return;
    }
    const QJsonObject compression = value.toObject();
    const QString algorithm = stringAt(compression, "algorithm");
    if (!compression.value("algorithm").isString()
        || (algorithm != "none" && algorithm != "packbits"))
    {
        result.errors.push_back(
            "output.tiffCompression.algorithm 只能是 none 或 packbits。");
    }
}

}  // namespace

ConfigValidationResult ConfigValidator::validate(const QJsonObject& root) {
    ConfigValidationResult result;

    const QJsonObject input = root.value("input").toObject();
    if (stringAt(input, "modelPath").isEmpty()) {
        result.errors.push_back("input.modelPath 不能为空。");
    }

    const QJsonObject output = root.value("output").toObject();
    if (stringAt(output, "packageDir").isEmpty()) {
        result.errors.push_back("output.packageDir 不能为空。");
    }
    CheckOutputDpi(output, QStringLiteral("dpiX"), result);
    CheckOutputDpi(output, QStringLiteral("dpiY"), result);
    CheckTiffCompression(output, result);
    const QString storage_mode = stringAt(output, "storageMode");
    if (!storage_mode.isEmpty() && storage_mode != "stripped" && storage_mode != "tiled") {
        result.errors.push_back("output.storageMode 只能是 stripped 或 tiled。");
    }
    const QJsonValue layer_thickness = output.value("layerThicknessMm");
    if (layer_thickness.isDouble() && layer_thickness.toDouble() <= 0.0) {
        result.errors.push_back("output.layerThicknessMm 必须大于 0。");
    }
    if (output.contains("bitDepth") && output.value("bitDepth").toInt(-1) != 8)
    {
        result.errors.push_back("output.bitDepth 必须保持为 8。");
    }
    if (output.contains("channelOrder"))
    {
        const QJsonArray expectedOrder{"R", "G", "B", "W", "S", "V"};
        if (output.value("channelOrder").toArray() != expectedOrder)
        {
            result.errors.push_back("output.channelOrder 必须保持为 R G B W S V。");
        }
    }

    const QJsonObject background = root.value("background").toObject();
    if (background.contains("value") && background.value("value").toInt(-1) != 255)
    {
        result.errors.push_back("background.value 必须保持为 255（空白不打印）。");
    }

    if (hasObject(root, "modelTransform"))
    {
        const QJsonObject transform = root.value("modelTransform").toObject();
        if (transform.contains("scale"))
        {
            const QJsonArray scale = transform.value("scale").toArray();
            if (scale.size() != 3)
            {
                result.errors.push_back("modelTransform.scale 必须包含 X/Y/Z 三个缩放值。");
            }
            else
            {
                for (int index = 0; index < scale.size(); ++index)
                {
                    if (!scale.at(index).isDouble() || scale.at(index).toDouble() <= 0.0)
                    {
                        result.errors.push_back(
                            QString("modelTransform.scale[%1] 必须大于 0。").arg(index));
                    }
                }
            }
        }
    }

    const QSet<QString> roles{"rgb", "white", "varnish", "ignore", "support_candidate", "support"};
    if (hasObject(root, "materialRoleMapping")) {
        const QJsonObject mapping = root.value("materialRoleMapping").toObject();
        const QString default_role = stringAt(mapping, "defaultRole");
        if (!isAllowed(default_role, roles)) {
            result.errors.push_back("materialRoleMapping.defaultRole 不是合法角色。");
        }
        const QJsonArray rules = mapping.value("rules").toArray();
        for (int i = 0; i < rules.size(); ++i) {
            const QJsonObject rule = rules.at(i).toObject();
            if (!isAllowed(stringAt(rule, "role"), roles)) {
                result.errors.push_back(QString("materialRoleMapping.rules[%1].role 不是合法角色。").arg(i));
            }
        }
    }

    if (hasObject(root, "materialPolicy")) {
        const QJsonObject varnish = root.value("materialPolicy").toObject().value("varnish").toObject();
        checkNonNegativeInt(varnish, "topLayers", "materialPolicy.varnish.topLayers", result);
    }

    if (hasObject(root, "modelFill")) {
        const QJsonObject modelFill = root.value("modelFill").toObject();
        const QSet<QString> materials{"white", "varnish", "rgb", "profile_default", "material_role"};
        if (!isAllowed(stringAt(modelFill, "material"), materials)) {
            result.errors.push_back("modelFill.material 不是当前认可的模型内部填充材料。");
        }
        const QSet<QString> scopes{
            "solid_volume",
            "below_texture_surface",
            "all_model",
            "complement_of_global_texture_shell"};
        if (!isAllowed(stringAt(modelFill, "scope"), scopes)) {
            result.errors.push_back("modelFill.scope 不是当前认可的模型填充范围。");
        }
        const QJsonObject texture = root.value("texture").toObject();
        if (modelFill.value("enabled").toBool(false)
            && stringAt(modelFill, "scope") == "below_texture_surface"
            && stringAt(modelFill, "material") != "rgb"
            && texture.value("enabled").toBool(false)
            && stringAt(texture, "applyMode") == "solid_volume_from_top_surface")
        {
            result.warnings.push_back(
                "纹理投影到整个实体会占满模型内部区域，导致白墨/光油模型填充像素为 0；请改用 top_surface_band。");
        }
    }

    if (hasObject(root, "materialProcessProfile")) {
        const QJsonObject profile = root.value("materialProcessProfile").toObject();
        const QJsonObject varnish = profile.value("varnish").toObject();
        checkNonNegativeInt(varnish, "topLayers", "materialProcessProfile.varnish.topLayers", result);
    }

    if (hasObject(root, "support")) {
        const QJsonObject support = root.value("support").toObject();
        const QSet<QString> modes{
            "none",
            "bottom_projection",
            "unsupported_only",
            "bottom_plus_unsupported",
            "bottom_projection_plus_unsupported",
            "full_vertical_projection",
            "island_filter"};
        const QString mode = stringAt(support, "mode");
        if (!isAllowed(mode, modes)) {
            result.errors.push_back("support.mode 不是当前 UI 认可的基础模式。");
        }
        const QSet<QString> placements{"lower", "upper", "both", "unsupported_only", "full_vertical_projection"};
        const QString placement = support.contains("placement") ? stringAt(support, "placement") : QString{"lower"};
        if (!isAllowed(placement, placements)) {
            result.errors.push_back("support.placement 不是当前认可的支撑摆放方式。");
        }
        const QJsonObject internalVoid = support.value("internalVoid").toObject();
        if (!internalVoid.isEmpty()) {
            checkNonNegativeInt(internalVoid, "minAreaPx", "support.internalVoid.minAreaPx", result);
            const QSet<QString> fillRules{"all_internal_voids"};
            if (!isAllowed(stringAt(internalVoid, "fillRule"), fillRules)) {
                result.errors.push_back("support.internalVoid.fillRule 不是当前认可的内部镂空填充规则。");
            }
        }
        const QJsonObject upper = support.value("upper").toObject();
        if (!upper.isEmpty()) {
            const QSet<QString> outsideValues{"outer_varnish_shell", "model_envelope"};
            if (!isAllowed(stringAt(upper, "outside"), outsideValues)) {
                result.errors.push_back("support.upper.outside 不是当前认可的上表面支撑外侧边界。");
            }
        }
        const QJsonObject baseProjection =
            support.value("baseProjection").toObject();
        if (!baseProjection.isEmpty())
        {
            checkNonNegativeInt(
                baseProjection,
                "layerCount",
                "support.baseProjection.layerCount",
                result);
            if (baseProjection.value("layerCount").toInt(30) > 1000)
            {
                result.errors.push_back(
                    "support.baseProjection.layerCount 不能超过 1000。");
            }
            const QSet<QString> sourceValues{
                "max_support_footprint"};
            if (!isAllowed(
                    stringAt(baseProjection, "source"),
                    sourceValues))
            {
                result.errors.push_back(
                    "support.baseProjection.source 不是当前认可的铺底投影来源。");
            }
            const QSet<QString> placementValues{
                "overlay_existing",
                "prepend_below_model"};
            const QString layerPlacement =
                stringAt(baseProjection, "layerPlacement");
            if (!layerPlacement.isEmpty()
                && !isAllowed(layerPlacement, placementValues))
            {
                result.errors.push_back(
                    "support.baseProjection.layerPlacement 不是当前认可的铺底层放置方式。");
            }
        }
        checkNonNegativeInt(support, "minIslandAreaPx", "support.minIslandAreaPx", result);
        checkNonNegativeInt(support, "xyDilationPx", "support.xyDilationPx", result);
        checkNonNegativeInt(support, "connectivity", "support.connectivity", result);
    } else {
        result.warnings.push_back("未找到 support 配置段。");
    }

    if (hasObject(root, "outerVarnish")) {
        const QJsonObject outerVarnish = root.value("outerVarnish").toObject();
        checkNonNegativeDouble(outerVarnish, "thicknessMm", "outerVarnish.thicknessMm", result);
        checkPositiveDouble(outerVarnish, "thicknessStepMm", "outerVarnish.thicknessStepMm", result);
        checkPositiveDouble(outerVarnish, "pixelPitchUm", "outerVarnish.pixelPitchUm", result);
        const QSet<QString> conflictPolicies{"varnish_shell_wins"};
        if (!isAllowed(stringAt(outerVarnish, "conflictPolicy"), conflictPolicies)) {
            result.errors.push_back("outerVarnish.conflictPolicy 不是当前认可的外侧光油冲突策略。");
        }
    }

    if (hasObject(root, "surfaceVarnish")) {
        const QJsonObject surfaceVarnish = root.value("surfaceVarnish").toObject();
        checkNonNegativeInt(surfaceVarnish, "thicknessPx", "surfaceVarnish.thicknessPx", result);
        if (surfaceVarnish.value("enabled").toBool(false)
            && surfaceVarnish.value("thicknessPx").toInt(0) <= 0) {
            result.errors.push_back("surfaceVarnish.thicknessPx 在启用表面光油时必须大于 0。");
        }
        const QSet<QString> sources{"explicit", "material_policy"};
        if (!isAllowed(stringAt(surfaceVarnish, "source"), sources)) {
            result.errors.push_back("surfaceVarnish.source 不是当前认可的表面光油来源。");
        }
    }

    if (hasObject(root, "preview")) {
        const QJsonObject preview = root.value("preview").toObject();
        const QJsonValue interval = preview.value("interval");
        if (interval.isDouble() && interval.toInt() <= 0) {
            result.errors.push_back("preview.interval 必须大于 0。");
        }
        const QString outputPolicy =
            preview.value("outputPolicy").toString();
        if (!outputPolicy.isEmpty()
            && outputPolicy != QStringLiteral("tiff_native")
            && outputPolicy
                != QStringLiteral("tiff_native_with_diagnostics"))
        {
            result.errors.push_back(
                "preview.outputPolicy 必须是 tiff_native 或 tiff_native_with_diagnostics。");
        }
        if (!outputPolicy.isEmpty()
            && preview.contains("enabled")
            && preview.value("enabled").toBool()
                != (outputPolicy
                    == QStringLiteral("tiff_native_with_diagnostics")))
        {
            result.warnings.push_back(
                "preview.outputPolicy 与旧 enabled 冲突，将以 outputPolicy 为准。");
        }
    }

    if (hasObject(root, "experimental")) {
        const QJsonObject openvdb = root.value("experimental").toObject().value("openvdbPipeline").toObject();
        if (openvdb.value("writeProductionRgbwsv").toBool(false)) {
            result.errors.push_back("experimental.openvdbPipeline.writeProductionRgbwsv 当前 UI 禁止启用。");
        }
    }

    if (!root.contains("materialProcessProfile")) {
        result.warnings.push_back("未找到 materialProcessProfile 配置段。");
    }
    if (!root.contains("materialPolicy")) {
        result.warnings.push_back("未找到 materialPolicy 配置段。");
    }

    const QString effectiveProfileId =
        root.value(QStringLiteral("materialProcessProfile"))
            .toObject()
            .value(QStringLiteral("target"))
            .toString();
    if (effectiveProfileId == QStringLiteral("single_material_relief"))
    {
        const SingleMaterialReliefState materialState =
            SingleMaterialReliefResolver::Read(
                root,
                effectiveProfileId,
                true,
                false);
        if (!materialState.valid)
        {
            result.errors.push_back(
                SingleMaterialReliefResolver::ErrorCodeValue(
                    materialState.errorcode)
                + QStringLiteral(": ")
                + (materialState.issues.isEmpty()
                       ? QStringLiteral("单材料浮雕 W/V 配置字段组不一致。")
                       : materialState.issues.front()));
        }
    }

    return result;
}
