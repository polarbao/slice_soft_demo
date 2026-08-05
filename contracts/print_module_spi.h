/**
 * @file print_module_spi.h
 * @brief 打印软件预处理模块服务提供者接口（SPI）。
 *
 * 所有预处理能力模块（切片、RIP、未来扩展）通过实现本接口接入宿主 PrintApp。
 *
 * ── 契约版本 ──
 *   调用契约：PM_SPI_VERSION（本文件）
 *   数据契约：各接缝 schema 字符串（p0.rgbwsv.2 / rip.ch7.1 / printdata.v1）
 *   两者独立版本化。
 *
 * ── ABI 不变量（违反即崩，无例外）──
 *   1. 跨边界只出现 C 基本类型与 const char* / char*。禁止任何结构体、
 *      STL 容器、C++ 类、Qt 类型、函数对象跨越 DLL 边界。
 *   2. 所有字符串 UTF-8 编码。缓冲区由调用方分配（见 §缓冲区协议）。
 *   3. 句柄不透明。创建与释放必须成对，且必须由同一个 DLL 完成。
 *   4. 任何导出函数都不得让异常逃逸。内部异常必须转为错误码。
 *   5. 运行时必须一致：MSVC x64，Release 用 /MD，Debug 用 /MDd。
 *      模块在 pm_module_info 中自述 runtime 与 buildConfig，宿主校验，
 *      不一致则拒绝装载。
 *   6. pm_module_t 允许多线程并发调用；单个 pm_job_t 只允许由一个线程操作。
 *
 * ── 缓冲区协议（全部 char* 出参函数统一遵守）──
 *   参数：(char* out, int cap, int* out_required)
 *
 *   探测长度：out == NULL 或 cap == 0
 *       → 返回 PM_ERR_BUFFER_SMALL，*out_required 置为所需字节数（不含结尾 NUL）
 *   缓冲足够：cap >= 所需字节数 + 1
 *       → 写入 UTF-8 内容并追加 '\0'
 *       → 返回写入的字节数（不含 NUL），*out_required 同值
 *   缓冲不足：0 < cap < 所需字节数 + 1
 *       → 不写入任何内容（out 保持不变，不做部分写）
 *       → 返回 PM_ERR_BUFFER_SMALL，*out_required 置为所需字节数（不含 NUL）
 *
 *   out_required 允许为 NULL（调用方不关心长度时）。
 *   返回值 >= 0 恒表示成功。
 */
#ifndef PRINT_MODULE_SPI_H
#define PRINT_MODULE_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 版本 ==================== */

#define PM_SPI_VERSION 1

/* ==================== 导出与调用约定 ==================== */

#if defined(PM_MODULE_STATIC)
#  define PM_API
#elif defined(PM_MODULE_BUILD_SHARED)
#  define PM_API __declspec(dllexport)
#else
#  define PM_API __declspec(dllimport)
#endif

#define PM_CALL __cdecl

/* ==================== 返回码 ==================== */

#define PM_OK                 0   /**< 成功（无长度语义的函数） */
#define PM_ERR_FAILED       (-1)  /**< 通用失败，详情见 pm_last_error / pm_result */
#define PM_ERR_BUFFER_SMALL (-2)  /**< 缓冲不足，见 out_required */
#define PM_ERR_INVALID_ARG  (-3)  /**< 空句柄或必需参数为 NULL */
#define PM_ERR_INVALID_STATE (-4) /**< 当前状态不允许该操作（如作业未终结即取结果） */

/* ==================== 不透明句柄 ==================== */

typedef struct pm_module_s pm_module_t;   /**< 模块实例 */
typedef struct pm_job_s    pm_job_t;      /**< 作业实例 */

/* ==================== 版本与能力自述（无需 pm_create）==================== */

/**
 * @brief 返回模块实现的 SPI 版本。
 * @return 必须为编译期的 PM_SPI_VERSION。宿主不匹配则拒绝装载。
 * @note 本函数必须在 pm_create 之前可调用，且不得有任何副作用。
 */
PM_API int PM_CALL pm_spi_version(void);

/**
 * @brief 输出模块能力自述 JSON。
 * @note 必须在 pm_create 之前可调用（宿主据此决定是否装载）。
 * @note 返回内容必须是常量，同一进程内多次调用结果一致。
 */
PM_API int PM_CALL pm_module_info(char* json_out, int cap, int* out_required);

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建模块实例。
 * @param options_json 模块级初始化选项，UTF-8。允许为 NULL（全用默认值）。
 *                     schema 见 CLD_10 §5。
 * @return 模块句柄；失败返回 NULL，原因通过 pm_last_error 获取。
 * @note 允许同一进程内创建多个实例，实例之间必须完全隔离。
 * @note 不得在本函数内启动业务线程池以外的任何全局副作用。
 */
PM_API pm_module_t* PM_CALL pm_create(const char* options_json);

/**
 * @brief 销毁模块实例。
 * @note 调用前调用方必须已对该实例下全部作业调用过 pm_release。
 *       若仍有未释放作业，实现必须先取消并等待其结束，不得泄漏或崩溃。
 * @note 传入 NULL 是合法的空操作。
 */
PM_API void PM_CALL pm_destroy(pm_module_t* module);

/* ==================== 作业 ==================== */

/**
 * @brief 提交作业。立即返回，不阻塞。
 * @param request_json 作业请求，UTF-8。schema 见 CLD_06 §5。
 * @return 作业句柄；受理失败返回 NULL，原因通过 pm_last_error 获取。
 * @note 受理成功不代表作业会成功，只代表参数合法且已入队。
 * @note 并发上限见 pm_module_info 的 capabilities.maxConcurrentJobs。
 *       超限时返回 NULL 且 pm_last_error 报 PM-<MOD>-RESOURCE-0041。
 */
PM_API pm_job_t* PM_CALL pm_submit(pm_module_t* module, const char* request_json);

/**
 * @brief 轮询作业进度。
 * @note 建议轮询间隔 200-500ms。本函数必须是廉价的（不得每次重算进度）。
 * @note progress.percent 必须单调不回退。
 * @note 允许在任意状态下调用，包括终结后（返回最后一次快照）。
 */
PM_API int PM_CALL pm_poll(pm_job_t* job, char* progress_json, int cap, int* out_required);

/**
 * @brief 请求取消作业（协作式）。
 * @return PM_OK 表示取消请求已受理（幂等：重复调用仍返回 PM_OK）。
 * @note 受理后 state 立即变为 "cancelling"，并在
 *       capabilities.cancelLatencyMs 内变为 "cancelled"。
 * @note 取消后必须删除全部 .staging 临时目录，且不得覆盖上一次有效产物。
 * @note 对已终结的作业调用返回 PM_OK（空操作）。
 */
PM_API int PM_CALL pm_cancel(pm_job_t* job);

/**
 * @brief 读取作业最终结果。
 * @return 状态非终结（queued/running/cancelling）时返回 PM_ERR_INVALID_STATE。
 * @note 终结状态为 succeeded / failed / cancelled 三者之一。
 */
PM_API int PM_CALL pm_result(pm_job_t* job, char* result_json, int cap, int* out_required);

/**
 * @brief 释放作业句柄。
 * @note 若作业仍在运行，实现必须先取消并等待结束再释放，不得直接杀线程。
 * @note 传入 NULL 是合法的空操作。
 */
PM_API void PM_CALL pm_release(pm_job_t* job);

/* ==================== 诊断 ==================== */

/**
 * @brief 模块自检。
 * @note 用于装载后健康检查与诊断包导出。必须不产生任何持久化副作用。
 */
PM_API int PM_CALL pm_self_test(pm_module_t* module, char* report_json, int cap, int* out_required);

/**
 * @brief 读取本线程最近一次失败的详情。
 * @note 线程局部存储。用于 pm_create / pm_submit 返回 NULL 时定位原因。
 * @note 返回 JSON：{ "code": "PM-...", "message": "...", "detail": "..." }
 * @note 成功调用后不清除；下一次失败覆盖。
 */
PM_API int PM_CALL pm_last_error(char* json_out, int cap, int* out_required);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* PRINT_MODULE_SPI_H */
