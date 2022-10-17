#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
    // 先需要有一个初始数据段，代表ISN
    TCPSegment segment;
    TCPHeader &header = segment.header();
    header.syn = true;
    header.seqno = _isn;
    _next_seqno = 1;
    _segments_out.push(segment);
    outstanding_segments_out_.push(segment);
    bytes_in_flight_++;
}

// 已发送但未被确认的大小
uint64_t TCPSender::bytes_in_flight() const { return bytes_in_flight_; }

void TCPSender::fill_window() {
    //    TCPSender请求填充window，他读取来自input ByteStream并且尽可能的send足够多的封装在TCPSegments的bytes，
    //    只要有新的bytes可供读到，并且window有多余空间
    //    你必须确保每个发送的TCPSegment完全适配receiver的window。确保任何一个segment尽可能大，
    //    记住SYN和FIN也会占用senos，这意味着他们占用空间
    //    你可以使用TCPSegment::length_in_sequence_space()函数来计算总的被一个segment占用的seqnos。 syn和fin同样也算

    // 判断_stream是否已经读完，读完了就要再加一个fin数据段
    // 未被确认的size < windowsize
    if (_stream.eof() && bytes_in_flight() < _window_size) {
        TCPSegment fin_segment;
        TCPHeader &fin_header = fin_segment.header();
        fin_header.fin = true;
        _segments_out.push(fin_segment);
        outstanding_segments_out_.push(fin_segment);
        bytes_in_flight_++;
        return;
    }

    if (_stream.buffer_size() == 0) {
        return;
    }
    // 填充TCPSegment： 需要填充两个：  1.TCPHeader  2.Buffer,按照窗口大小尽可能填充
    TCPSegment segment;
    // header和buffer引用传递
    TCPHeader &header = segment.header();
    Buffer &buffer = segment.payload();
    // 填充buffer
    string max_str = _stream.read(_window_size);
    uint64_t len = max_str.size();
    Buffer temp_buffer(std::move(max_str));
    // 移动构造
    buffer = std::move(temp_buffer);
    // 填充header
    header.seqno = wrap(_next_seqno, _isn);
    _next_seqno += len;
    header.win = _window_size;
    // 如果这次刚好传完，那就需要设定fin为true
    bytes_in_flight_ += len;
    header.fin = _stream.eof() && bytes_in_flight() < _window_size;
    // 加入数据段
    _segments_out.push(segment);
    outstanding_segments_out_.push(segment);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;

    while (!outstanding_segments_out_.empty()) {
        TCPSegment segment = outstanding_segments_out_.front();
        WrappingInt32 seqno = segment.header().seqno;
        uint64_t abseqno = unwrap(seqno, _isn, _stream.bytes_read());
        if (abseqno + segment.length_in_sequence_space() - 1 >= unwrap(ackno, _isn, _stream.bytes_read())) {
            // 再往后就是未被确认的segment了
            break;
        }
        bytes_in_flight_ -= segment.length_in_sequence_space();
        outstanding_segments_out_.pop();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
