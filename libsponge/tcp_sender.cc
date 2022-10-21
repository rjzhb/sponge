#include "tcp_sender.hh"

#include "tcp_config.hh"

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
    , _retransmission_timer(retx_timeout)
    , _stream(capacity) {}

// TCP 有没有可能对报文的一部分进行确认, 本次lab 先默认不存在这种情况, 后续根据实际情况进行修改
/*syn 和 fin 也占据一个字节*/
void TCPSender::_send_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    // length_in_sequence_space计算segment的载荷部分和SYN和FIN的长度
    _next_seqno += seg.length_in_sequence_space();
    // 更新发送窗口中的字节数量
    _bytes_in_flight += seg.length_in_sequence_space();
    // 将segments发送出去，加入发送窗口
    _segments_out.push(seg);
    _segments_wait.push(seg);
    // 当发送一个segment时，如果计时器没有启动，就启动一个计时器
    if (!_timer_running) {
        _timer_running = true;
        _time_passed = 0;
    }
}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // 判断是否发送了 syn 同步报文段
    if (!_syn_send) {
        TCPSegment tcpSegment{};
        tcpSegment.header().syn = true;
        _syn_send = true;
        _send_segment(tcpSegment);
        return;
    }
    // 如果 syn 同步报文还没有确认，那么直接返回
    if (!_segments_wait.empty() && _segments_wait.front().header().syn)
        return;
    // 如果已经发送了 fin 终止报文段，那么就不再进行发送
    if (_fin_send) {
        return;
    }

    uint16_t cur_window_size = _cur_window_size == 0 ? 1 : _cur_window_size;
    uint16_t remaining_size = (cur_window_size >= bytes_in_flight() ? cur_window_size - bytes_in_flight() : 0);

    if (_stream.eof() && remaining_size >= 1) {
        TCPSegment tcpSegment{};
        tcpSegment.header().fin = true;
        _fin_send = true;
        _send_segment(tcpSegment);
        return;
    }

    while (remaining_size > 0 && !_stream.buffer_empty()) {
        uint16_t size;
        size = (remaining_size > TCPConfig::MAX_PAYLOAD_SIZE ? TCPConfig::MAX_PAYLOAD_SIZE : remaining_size);
        TCPSegment tcpSegment{};
        tcpSegment.payload() = _stream.read(size > _stream.buffer_size() ? _stream.buffer_size() : size);

        // 如果到结尾了，当前报文段 还留有字节，那么加上 fin
        if (_stream.eof() && tcpSegment.length_in_sequence_space() < remaining_size) {
            tcpSegment.header().fin = true;
            _fin_send = true;
        }
        _send_segment(tcpSegment);
        remaining_size -= tcpSegment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 得到绝对 下标
    uint64_t ab_sn = unwrap(ackno, _isn, _stream.bytes_read());
    // 合法性 判断
    if (ab_sn > _next_seqno) {
        return;
    }
    _cur_window_size = window_size;

    while (!_segments_wait.empty()) {
        TCPSegment front = _segments_wait.front();
        if (unwrap(front.header().seqno, _isn, _stream.bytes_read()) + front.length_in_sequence_space() <= ab_sn) {
            // 更新 _bytes_in_flight
            _bytes_in_flight -= front.length_in_sequence_space();

            _segments_wait.pop();

            // 重置 最大过期时间
            _retransmission_timer = _initial_retransmission_timeout;
            // 重置 计时器时间
            _time_passed = 0;
            // 重置重传次数
            _consecutive_retransmissions = 0;

        } else
            break;
    }
    if (_segments_wait.empty()) {
        _timer_running = false;
        _time_passed = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
bool TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_running) {
        return false;
    }

    bool is_modify = false;

    _time_passed += ms_since_last_tick;
    if (_time_passed >= _retransmission_timer) {
        // 超时重传
        _segments_out.push(_segments_wait.front());
        is_modify = true;
        if (_cur_window_size != 0 || _segments_wait.front().header().syn) {
            // 重传 + 1
            _consecutive_retransmissions++;
            _retransmission_timer *= 2;
        }
        // 计时器 归零
        _time_passed = 0;
    }
    return is_modify;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

/*发送空报文，没有数据载荷，也不是fin和 syn报文 所以直接发送*/
void TCPSender::send_empty_segment() {
    // 即使是空报文段也是需要一个有效序列号的
    TCPSegment seg{};
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
