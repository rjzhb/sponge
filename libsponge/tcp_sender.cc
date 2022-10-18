#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>
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
    retransmission_timeout_ = retx_timeout;
    // 先需要有一个初始数据段，代表ISN
    TCPSegment segment;
    TCPHeader &header = segment.header();
    header.syn = true;
    header.seqno = _isn;
    _next_seqno++;
    _segments_out.push(segment);
    outstanding_segments_out_.push(segment);
    bytes_in_flight_++;
}

// 已发送但未被确认的大小
uint64_t TCPSender::bytes_in_flight() const { return bytes_in_flight_; }

void TCPSender::fill_window() {
    // 判断_stream是否已经读完，读完了就要再加一个fin数据段
    // 未被确认的size < windowsize
    uint16_t truly_window_size = _window_size == 0 ? 1 : _window_size;
    // 先判断窗口大小
    if (bytes_in_flight_ >= truly_window_size) {
        return;
    }

    // 判断是否可以标志fin
    if (_stream.eof() && !ack_fin_) {
        TCPSegment fin_segment;
        TCPHeader &fin_header = fin_segment.header();
        fin_header.fin = true;
        fin_header.seqno = wrap(_next_seqno, _isn);
        _next_seqno++;
        _segments_out.push(fin_segment);
        outstanding_segments_out_.push(fin_segment);
        bytes_in_flight_++;
        ack_fin_ = true;
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
    size_t window_size = static_cast<size_t>(truly_window_size);
    size_t left_size = window_size > bytes_in_flight_ ? window_size - bytes_in_flight_ : 0;
    size_t actually_size = min(left_size, _stream.buffer_size());
    size_t read_size = min(actually_size, TCPConfig::MAX_PAYLOAD_SIZE);

    do {
        string max_str = _stream.read(read_size);

        // 应对无符号整形
        if (actually_size >= read_size) {
            actually_size -= read_size;
        } else {
            actually_size = 0;
        }

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
        header.fin = _stream.eof() && bytes_in_flight() < truly_window_size;
        if (header.fin) {
            ack_fin_ = header.fin;
        }
        // fin占用序列号
        bytes_in_flight_ += header.fin ? 1 : 0;
        _next_seqno += header.fin ? 1 : 0;
        // 加入数据段
        _segments_out.push(segment);
        outstanding_segments_out_.push(segment);
    } while (actually_size > read_size);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;

    uint64_t absolute_ackno = unwrap(ackno, _isn, _stream.bytes_read());
    // 攻击的目的，直接忽略
    if (absolute_ackno > next_seqno_absolute()) {
        return;
    }

    while (!outstanding_segments_out_.empty()) {
        TCPSegment segment = outstanding_segments_out_.front();
        WrappingInt32 seqno = segment.header().seqno;
        uint64_t abseqno = unwrap(seqno, _isn, _stream.bytes_read());
        if (abseqno + segment.length_in_sequence_space() - 1 >= absolute_ackno) {
            // 再往后就是未被确认的segment了
            break;
        }
        bytes_in_flight_ -= segment.length_in_sequence_space();
        outstanding_segments_out_.pop();
        // 初始化重传次数
        retx_times_ = 0;
        // 初始化RTO
        retransmission_timeout_ = _initial_retransmission_timeout;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 计时开始
    if (retransmission_timeout_ >= ms_since_last_tick) {
        retransmission_timeout_ -= ms_since_last_tick;
    } else {
        retransmission_timeout_ = 0;
    }

    // 超时重传
    if (retransmission_timeout_ == 0 && !outstanding_segments_out_.empty()) {
        TCPSegment outstanding_segment = outstanding_segments_out_.front();
        _segments_out.push(outstanding_segment);
        retx_times_++;
        if (_window_size || outstanding_segment.header().syn) {
            retransmission_timeout_ = (1 << retx_times_) * _initial_retransmission_timeout;
        } else {
            retransmission_timeout_ = _initial_retransmission_timeout;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return retx_times_; }

void TCPSender::send_empty_segment() {}
