# 运控 SDK 版本发布检查清单

## 1. 版本准备

- [ ] `componentId` 仍为 `motion-runtime`。
- [ ] 实现版本、API 契约和通信协议版本没有混用。
- [ ] 版本增量符合 SemVer。
- [ ] prerelease 与当前阶段一致；稳定版为空。
- [ ] `CHANGELOG.md` 已更新。
- [ ] 版本查询不执行初始化、连接或硬件命令。

## 2. 构建与一致性

- [ ] CMake 配置输出的实现版本与 manifest 一致。
- [ ] `MC_GetVersion()` 与 `implementationVersion` 一致。
- [ ] `MC_GetVersionInfo()` 字段完整。
- [ ] Git 可用时，构建 revision 为当前 12 位 revision。
- [ ] dirty 工作树明确带 `.dirty`。
- [ ] Git 不可用时显示 `unknown`，不伪装为 clean。
- [ ] Debug 完整构建标识包含 `.debug.`，且 `runtimeLibrary=/MDd`。
- [ ] Release 完整构建标识包含 `.release.`，且 `runtimeLibrary=/MD`。
- [ ] 第三方静态/动态依赖分别包含 `thirdparty-static` / `thirdparty-dynamic`。
- [ ] 构建 manifest 记录实际 vcpkg target triplet。
- [ ] DLL `ProductVersion` 与实现版本一致，`FileVersion` 明确包含 `-debug` 或 `-release`。
- [ ] CPack 和发布包目录内 manifest 与实现版本一致。
- [ ] Debug/Release ZIP 文件名分别包含 `-debug-` / `-release-`，不会互相覆盖。
- [ ] UI、启动日志和 API 读取同一结构化版本快照。

## 3. 推荐验证命令

```powershell
cmake --preset main
cmake --build --preset main-release
ctest --preset sdk-release-tests --output-on-failure

cmake --preset with-test-app
cmake --build --preset motion-test-app-release
```

发布包消费验证按 `docs/guides/运控SDK发布包集成指南.md` 执行。只记录真实执行并通过的命令。

## 4. 稳定发布

- [ ] 发布工作树干净，或 dirty 状态已经阻断正式发布。
- [ ] 发布包包含源码 manifest 和构建 manifest。
- [ ] 记录编译器、MSVC runtime、vcpkg baseline/triplet 和测试结果。
- [ ] 计算并归档发布制品 SHA256。
- [ ] 创建 `vMAJOR.MINOR.PATCH` annotated tag。
- [ ] tag、manifest、DLL 属性、构建 manifest 和 changelog 一致。
- [ ] 真机状态单独记录，不用软件侧验证替代。

## 5. 回滚

- [ ] 可以定位上一稳定 tag 和 source revision。
- [ ] 可以恢复上一受控 manifest 和完整发布包。
- [ ] 按整包哈希回滚，不在同一进程内热替换单个 DLL。
- [ ] 回滚后重新执行版本快照与兼容性检查。
