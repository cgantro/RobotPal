#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

// Small, latest-frame-wins queue for the TCP video sender. Frame IDs use
// serial-number arithmetic so wraparound does not make a newer frame stale.
class TcpSendQueue {
public:
    static constexpr size_t Capacity = 2;

    struct Item {
        uint32_t frameId;
        std::vector<uint8_t> data;
    };

    struct PushResult {
        bool accepted = false;
        std::optional<uint32_t> droppedFrameId;
    };

    PushResult Push(uint32_t frameId, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_latestFrameId && !IsNewer(frameId, *m_latestFrameId)) return {};

        m_latestFrameId = frameId;
        PushResult result{true, std::nullopt};
        if (m_items.size() == Capacity) {
            result.droppedFrameId = m_items.front().frameId;
            m_items.pop_front();
        }
        m_items.push_back(Item{frameId, data});
        return result;
    }

    std::optional<Item> TryPop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_items.empty()) return std::nullopt;
        Item item = std::move(m_items.front());
        m_items.pop_front();
        return item;
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_items.size();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.clear();
        m_latestFrameId.reset();
    }

private:
    static bool IsNewer(uint32_t candidate, uint32_t reference) {
        return static_cast<int32_t>(candidate - reference) > 0;
    }

    std::deque<Item> m_items;
    std::mutex m_mutex;
    std::optional<uint32_t> m_latestFrameId;
};
