#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

enum class TcpSendStatus { Complete, SocketError, ZeroBytes };

struct TcpSendResult {
    TcpSendStatus status;
    size_t bytesSent;
};

// Send callback has the same result contract as the platform send() call:
// positive bytes, zero for a closed send, and a negative value for an error.
template <typename SendFn>
TcpSendResult TcpSendAll(const uint8_t* data, size_t size, SendFn sendFn) {
    size_t offset = 0;
    while (offset < size) {
        const auto remaining = size - offset;
        const int chunk = static_cast<int>(std::min(remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
        const int sent = sendFn(reinterpret_cast<const char*>(data) + offset, chunk);
        if (sent < 0) return {TcpSendStatus::SocketError, offset};
        if (sent == 0) return {TcpSendStatus::ZeroBytes, offset};
        if (static_cast<size_t>(sent) > remaining) return {TcpSendStatus::SocketError, offset};
        offset += static_cast<size_t>(sent);
    }
    return {TcpSendStatus::Complete, offset};
}
