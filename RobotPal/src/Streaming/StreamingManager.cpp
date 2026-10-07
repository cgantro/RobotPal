#include "RobotPal/Streaming/StreamingManager.h"

#include "RobotPal/Components/Components.h"
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/Profiling.h"
#include "stb_image_write.h"

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

#include <utility>

namespace {
constexpr std::size_t kEncodeWorkerCount = 4;
}

StreamingManager::StreamingManager(flecs::world& world)
    : m_World(world)
{
    auto& handle = m_World.get_mut<NetworkEngineHandle>();
    m_NetworkEngine = handle.instance;
}

StreamingManager::~StreamingManager() {
    Shutdown();
}

void StreamingManager::Init() {
    bool expected = false;
    if (!m_Running.compare_exchange_strong(expected, true)) {
        return;
    }

    m_EncodeWorkers.reserve(kEncodeWorkerCount);
    for (std::size_t i = 0; i < kEncodeWorkerCount; ++i) {
        m_EncodeWorkers.emplace_back(&StreamingManager::EncodeWorkerLoop, this);
    }
}

void StreamingManager::Shutdown() {
    if (!m_Running.exchange(false)) {
        return;
    }

    m_QueueCv.notify_all();

    for (auto& worker : m_EncodeWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_EncodeWorkers.clear();

    std::lock_guard<std::mutex> lock(m_QueueMutex);
    while (!m_EncodeQueue.empty()) {
        m_EncodeQueue.pop();
    }
}

void StreamingManager::SendFrame(const FrameData& frame) {
    RP_PROFILE_SCOPE("Streaming.Enqueue");

    if (!m_Running.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_EncodeQueue.push(frame);
    }
    m_QueueCv.notify_one();
}

void StreamingManager::EncodeWorkerLoop() {
    RP_PROFILE_THREAD("Streaming JPEG Worker");

    while (true) {
        FrameData frame;

        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            m_QueueCv.wait(lock, [this] {
                return !m_EncodeQueue.empty() || !m_Running.load();
            });

            if (!m_Running.load() && m_EncodeQueue.empty()) {
                return;
            }

            frame = std::move(m_EncodeQueue.front());
            m_EncodeQueue.pop();
        }

        RP_PROFILE_SCOPE("Streaming.WorkerJob");

        WriteContext ctx;
        ctx.buffer.reserve(static_cast<size_t>(frame.width) * frame.height);

        int ok = 0;
        {
            RP_PROFILE_SCOPE("Streaming.JPEG");
            ok = stbi_write_jpg_to_func(
                write_func,
                &ctx,
                frame.width,
                frame.height,
                frame.channels,
                frame.pixel_data.data(),
                85
            );
        }

        if (!ok || ctx.buffer.empty() || !m_NetworkEngine) {
            continue;
        }

        std::vector<uint8_t> packet;
        const uint32_t sizeBe = htonl(static_cast<uint32_t>(ctx.buffer.size()));
        const auto* sizePtr = reinterpret_cast<const uint8_t*>(&sizeBe);

        packet.insert(packet.end(), sizePtr, sizePtr + 4);
        packet.insert(packet.end(), ctx.buffer.begin(), ctx.buffer.end());

        m_NetworkEngine->SendPacket(packet);
    }
}

void StreamingManager::write_func(void* ctx, void* data, int size) {
    auto* output = static_cast<WriteContext*>(ctx);
    auto* begin = static_cast<uint8_t*>(data);
    output->buffer.insert(output->buffer.end(), begin, begin + size);
}
