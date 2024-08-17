#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + static_cast<uint32_t>(n); }

uint64_t findClosest(uint64_t a, uint64_t b, uint64_t c, uint64_t target) {
    // 计算每个数与 target 之间的差，确保差值为正
    uint64_t diff_a = (a > target) ? (a - target) : (target - a);
    uint64_t diff_b = (b > target) ? (b - target) : (target - b);
    uint64_t diff_c = (c > target) ? (c - target) : (target - c);

    // 比较差值，找到最小的那个
    if (diff_a <= diff_b && diff_a <= diff_c) {
        return a;
    } else if (diff_b <= diff_a && diff_b <= diff_c) {
        return b;
    } else {
        return c;
    }
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t base1 = (checkpoint / (1ul << 32)) * (1ul << 32);
    uint64_t base2 = (checkpoint / (1ul << 32) + 1) * (1ul << 32);
    uint64_t base3 = (checkpoint / (1ul << 32) - 1) * (1ul << 32);
    uint64_t res1, res2, res3;
    if (n - isn < 0) {
        res1 = base1 + (1ul << 32) - static_cast<uint64_t>(-(n - isn));
        res2 = base2 + (1ul << 32) - static_cast<uint64_t>(-(n - isn));
        res3 = base3 + (1ul << 32) - static_cast<uint64_t>(-(n - isn));
    } else {
        res1 = base1 + static_cast<uint64_t>(n - isn);
        res2 = base2 + static_cast<uint64_t>(n - isn);
        res3 = base3 + static_cast<uint64_t>(n - isn);
    }
    return findClosest(res1, res2, res3, checkpoint);
}
