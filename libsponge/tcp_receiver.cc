#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

/*接收TCP报文，然后还原*/
void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    //保证收到第一个报文段为 请求同步报文
    if(!_syn && !header.syn) return;
    if(_fin && header.fin) return;
    //如果已经收到请求同步报文 不接收其他同步请求报文
    if(_syn && header.syn) return;
    //当前是同步报文
    if(header.syn){
        _syn = true;
        _isn = seg.header().seqno.raw_value();
        //同步请求报文和终止报文在同一个 报文段
        if(header.fin){
            _fin = true;
        }
        _reassembler.push_substring(seg.payload().copy(),0,header.fin);
        return;
    }

    uint64_t a_sno = unwrap(header.seqno, WrappingInt32(_isn), _reassembler.stream_out()._write_cnt);
    if(header.fin) _fin = true;
    _reassembler.push_substring(seg.payload().copy(), a_sno - 1, header.fin);

}
/*返回当前的确认号字段*/
optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_syn) return {};
    uint32_t res = wrap(_reassembler.stream_out().bytes_written(), WrappingInt32(_isn)).raw_value();
    if(_reassembler.stream_out().input_ended()) res ++ ;
    return WrappingInt32(res + 1);
}
/*返回当前的剩余端口*/
size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
