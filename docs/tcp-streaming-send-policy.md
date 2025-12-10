# TCP video send policy

The native TCP transport keeps at most two completed video packets waiting to send. When a newer frame arrives at capacity, it evicts the oldest queued frame. It also rejects a packet whose frame ID is older than the newest frame already accepted, which prevents JPEG workers that finish out of order from putting stale frames behind newer ones. Frame IDs use 32-bit serial-number ordering and support wraparound.

The send worker retries partial `send()` results until the packet is fully written. A socket error or a zero-byte result marks the connection disconnected and records a failed send with the number of bytes already written. A packet already being written cannot be replaced midway; the latest-frame policy applies to queued packets.

`tests/tcp_send_queue_test.cpp` exercises the bounded queue, stale-frame rejection, frame ID wraparound, partial-send retries, zero-byte sends, and socket errors. Compile it as a C++17 test with `RobotPal/include` on the include path.
