#pragma once

// F-09 单测批次 5 起，用例分散到多个编译单元、由 Main.cpp 汇总。
//
// 【为什么这样拆】续写 Main.cpp 会撞 G1 的 500 行阈值；
// 每批另起一个 CMake 目标又在给 R-08 加目标块（批次 3、4 已经各加了一个）。
// 让每个 TU 各自跑一套 RunCases、Main.cpp 把返回码或起来，两头都避开了，
// 而且【不需要动 tests/support/Expect.h】——那个头有 20 个文件在包含。
//
// 约定：每个 Run*Cases 自成一套（自带套件名），全通过返回 0。

/// 报告序列化器（output/reports/SliceReportJson）的用例。
int RunReportJsonCases();

/// 支撑放置策略与统计三件套（support/SliceSupportGeneration）的用例。
int RunSupportStatsCases();

/// 四个在多来源之间定优先级的取值入口（materials / preview / progress）的用例。
int RunPolicyResolverCases();

/// 枚举翻译、配置与生效之分、报告 schema（materials / support / reports）的用例。
int RunClosureAndReportCases();
