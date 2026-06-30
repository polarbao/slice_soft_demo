# DEV_07_Qt调试UI架构设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：PRD_07  
> 所属模块：apps/slicer_debug_ui  
> 技术栈：C++20 / Qt 5.15 Widgets / CMake  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

新增 Qt Widgets 应用：

```text
apps/slicer_debug_ui
```

第一版采用：

```text
QProcess wrapper
```

调用现有工具：

```text
slicer_cli
rip_reader_test
scripts/run_regression.ps1
scripts/compare_material_profiles.ps1
```

不把 slicer_core 直接接入 UI 主线程。

---

## 2. 推荐目录结构

```text
apps/
  slicer_debug_ui/
    CMakeLists.txt
    main.cpp
    MainWindow.h
    MainWindow.cpp
    widgets/
      ConfigPanel.*
      RunPanel.*
      ReportTree.*
      PreviewPanel.*
      MaterialProcessPanel.*
      LogPanel.*
    services/
      ToolPaths.*
      ProcessRunner.*
      PackageLoader.*
      ReportLoader.*
      PreviewLoader.*
      ProfileCompareRunner.*
```

---

## 3. CMake

新增可选构建：

```cmake
option(BUILD_SLICER_DEBUG_UI "Build Qt slicer debug UI" ON)
```

如果 Qt 不存在：

```text
CLI targets 仍必须能构建；
UI target 可跳过。
```

示例：

```cmake
find_package(Qt5 COMPONENTS Widgets REQUIRED)
add_executable(slicer_debug_ui ...)
target_link_libraries(slicer_debug_ui PRIVATE Qt5::Widgets)
```

---

## 4. 核心类设计

### 4.1 ProcessRunner

职责：

```text
异步运行外部命令
捕获 stdout / stderr
记录 exit code
记录 duration
发出 started / output / finished / failed signal
```

推荐：

```cpp
class ProcessRunner : public QObject {
    Q_OBJECT
public:
    void run(const QString& program, const QStringList& args, const QString& workingDir);
signals:
    void started(QString command);
    void output(QString text);
    void errorOutput(QString text);
    void finished(int exitCode, qint64 elapsedMs);
};
```

---

### 4.2 PackageLoader

职责：

```text
读取 package manifest
定位 reports
定位 preview
检查 package 是否有效
```

输出：

```text
PackageSummary
```

---

### 4.3 ReportLoader

职责：

```text
读取 JSON
解析 key summary
提供 raw JSON view
```

第一版可使用：

```text
QJsonDocument
QJsonObject
QJsonArray
```

---

### 4.4 PreviewLoader

职责：

```text
扫描 preview 目录
按 channel / layer index 组织图像
支持 PNG / PPM
返回 QImage
```

---

## 5. MainWindow 布局建议

```text
Left:
  Config / Package panel

Center:
  Preview panel
  Report tabs

Right:
  MaterialProcess summary
  RIP summary
  Warnings / failures

Bottom:
  Log console
```

---

## 6. 命令封装

### 6.1 Run Slicer

```text
build/Debug/slicer_cli.exe --config <configPath>
```

### 6.2 Run RIP Summary

```text
build/Debug/rip_reader_test.exe --package <packagePath> --summary
```

### 6.3 Quick Regression

```text
powershell -ExecutionPolicy Bypass -File scripts/run_regression.ps1 -Mode quick
```

### 6.4 Profile Compare

```text
powershell -ExecutionPolicy Bypass -File scripts/compare_material_profiles.ps1 -PackageA <A> -PackageB <B> -Output <out>
```

---

## 7. Report Summary 解析

第一版重点解析：

```text
manifest.schema
manifest.tiff.storageMode
manifest.tiff.channelOrder
material_process_report.validation.pass
material_process_report.profileName
material_process_report.rgb.printPixels
material_process_report.white.printPixels
material_process_report.varnish.printPixels
material_process_report.support.printPixels
material_process_report.warnings
material_process_report.validation.failures
```

其他 report 可先 raw JSON 展示。

---

## 8. Preview 支持

优先支持：

```text
PNG
PPM
```

Preview 文件命名不强绑定，只要从 `preview_report.json` 或 preview 目录扫描即可。

---

## 9. 错误处理

UI 不吞错误：

```text
QProcess exitCode != 0 时显示 FAIL
stderr 红色显示
E_* 错误码高亮
JSON 解析失败显示具体文件路径
缺失 report 显示 warning，不崩溃
```

---

## 10. 不做内容

DEV_07 不做：

```text
设备通信
切片核心改造
RIP 半色调
OpenVDB
生产级任务队列
模型 3D viewport
```

---

## 11. 实施顺序

```text
1. CMake 增加 optional Qt UI target；
2. MainWindow 空壳；
3. ProcessRunner；
4. Config / Package 选择；
5. Run Slicer / Run RIP；
6. LogPanel；
7. ReportLoader / ReportTree；
8. PreviewPanel；
9. MaterialProcessPanel；
10. Profile Compare；
11. quick regression 按钮；
12. REPORT_07。
```

---

## 12. 结论

07 的架构重点是：

```text
CLI wrapper + report viewer + preview viewer
```

不要把第一版做成全量生产 UI。
