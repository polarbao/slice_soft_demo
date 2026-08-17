# 运控 SDK 版本管理资料

本目录吸收并按 motionControlSDK 实际边界整理自
`version_management_module_kit_v1.0.0`。原规则包面向多个 PrintSolution 模块，本目录只保留
运控 SDK 维护者和发布人员需要长期使用的内容。

推荐阅读顺序：

1. [统一版本规则.md](统一版本规则.md)：理解版本对象、SemVer 和禁止项。
2. [运控SDK版本实施指南.md](运控SDK版本实施指南.md)：理解本仓库的 manifest、CMake、API 和制品映射。
3. [运控SDK版本发布检查清单.md](运控SDK版本发布检查清单.md)：执行候选版和稳定版发布检查。

未复制原规则包中的 `04_PRINTSOLUTION_CURRENT_MAPPING.md`、宿主组件登记示例和
`PACKAGE_INFO.json`，因为这些内容描述的是 PrintSolution 宿主状态，不是运控 SDK 的发布契约。
通用模板也未原样复制；其有效字段已经落入仓库根目录的 `version-manifest.json`、CMake 生成逻辑、
公开 `MotionSdkVersionInfo` 和本目录文档。
