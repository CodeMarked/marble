#include "gameplay/ReliableChannel.hpp"

#include <array>
#include <cstdint>
#include <cstring>

int main() {
    using namespace marble::gameplay;

    // --- sequenceAfter ---
    if (!sequenceAfter(2u, 1u)) {
        return 1;
    }
    if (sequenceAfter(1u, 2u)) {
        return 2;
    }
    // wrap: 1 is after 65535 (distance 2 forward)
    if (!sequenceAfter(1u, 65535u)) {
        return 3;
    }
    // wrap reverse: 65535 is NOT after 1 (would be 65534 forward, > half space)
    if (sequenceAfter(65535u, 1u)) {
        return 4;
    }

    // --- basic sequence allocation ---
    ReliableChannel<4, 64> ch{};
    if (ch.nextSequence() != 1u) {
        return 5;
    }
    if (ch.nextSequence() != 2u) {
        return 6;
    }

    // --- recordOutgoing + activeCount ---
    std::array<std::uint8_t, 16> msg{};
    msg[0] = 0xAA;
    if (!ch.recordOutgoing(1u, msg.data(), msg.size(), 10u)) {
        return 7;
    }
    if (ch.activeCount() != 1u) {
        return 8;
    }

    // --- processAcks retires acked entry ---
    ch.processAcks(1u, 0u);
    if (ch.activeCount() != 0u) {
        return 9;
    }

    // --- ack bitmap retirement ---
    static_cast<void>(ch.recordOutgoing(3u, msg.data(), msg.size(), 20u));
    static_cast<void>(ch.recordOutgoing(4u, msg.data(), msg.size(), 21u));
    static_cast<void>(ch.recordOutgoing(5u, msg.data(), msg.size(), 22u));
    if (ch.activeCount() != 3u) {
        return 10;
    }
    // ackSeq=5, ackBits bit0=1 (seq 4 acked), bit1=1 (seq 3 acked)
    ch.processAcks(5u, 0x03u);
    if (ch.activeCount() != 0u) {
        return 11;
    }

    // --- processIncomingSequence dedup ---
    ch.reset();
    if (!ch.processIncomingSequence(1u)) {
        return 12;
    }
    if (ch.ackSequence() != 1u) {
        return 13;
    }
    // duplicate
    if (ch.processIncomingSequence(1u)) {
        return 14;
    }
    // advance
    if (!ch.processIncomingSequence(2u)) {
        return 15;
    }
    if (ch.ackSequence() != 2u) {
        return 16;
    }
    // bitmap: bit 0 should be set (seq 1 received)
    if ((ch.ackBitmap() & 1u) == 0u) {
        return 17;
    }

    // --- out-of-order incoming ---
    ch.reset();
    static_cast<void>(ch.processIncomingSequence(1u));
    static_cast<void>(ch.processIncomingSequence(3u));
    // ackSequence=3, bitmap bit1 = 1 (seq 1), bit0 = 0 (seq 2 missing)
    if (ch.ackSequence() != 3u) {
        return 18;
    }
    if ((ch.ackBitmap() & 0x01u) != 0u) {
        return 19; // seq 2 was NOT received
    }
    if ((ch.ackBitmap() & 0x02u) == 0u) {
        return 20; // seq 1 WAS received
    }
    // late arrival of seq 2
    if (!ch.processIncomingSequence(2u)) {
        return 21;
    }
    if ((ch.ackBitmap() & 0x01u) == 0u) {
        return 22; // now seq 2 should be marked
    }

    // --- sequence wrap in incoming ---
    ch.reset();
    static_cast<void>(ch.processIncomingSequence(65534u));
    static_cast<void>(ch.processIncomingSequence(65535u));
    static_cast<void>(ch.processIncomingSequence(1u));
    if (ch.ackSequence() != 1u) {
        return 23;
    }

    // --- pendingRetransmit ---
    ch.reset();
    static_cast<void>(ch.recordOutgoing(ch.nextSequence(), msg.data(), msg.size(), 100u));
    // too early: currentTick=102, rtt=6 -> age 2 < 6
    if (ch.pendingRetransmit(102u, 6u) != nullptr) {
        return 24;
    }
    // ready: currentTick=107, rtt=6 -> age 7 >= 6
    auto* entry = ch.pendingRetransmit(107u, 6u);
    if (entry == nullptr) {
        return 25;
    }
    if (entry->sequence != 1u) {
        return 26;
    }

    // --- markResent updates tick ---
    ch.markResent(1u, 107u);
    if (ch.pendingRetransmit(108u, 6u) != nullptr) {
        return 27; // age 1 < 6 after resent
    }

    // --- retransmit buffer overflow evicts oldest ---
    ch.reset();
    for (std::uint32_t i = 0; i < 4u; ++i) {
        static_cast<void>(ch.recordOutgoing(ch.nextSequence(), msg.data(), msg.size(), 200u + i));
    }
    if (ch.activeCount() != 4u) {
        return 28;
    }
    // buffer full (capacity 4), next record evicts oldest
    static_cast<void>(ch.recordOutgoing(ch.nextSequence(), msg.data(), msg.size(), 210u));
    if (ch.activeCount() != 4u) {
        return 29; // still 4 (one evicted, one added)
    }

    // --- reject null/zero-length for recordOutgoing ---
    if (ch.recordOutgoing(99u, nullptr, 10u, 0u)) {
        return 30;
    }
    if (ch.recordOutgoing(99u, msg.data(), 0u, 0u)) {
        return 31;
    }

    return 0;
}
