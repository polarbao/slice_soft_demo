#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>

namespace slicer_core
{

/**
 * @brief 多实例按 global layer 对齐的同步屏障（MF-05 步骤 1）。
 *
 * 现状是 `MultiModelProductionService` 把**全部实例切完**才一起合成，而单实例
 * raster 持有全部层 —— 用户 10um 场景每实例 103.7 GiB，双模型 207 GiB，直接内存
 * 不足。但合成第 L 层只需各实例的第 L 层，`ownedlayercallback` 本就是逐层交付，
 * 障碍只是 instance-major 切片与 layer-major 合成的错位。
 *
 * 本类**不持有任何层数据**，只做同步：调用方各自持有自己的单层槽，写完槽再
 * `DepositAndWait`。这样屏障与载荷类型无关，可独立单测，也不会把 pipeline 类型
 * 拖进同步逻辑。峰值由此降为 O(实例数 x 列数)，与层数无关。
 *
 * 层数不齐由「全局层号」自然处理：某实例第一层落在全局第 5 层时，它在消费者
 * 处理第 0~4 层期间只是 pending 于 5，既不贡献也不阻塞。
 *
 * 线程模型：每实例一个生产者线程，一个消费者线程。全部方法线程安全。
 */
class SceneLayerBarrier
{
public:
    explicit SceneLayerBarrier(std::size_t instanceCount);

    /**
     * @brief 生产者：声明本实例已把第 `globalLayerIndex` 层写入自己的槽，并等待消费。
     * @return false 表示屏障已失效（取消或某实例异常），生产者应立即退出，
     *         **不得**继续产层 —— 这条是防死锁的关键。
     */
    bool DepositAndWait(std::size_t instance, int globalLayerIndex);

    /// 生产者：声明本实例不再有任何层。
    void Finish(std::size_t instance);

    /**
     * @brief 消费者：等到本层所有实例就绪（已 deposit 到本层、已 Finish、或 pending 在更后的层）。
     * @return 本层有内容的实例序号（**按实例序升序**，不按线程完成先后 —— 否则输出 hash 会抖）；
     *         `nullopt` 表示全部实例已结束或屏障已失效。
     */
    std::optional<std::vector<std::size_t>> AwaitLayer(int globalLayerIndex);

    /// 消费者：本层消费完毕，放行停在本层的生产者。
    void ReleaseLayer(int globalLayerIndex);

    /// 使屏障失效并唤醒所有等待者。取消与异常传播共用此入口。
    void Fail();

    [[nodiscard]] bool Failed() const;

private:
    struct InstanceState
    {
        int pendingLayer{-1};
        bool finished{false};
    };

    mutable std::mutex m_mutex;
    std::condition_variable m_producerReady;
    std::condition_variable m_consumerReleased;
    std::vector<InstanceState> m_instances;
    bool m_failed{false};
};

}  // namespace slicer_core
