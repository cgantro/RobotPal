#include "RobotPal/Network/TcpSendQueue.h"
#include "RobotPal/Network/TcpSendAll.h"

#include <algorithm>
#include <iostream>

int main() {
    const uint8_t bytes[] = {1, 2, 3, 4, 5};
    size_t acceptedBytes = 0;
    const auto partial = TcpSendAll(bytes, sizeof(bytes), [&acceptedBytes](const char*, int requested) {
        const int accepted = std::min(2, requested);
        acceptedBytes += static_cast<size_t>(accepted);
        return accepted;
    });
    if (partial.status != TcpSendStatus::Complete || partial.bytesSent != sizeof(bytes) || acceptedBytes != sizeof(bytes)) {
        std::cerr << "send helper should retry until every byte is sent\n";
        return 1;
    }
    const auto zero = TcpSendAll(bytes, sizeof(bytes), [](const char*, int) { return 0; });
    if (zero.status != TcpSendStatus::ZeroBytes || zero.bytesSent != 0) {
        std::cerr << "send helper should report a zero-byte send\n";
        return 1;
    }
    size_t calls = 0;
    const auto error = TcpSendAll(bytes, sizeof(bytes), [&calls](const char*, int) {
        return calls++ == 0 ? 2 : -1;
    });
    if (error.status != TcpSendStatus::SocketError || error.bytesSent != 2) {
        std::cerr << "send helper should report errors after preserving the sent-byte count\n";
        return 1;
    }

    TcpSendQueue queue;
    if (queue.Push(10, {10}).accepted != true ||
        queue.Push(11, {11}).accepted != true ||
        queue.Size() != TcpSendQueue::Capacity) {
        std::cerr << "queue should accept ordered frames up to its capacity\n";
        return 1;
    }

    const auto replacement = queue.Push(12, {12});
    if (!replacement.accepted || replacement.droppedFrameId != 10 || queue.Size() != 2) {
        std::cerr << "queue should evict its oldest frame at capacity\n";
        return 1;
    }
    const auto stale = queue.Push(11, {99});
    if (stale.accepted || queue.Size() != 2) {
        std::cerr << "queue should reject out-of-order completed frames\n";
        return 1;
    }

    auto first = queue.TryPop();
    auto second = queue.TryPop();
    if (!first || !second || first->frameId != 11 || second->frameId != 12) {
        std::cerr << "queue should preserve order among retained frames\n";
        return 1;
    }

    queue.Clear();
    if (!queue.Push(0xfffffffeu, {1}).accepted || !queue.Push(1, {2}).accepted) {
        std::cerr << "frame ID comparison should allow wraparound\n";
        return 1;
    }
    std::cout << "tcp_send_queue_test: all checks passed\n";
    return 0;
}
