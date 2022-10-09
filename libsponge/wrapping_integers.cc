#include "wrapping_integers.hh"

uint64_t max_val = (UINT64_C(1) << 32);
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}
using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
// 输入absolute seqnos 以及 isn 转化为seqnos
// seqnos : ISN + 1(MOD 1 << 32)
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t seqnos = (isn.raw_value() + n % max_val) % (max_val);
    return WrappingInt32{seqnos};
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
    WrappingInt32 c = wrap(checkpoint, isn);
    int32_t offset = n.raw_value() - c.raw_value();
    int64_t ans = checkpoint + offset;
    return ans >= 0 ? ans : ans + (1ul << 32);
}
