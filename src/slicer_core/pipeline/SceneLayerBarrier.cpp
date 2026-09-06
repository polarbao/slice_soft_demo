#include "slicer_core/pipeline/SceneLayerBarrier.h"

#include <stdexcept>

namespace slicer_core
{

SceneLayerBarrier::SceneLayerBarrier(const std::size_t instanceCount)
    : m_instances(instanceCount)
{
    if (instanceCount == 0U)
    {
        throw std::invalid_argument("SceneLayerBarrier requires at least one instance");
    }
}

bool SceneLayerBarrier::DepositAndWait(
    const std::size_t instance,
    const int globalLayerIndex)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (instance >= m_instances.size())
    {
        throw std::out_of_range("SceneLayerBarrier deposit instance out of range");
    }
    InstanceState& state = m_instances.at(instance);
    if (state.finished)
    {
        throw std::logic_error("SceneLayerBarrier deposit after finish");
    }
    if (m_failed)
    {
        return false;
    }
    if (m_draining)
    {
        // 消费方已正常结束，生产者不必再等对齐 —— 直接放行让它跑完收尾。
        return true;
    }
    state.pendingLayer = globalLayerIndex;
    m_producerReady.notify_all();
    // 等到本层被消费者放行；失效或收尾时立刻返回，避免生产者永久挂住。
    m_consumerReleased.wait(lock, [this, &state] {
        return m_failed || m_draining || state.pendingLayer < 0;
    });
    return !m_failed;
}

void SceneLayerBarrier::Finish(const std::size_t instance)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (instance >= m_instances.size())
        {
            throw std::out_of_range("SceneLayerBarrier finish instance out of range");
        }
        InstanceState& state = m_instances.at(instance);
        state.finished = true;
        state.pendingLayer = -1;
    }
    m_producerReady.notify_all();
}

std::optional<std::vector<std::size_t>> SceneLayerBarrier::AwaitLayer(
    const int globalLayerIndex)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    // 就绪条件：每个实例要么已结束，要么已 deposit 到【本层或更后的层】。
    // 「更后的层」对应该实例在本层没有内容（层数不齐），既不贡献也不阻塞。
    m_producerReady.wait(lock, [this, globalLayerIndex] {
        if (m_failed)
        {
            return true;
        }
        for (const InstanceState& state : m_instances)
        {
            if (state.finished)
            {
                continue;
            }
            if (state.pendingLayer < globalLayerIndex)
            {
                return false;
            }
        }
        return true;
    });
    if (m_failed)
    {
        return std::nullopt;
    }

    std::vector<std::size_t> ready;
    bool anyAlive{false};
    for (std::size_t index{0}; index < m_instances.size(); ++index)
    {
        const InstanceState& state = m_instances.at(index);
        if (state.finished)
        {
            continue;
        }
        anyAlive = true;
        if (state.pendingLayer == globalLayerIndex)
        {
            // 按实例序追加，故返回值恒为升序 —— 合成顺序不受线程调度影响。
            ready.push_back(index);
        }
    }
    if (!anyAlive)
    {
        return std::nullopt;
    }
    return ready;
}

void SceneLayerBarrier::ReleaseLayer(const int globalLayerIndex)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (InstanceState& state : m_instances)
        {
            if (!state.finished && state.pendingLayer == globalLayerIndex)
            {
                state.pendingLayer = -1;
            }
        }
    }
    m_consumerReleased.notify_all();
}

void SceneLayerBarrier::Drain()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_draining = true;
        for (InstanceState& state : m_instances)
        {
            state.pendingLayer = -1;
        }
    }
    m_consumerReleased.notify_all();
    m_producerReady.notify_all();
}

void SceneLayerBarrier::Fail()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failed = true;
    }
    m_producerReady.notify_all();
    m_consumerReleased.notify_all();
}

bool SceneLayerBarrier::Failed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_failed;
}

}  // namespace slicer_core
