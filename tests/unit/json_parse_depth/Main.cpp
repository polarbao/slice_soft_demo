// P0FIX / P0-01：JSON 解析递归深度上限负例。
// 目的不是测「深嵌套会报错」，而是测「深嵌套【不会把进程带走】」——
// 栈溢出在 Windows 上是 EXCEPTION_STACK_OVERFLOW，catch(...) 接不住，
// 一旦发生就是装载本模块的宿主打印软件整个消失。所以判据是
// 「抛出可捕获的 std::exception」，不是「返回某个值」。

#include "slicer_core/json_value.h"

#include <cstddef>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

namespace {

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

std::string NestedArray(const std::size_t depth)
{
    return std::string(depth, '[') + std::string(depth, ']');
}

std::string NestedObject(const std::size_t depth)
{
    std::string text;
    for (std::size_t i = 0; i < depth; ++i)
    {
        text += "{\"k\":";
    }
    text += "1";
    for (std::size_t i = 0; i < depth; ++i)
    {
        text += "}";
    }
    return text;
}

// 交替对象/数组，总深度为 2 * pairs。
std::string NestedMixed(const std::size_t pairs)
{
    std::string text;
    for (std::size_t i = 0; i < pairs; ++i)
    {
        text += "{\"k\":[";
    }
    text += "1";
    for (std::size_t i = 0; i < pairs; ++i)
    {
        text += "]}";
    }
    return text;
}

// 返回 true 表示解析抛出了【可捕获的】异常；false 表示解析成功返回。
// 若实现是栈溢出，进程会在此直接终止，测试拿不到返回值——那正是要防的结果。
bool ParseThrows(const std::string& text)
{
    try
    {
        std::istringstream input{text};
        (void)slicer_core::Json::parse(input);
        return false;
    }
    catch (const std::exception&)
    {
        return true;
    }
}

}  // namespace

int main()
{
    bool passed = true;

    // 1. 正常浅层输入不受影响。
    passed = ExpectTrue(!ParseThrows(R"({"a":[1,2,{"b":null}]})"),
                 "shallow object/array still parses")
        && passed;

    // 2. 全仓 JSON 实测最深 9 层；16 层远低于上限，行为必须不变。
    passed = ExpectTrue(!ParseThrows(NestedArray(16)), "depth 16 array parses")
        && passed;
    passed = ExpectTrue(!ParseThrows(NestedMixed(8)), "depth 16 mixed object/array parses")
        && passed;

    // 3. 边界：上限 64 之内必须通过，超过必须抛。
    passed = ExpectTrue(!ParseThrows(NestedArray(64)),
                 "depth 64 is accepted (at the limit)")
        && passed;
    passed = ExpectTrue(ParseThrows(NestedArray(65)),
                 "depth 65 is rejected with a catchable exception")
        && passed;

    // 4. 对象嵌套同样计数，不能只拦数组。
    passed = ExpectTrue(!ParseThrows(NestedObject(64)), "depth 64 object is accepted")
        && passed;
    passed = ExpectTrue(ParseThrows(NestedObject(65)), "depth 65 object is rejected")
        && passed;

    // 5. 攻击面本体：远超栈容量的嵌套必须是可捕获错误，而不是进程消失。
    passed = ExpectTrue(ParseThrows(NestedArray(200000U)),
                 "200000-deep nesting is a catchable error, not a stack overflow")
        && passed;

    // 6. 深度必须随退出【递减】：一万个各自只有 3 层的兄弟节点远低于上限，
    //    必须全部通过。只增不减的实现会在这里误触上限。
    {
        std::string siblings = "[";
        for (std::size_t i = 0; i < 10000U; ++i)
        {
            if (i != 0U)
            {
                siblings += ",";
            }
            siblings += "[[1]]";
        }
        siblings += "]";
        passed = ExpectTrue(!ParseThrows(siblings),
                     "10000 shallow siblings do not accumulate depth")
            && passed;
    }

    if (!passed)
    {
        std::cerr << "json parse depth guard: FAIL\n";
        return 1;
    }
    std::cout << "json parse depth guard: PASS\n";
    return 0;
}
