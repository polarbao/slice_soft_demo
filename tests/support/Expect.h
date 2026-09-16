#pragma once

// 共享测试断言与用例跑法。一个头文件解决两件事：
//
//   F-17 断言失败不报原因。仓内约 85 处 ExpectTrue(cond, "文字") 在失败时只打印那句
//        文字，不打印任何实参——「expected 2048 kept lines」这种消息无法告诉你实际是多少。
//        本头的宏打印表达式原文与双方实参。
//
//   F-44 未捕获异常不是快速失败而是【挂死】。实测本仓 191 个含 main() 的测试源文件中
//        99 个（52%）完全没有 catch；这些二进制遇未捕获 C++ 异常时被 Windows 错误报告
//        接管，停在那里直到 ctest 超时。一个坏 fixture 的代价因此不是「快速红一次」，
//        而是吃满一次超时（本仓部分项 180 s）。RunCases 逐例 try/catch 并继续下一例。
//
// 边界（必须知道）：
//   - catch(...) 接不住 Windows 结构化异常。访问违例与 EXCEPTION_STACK_OVERFLOW
//     仍会带走进程，本头只解决 C++ 异常这一类。
//   - 每条 RUN 用 std::endl 强制刷新。万一真的被 SEH 带走，日志里最后一条 RUN
//     就是罪魁用例——不刷新的话缓冲区会一起丢掉，连这点线索都没有。
//   - 断言失败【不抛异常】，用例会跑完并报告全部失败，而不是停在第一条。

#include <cstddef>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>

namespace slicesoft_test
{
namespace detail
{

inline int& FailureCount()
{
    static int value{0};
    return value;
}

// bool 打成 true/false 而不是 1/0——后者在「expected true, actual 0」里几乎没有信息量。
inline std::string Describe(const bool value)
{
    return value ? "true" : "false";
}

template <typename T>
std::string Describe(const T& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

// 失败信息写 stderr（沿用仓内既有约定），但写之前必须先刷 stdout：
// 两个流各自缓冲，不刷的话 ASSERT/FAIL 会整体排到收尾汇总行【之后】，
// 与它所属的 RUN 行错位，在 ctest 日志里几乎无法对应回具体用例。
inline void Report(const char* const file, const int line, const std::string& detail)
{
    ++FailureCount();
    std::cout.flush();
    std::cerr << "    ASSERT " << file << ':' << line << "  " << detail << std::endl;
}

}  // namespace detail

/// @brief 一条具名用例。
struct Case
{
    const char* name;
    std::function<void()> body;
};

/// @brief 逐例运行并捕获异常，返回进程退出码（0 = 全通过）。
/// @param suite 套件名，出现在收尾汇总行里。
/// @param cases 用例表。
/// @return 全部通过返回 0，否则返回 1。
inline int RunCases(const char* const suite, std::initializer_list<Case> cases)
{
    int failed{0};
    for (const Case& item : cases)
    {
        // endl 而非 '\n'：强制刷新，使被 SEH 带走时日志仍能指认是哪一例。
        std::cout << "RUN " << item.name << std::endl;
        detail::FailureCount() = 0;

        std::string thrown;
        try
        {
            item.body();
        }
        catch (const std::exception& error)
        {
            thrown = error.what();
        }
        catch (...)
        {
            thrown = "<non-std exception>";
        }

        if (!thrown.empty())
        {
            std::cout.flush();
            std::cerr << "FAIL " << item.name << " exception=" << thrown << std::endl;
            ++failed;
            continue;
        }
        if (detail::FailureCount() > 0)
        {
            std::cout.flush();
            std::cerr << "FAIL " << item.name << ' ' << detail::FailureCount()
                      << " assertion(s)" << std::endl;
            ++failed;
            continue;
        }
        std::cout << "PASS " << item.name << '\n';
    }

    std::cout << suite << ": " << (failed == 0 ? "PASS" : "FAIL") << " ("
              << cases.size() << " case(s), " << failed << " failed)" << std::endl;
    return failed == 0 ? 0 : 1;
}

/// @brief 最小保护：把既有 main 的函数体包进 try/catch，**不改任何断言与用例结构**。
///
/// `RunCases` 是完整迁移（同时买到 F-17 的实参打印与 F-44 的不挂死），但它要求把用例
/// 拆成具名条目——那属于「改写用例」，`AGENTS.md` PC-12 记录该类批量改写试过并回退。
/// 本函数只解决 F-44：给整个 main 套一层捕获，使未捕获异常变成快速失败而非挂住等
/// ctest 超时。改动粒度是【两行】，不触碰任何断言，因此不在 PC-12 的范围内。
///
/// 用法：把原 `int main(...)` 改名为 `RunGuardedBody(...)`，在文件末尾追加
/// @code
/// int main() { return slicesoft_test::GuardedMain("suite name", RunGuardedBody); }
/// @endcode
///
/// 边界同 RunCases：`catch(...)` 接不住 Windows 结构化异常。
inline int GuardedMain(const char* const suite, const std::function<int()>& body)
{
    try
    {
        return body();
    }
    catch (const std::exception& error)
    {
        std::cout.flush();
        std::cerr << "FAIL " << suite << " uncaught exception=" << error.what()
                  << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cout.flush();
        std::cerr << "FAIL " << suite << " uncaught <non-std exception>" << std::endl;
        return 1;
    }
}

}  // namespace slicesoft_test

// do/while(false) 包裹：使宏在 if/else 里作为单条语句安全展开。
#define SLICESOFT_EXPECT_TRUE(condition, message)                                  \
    do                                                                             \
    {                                                                              \
        if (!(condition))                                                          \
        {                                                                          \
            ::slicesoft_test::detail::Report(                                      \
                __FILE__, __LINE__,                                                \
                std::string{(message)} + "  |  expected true: " + #condition);     \
        }                                                                          \
    } while (false)

#define SLICESOFT_EXPECT_FALSE(condition, message)                                 \
    do                                                                             \
    {                                                                              \
        if ((condition))                                                           \
        {                                                                          \
            ::slicesoft_test::detail::Report(                                      \
                __FILE__, __LINE__,                                                \
                std::string{(message)} + "  |  expected false: " + #condition);    \
        }                                                                          \
    } while (false)

// 与 EXPECT_TRUE(a == b) 的差别就在这里：失败时打印两侧的【实际值】。
#define SLICESOFT_EXPECT_EQ(actual, expected, message)                             \
    do                                                                             \
    {                                                                              \
        const auto& slicesoftActual = (actual);                                    \
        const auto& slicesoftExpected = (expected);                                \
        if (!(slicesoftActual == slicesoftExpected))                               \
        {                                                                          \
            ::slicesoft_test::detail::Report(                                      \
                __FILE__, __LINE__,                                                \
                std::string{(message)} + "  |  " + #actual + " = "                 \
                    + ::slicesoft_test::detail::Describe(slicesoftActual)          \
                    + ", expected " + #expected + " = "                            \
                    + ::slicesoft_test::detail::Describe(slicesoftExpected));      \
        }                                                                          \
    } while (false)

#define SLICESOFT_EXPECT_NE(actual, unexpected, message)                           \
    do                                                                             \
    {                                                                              \
        const auto& slicesoftActual = (actual);                                    \
        const auto& slicesoftUnexpected = (unexpected);                            \
        if (slicesoftActual == slicesoftUnexpected)                                \
        {                                                                          \
            ::slicesoft_test::detail::Report(                                      \
                __FILE__, __LINE__,                                                \
                std::string{(message)} + "  |  " + #actual + " = "                 \
                    + ::slicesoft_test::detail::Describe(slicesoftActual)          \
                    + ", expected it to differ from " + #unexpected);              \
        }                                                                          \
    } while (false)

/// 无条件失败，用于「不该走到这里」的分支。
#define SLICESOFT_FAIL(message)                                                    \
    ::slicesoft_test::detail::Report(__FILE__, __LINE__, std::string{(message)})
