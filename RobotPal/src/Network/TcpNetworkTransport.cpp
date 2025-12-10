#include "RobotPal/Network/TcpNetworkTransport.h"
#include "RobotPal/Network/TcpSendAll.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include "RobotPal/Util/Benchmark.h"
#ifndef __EMSCRIPTEN__
namespace {
uint32_t BenchmarkFrameId(const std::vector<uint8_t>& packet) {
    static const uint8_t magic[8] = {'R','P','B','E','N','C','H','1'};
    if (packet.size() < 24 || std::memcmp(packet.data() + 4, magic, 8) != 0) return 0;
    uint32_t id = 0;
    for (int i = 0; i < 4; ++i) id |= static_cast<uint32_t>(packet[12 + i]) << (i * 8);
    return id;
}
}
TcpNetworkTransport::TcpNetworkTransport()
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

TcpNetworkTransport::~TcpNetworkTransport()
{
    Disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TcpNetworkTransport::Connect(const std::string& url)
{
    if (m_IsConnected) return true;

    // URL 파싱 (예: "127.0.0.1:9998")
    std::string ip = url;
    int port = 9998; // 기본 포트
    size_t colonPos = url.find(':');
    if (colonPos != std::string::npos) {
        ip = url.substr(0, colonPos);
        port = std::stoi(url.substr(colonPos + 1));
    }

    // 소켓 생성
    m_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_Socket == INVALID_SOCKET) {
        std::cerr << "[TCP] Failed to create socket\n";
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    // 연결 시도 (Blocking)
    if (connect(m_Socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[TCP] Connection Failed: " << url << "\n";
        CleanupSocket();
        return false;
    }

    std::cout << "[TCP] Connected to " << url << "\n";
    m_IsConnected = true;
    m_IsRunning = true;

    m_RecvThread = std::thread(&TcpNetworkTransport::RecvWorker, this);
    m_SendThread = std::thread(&TcpNetworkTransport::SendWorker, this);

    return true;
}

void TcpNetworkTransport::Disconnect()
{
    if (!m_IsRunning) return;

    m_IsRunning = false;
    m_IsConnected = false;

    // 소켓을 닫으면 recv()가 에러를 리턴하며 RecvWorker가 종료됨
    CleanupSocket();

    if (m_RecvThread.joinable()) m_RecvThread.join();
    if (m_SendThread.joinable()) m_SendThread.join();

    // 큐 비우기
    m_SendQueue.Clear();
    std::cout << "[TCP] Disconnected\n";
}

void TcpNetworkTransport::Send(const std::vector<uint8_t>& data)
{
    auto frameId = BenchmarkFrameId(data);
    if (frameId == 0) frameId = m_UnframedSequence.fetch_add(1, std::memory_order_relaxed);
    Send(data, frameId);
}

void TcpNetworkTransport::Send(const std::vector<uint8_t>& data, uint32_t frameId)
{
    if (!m_IsConnected) return;
    auto result = m_SendQueue.Push(frameId, data);
    if (!result.accepted) {
        robotpal::benchmark::Event("transport_stale_dropped", frameId, 0, m_SendQueue.Size(), "older_frame");
        return;
    }
    if (result.droppedFrameId) {
        robotpal::benchmark::Event("transport_queue_dropped", *result.droppedFrameId, 0, m_SendQueue.Size(), "latest_frame_wins_capacity_2");
    }
    robotpal::benchmark::Event("transport_queued", frameId, 0, m_SendQueue.Size());
}

// --- Background Threads ---

void TcpNetworkTransport::RecvWorker()
{
    // 4KB 버퍼
    const int BUF_SIZE = 4096;
    std::vector<uint8_t> buffer(BUF_SIZE);

    while (m_IsRunning)
    {
        // recv는 데이터가 올 때까지 Blocking 됨 (CPU 소모 없음)
        // Disconnect()에서 소켓을 닫으면 SOCKET_ERROR 리턴하며 깨어남
        int bytesReceived = recv(m_Socket, (char*)buffer.data(), BUF_SIZE, 0);

        if (bytesReceived > 0)
        {
            // 실제 받은 크기만큼 잘라서 전달
            std::vector<uint8_t> packet(buffer.begin(), buffer.begin() + bytesReceived);
            
            // [부모 클래스 메서드 호출] 엔진으로 데이터 Push
            TriggerRecv(std::move(packet));
        }
        else if (bytesReceived == 0)
        {
            std::cout << "[TCP] Server closed connection.\n";
            m_IsConnected = false;
            break;
        }
        else
        {
            // 에러 혹은 소켓 닫힘
            if (m_IsRunning) {
                std::cerr << "[TCP] Recv Error\n";
            }
            break;
        }
    }
}

void TcpNetworkTransport::SendWorker()
{
    while (m_IsRunning)
    {
        // 큐에서 패킷 꺼내기
        auto packetOpt = m_SendQueue.TryPop();

        if (packetOpt.has_value())
        {
            const auto& data = packetOpt->data;
            const auto frameId = packetOpt->frameId;
            const auto sendStart = robotpal::benchmark::SteadyNowNs();
            const auto sendResult = TcpSendAll(data.data(), data.size(), [this](const char* bytes, int length) {
                if (!m_IsRunning) return 0;
                int flags = 0;
#ifdef MSG_NOSIGNAL
                flags |= MSG_NOSIGNAL;
#endif
                return send(m_Socket, bytes, length, flags);
            });
            const auto sendNs = robotpal::benchmark::SteadyNowNs() - sendStart;

            if (sendResult.status != TcpSendStatus::Complete) {
                const char* reason = sendResult.status == TcpSendStatus::SocketError ? "socket_error" :
                                     (m_IsRunning ? "zero_byte_send" : "shutdown");
                robotpal::benchmark::Event("send_failed", frameId, sendNs, sendResult.bytesSent, reason);
                if (m_IsRunning) std::cerr << "[TCP] Send Error\n";
                m_IsConnected = false;
                break;
            } else {
                robotpal::benchmark::Event("send_completed", frameId, sendNs, sendResult.bytesSent);
            }
        }
        else
        {
            // 보낼 데이터가 없으면 살짝 잠들어서 CPU 점유율 낮춤
            // (NetworkQueue에 Condition Variable이 있다면 대기하는 게 더 좋음)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void TcpNetworkTransport::CleanupSocket()
{
    if (m_Socket != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(m_Socket);
#else
        close(m_Socket);
#endif
        m_Socket = INVALID_SOCKET;
    }
}
#endif
