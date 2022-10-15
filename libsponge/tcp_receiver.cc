#include "tcp_receiver.hh"

#include <wrapping_integers.hh>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    //如果header中有syn flag
    if (header.syn) {
        //标记ISN
        init_isn_ = true;
        ISN = header.seqno;
    }

    bool is_fin = header.fin;
    init_fin_ = is_fin;
    //最后一次write的pos
    size_t checkpoint = _reassembler.start_pos() - 1;
    //解除包装
    uint32_t stream_index = (header.syn ? 0 : -1) + unwrap(header.seqno, ISN, checkpoint);
    // push data
    if (init_isn_) {
        _reassembler.push_substring(seg.payload().copy(), stream_index, is_fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!init_isn_) {
        return std::optional<WrappingInt32>{};
    }
    if (init_fin_) {
        return wrap(_reassembler.start_pos() + 1, ISN) + 1;
    }
    return wrap(_reassembler.start_pos() + 1, ISN);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
