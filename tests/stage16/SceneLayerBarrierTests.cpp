#include "slicer_core/pipeline/SceneLayerBarrier.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

using slicer_core::SceneLayerBarrier;

bool ExpectTrue(const bool condition, const std::string& what)
{
    if (!condition)
    {
        std::cout << "FAIL " << what << "\n";
    }
    return condition;
}

/**
 * @brief 合成顺序必须按实例序，而非线程完成先后。
 *
 * 这条是输出确定性的根，故让生产者故意错开起跑顺序（后面的实例先跑），
 * 断言每层拿到的就绪集合恒为升序。
 */
bool ReadySetIsOrderedByInstanceNotByThread()
{
    constexpr std::size_t instanceCount{4};
    constexpr int layerCount{16};
    SceneLayerBarrier barrier(instanceCount);
    std::vector<std::thread> producers;
    for (std::size_t index{0}; index < instanceCount; ++index)
    {
        producers.emplace_back([&barrier, index] {
            // 序号越大起跑越早，制造与实例序相反的完成倾向。
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(instanceCount - index)));
            for (int layer{0}; layer < layerCount; ++layer)
            {
                if (!barrier.DepositAndWait(index, layer))
                {
                    return;
                }
            }
            barrier.Finish(index);
        });
    }

    bool passed{true};
    for (int layer{0}; layer < layerCount; ++layer)
    {
        const auto ready = barrier.AwaitLayer(layer);
        passed = ExpectTrue(ready.has_value(), "layer " + std::to_string(layer) + " has a ready set")
            && passed;
        if (!ready.has_value())
        {
            break;
        }
        passed = ExpectTrue(ready->size() == instanceCount, "every instance contributes") && passed;
        for (std::size_t position{0}; position < ready->size(); ++position)
        {
            passed = ExpectTrue(ready->at(position) == position, "ready set is instance-ordered")
                && passed;
        }
        barrier.ReleaseLayer(layer);
    }
    for (std::thread& producer : producers)
    {
        producer.join();
    }
    return passed;
}

/**
 * @brief 层数不齐：某实例的层落在全局第 2~3 层，其余层它既不贡献也不阻塞。
 */
bool UnevenLayerCountsDoNotBlock()
{
    SceneLayerBarrier barrier(2U);
    std::thread wide([&barrier] {
        for (int layer{0}; layer < 5; ++layer)
        {
            if (!barrier.DepositAndWait(0U, layer))
            {
                return;
            }
        }
        barrier.Finish(0U);
    });
    std::thread narrow([&barrier] {
        for (int layer{2}; layer <= 3; ++layer)
        {
            if (!barrier.DepositAndWait(1U, layer))
            {
                return;
            }
        }
        barrier.Finish(1U);
    });

    bool passed{true};
    const std::vector<std::size_t> expectedSizes{1U, 1U, 2U, 2U, 1U};
    for (int layer{0}; layer < 5; ++layer)
    {
        const auto ready = barrier.AwaitLayer(layer);
        passed = ExpectTrue(ready.has_value(), "uneven layer " + std::to_string(layer)) && passed;
        if (!ready.has_value())
        {
            break;
        }
        passed = ExpectTrue(
            ready->size() == expectedSizes.at(static_cast<std::size_t>(layer)),
            "uneven layer " + std::to_string(layer) + " contributor count") && passed;
        barrier.ReleaseLayer(layer);
    }
    wide.join();
    narrow.join();
    return passed;
}

/**
 * @brief 失效必须唤醒【全部】等待者，且生产者据返回值退出而不再产层。
 *
 * 这条防的是死锁：只要有一个生产者停在 DepositAndWait 上不醒，整个作业就挂住。
 */
bool FailWakesEveryWaiter()
{
    SceneLayerBarrier barrier(3U);
    std::atomic<int> exited{0};
    std::vector<std::thread> producers;
    for (std::size_t index{0}; index < 3U; ++index)
    {
        producers.emplace_back([&barrier, &exited, index] {
            for (int layer{0}; layer < 1000; ++layer)
            {
                if (!barrier.DepositAndWait(index, layer))
                {
                    ++exited;
                    return;
                }
            }
            barrier.Finish(index);
        });
    }
    // 先正常放行一层，确认三者都已进入等待，再令屏障失效。
    const auto ready = barrier.AwaitLayer(0);
    bool passed = ExpectTrue(ready.has_value() && ready->size() == 3U, "all three deposited");
    barrier.Fail();
    for (std::thread& producer : producers)
    {
        producer.join();
    }
    passed = ExpectTrue(exited.load() == 3, "every producer observed the failure") && passed;
    passed = ExpectTrue(barrier.Failed(), "barrier reports failure") && passed;
    passed = ExpectTrue(!barrier.AwaitLayer(1).has_value(), "consumer stops after failure") && passed;
    return passed;
}

}  // namespace

int main()
{
    bool passed{true};
    passed = ReadySetIsOrderedByInstanceNotByThread() && passed;
    passed = UnevenLayerCountsDoNotBlock() && passed;
    passed = FailWakesEveryWaiter() && passed;
    if (passed)
    {
        std::cout << "PASS MF-05 scene layer barrier tests\n";
        return 0;
    }
    return 1;
}
