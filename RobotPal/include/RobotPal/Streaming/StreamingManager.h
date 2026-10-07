#pragma once

#include "RobotPal/Streaming/IStreamingManager.h"
#include "RobotPal/Network/INetworkTransport.h"

#include <flecs.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class StreamingManager : public IStreamingManager
{
public:
    explicit StreamingManager(flecs::world& world);
    ~StreamingManager() override;

    void Init() override;
    void Shutdown() override;
    void SendFrame(const FrameData& frame) override;

private:
    struct WriteContext {
        std::vector<uint8_t> buffer;
    };

    void EncodeWorkerLoop();
    static void write_func(void* ctx, void* data, int size);

private:
    flecs::world& m_World;
    NetworkEngine* m_NetworkEngine = nullptr;

    std::atomic<bool> m_Running{false};
    std::mutex m_QueueMutex;
    std::condition_variable m_QueueCv;
    std::queue<FrameData> m_EncodeQueue;
    std::vector<std::thread> m_EncodeWorkers;
};
