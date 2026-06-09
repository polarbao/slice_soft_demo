#include "ConfigValidator.h"

#include <QJsonArray>
#include <QSet>

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

bool isAllowed(const QString& value, const QSet<QString>& allowed) {
    return value.isEmpty() || allowed.contains(value);
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
    const QString storage_mode = stringAt(output, "storageMode");
    if (!storage_mode.isEmpty() && storage_mode != "stripped" && storage_mode != "tiled") {
        result.errors.push_back("output.storageMode 只能是 stripped 或 tiled。");
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
        checkNonNegativeInt(support, "minIslandAreaPx", "support.minIslandAreaPx", result);
        checkNonNegativeInt(support, "xyDilationPx", "support.xyDilationPx", result);
        checkNonNegativeInt(support, "connectivity", "support.connectivity", result);
    } else {
        result.warnings.push_back("未找到 support 配置段。");
    }

    if (hasObject(root, "preview")) {
        const QJsonValue interval = root.value("preview").toObject().value("interval");
        if (interval.isDouble() && interval.toInt() <= 0) {
            result.errors.push_back("preview.interval 必须大于 0。");
        }
    }

    if (!root.contains("materialProcessProfile")) {
        result.warnings.push_back("未找到 materialProcessProfile 配置段。");
    }
    if (!root.contains("materialPolicy")) {
        result.warnings.push_back("未找到 materialPolicy 配置段。");
    }

    return result;
}
