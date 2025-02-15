#include "tcp_connection.hh"

#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <thread>
#include <util/logPoint.cc>
// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return time_last_segment_received_; }

void TCPConnection::fill_window(bool should_reply) {
    _sender.fill_window();

    send_segments(should_reply);
}

void TCPConnection::fill_queue(std::queue<TCPSegment> &segments_out) {
    TCPSegment tmp = segments_out.front();
    segments_out.pop();
    if (_receiver.ackno().has_value()) {
        tmp.header().ack = true;
        tmp.header().ackno = _receiver.ackno().value();
    }
    tmp.header().rst = rst_;
    size_t window_size = _receiver.window_size();
    tmp.header().win = window_size < std::numeric_limits<uint16_t>::max() ? static_cast<uint16_t>(window_size)
                                                                          : std::numeric_limits<uint16_t>::max();
    _segments_out.push(tmp);
}

void TCPConnection::send_segments(bool should_reply) {
    stringstream ss;
    std::queue<TCPSegment> &segments_out = _sender.segments_out();
    if (segments_out.empty() && should_reply) {
        ss << this_thread::get_id();
        lprintf(to_string(getpid()).c_str());
        lprintf("当前segments_out为空\n");
        _sender.send_empty_segment();
    }
    if (rst_) {
        lprintf(to_string(getpid()).c_str());
        lprintf("出现rst，发送rst报文\n");
        fill_queue(segments_out);
        return;
    }
    while (!segments_out.empty()) {
        lprintf(to_string(getpid()).c_str());
        lprintf("发送非空报文\n");
        fill_queue(segments_out);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    stringstream ss;
    lprintf(ss.str().c_str());
    lprintf("调用segment_received\n");

    if (!active_) {
        return;
    }

    // 初始化时间
    time_last_segment_received_ = 0;

    if (seg.header().rst) {
        // RST segments without ACKs should be ignored in SYN_SENT
        if (state_ == TCPState::State::SYN_SENT && !seg.header().ack) {
            return;
        }
        lprintf("在ack中准备调用set_rst\n");
        set_rst();
        return;
    }

    _receiver.segment_received(seg);

    // 三次握手阶段
    if (state_ == TCPState::State::SYN_SENT) {
        // 状态转移            state_ = TCPState::State::FIN_WAIT_2;
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().syn && !seg.header().ack) {
            ss << this_thread::get_id();
            lprintf(ss.str().c_str());
            lprintf("经过状态SYN_SENT,转移至SYN_RCVD\t");
            state_ = TCPState::State::SYN_RCVD;
            fill_window(seg.length_in_sequence_space());
        } else {
            ss << this_thread::get_id();
            lprintf(ss.str().c_str());
            lprintf("经过状态SYN_SENT,转移至ESTABLISHED\t");
            state_ = TCPState::State::ESTABLISHED;
            fill_window(seg.length_in_sequence_space());
        }

    } else if (state_ == TCPState::State::LISTEN) {
        ss << this_thread::get_id();
        lprintf(ss.str().c_str());
        lprintf("经过状态LISTEN\n");
        if (seg.header().syn) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            fill_window(seg.length_in_sequence_space());
            // 状态转移
            ss << this_thread::get_id();
            lprintf(ss.str().c_str());
            lprintf("经过LISTEN,转移至SYN_RCVD\n");
            state_ = TCPState::State::SYN_RCVD;
        }

    } else if (state_ == TCPState::State::SYN_RCVD) {
        ss << this_thread::get_id();
        lprintf(ss.str().c_str());
        lprintf("经过状态SYN_RCVD\n");
        if (seg.header().rst) {
            lprintf(ss.str().c_str());
            lprintf("经过状态SYN_RCVD，转移至LISTEN\n");
            state_ = TCPState::State::LISTEN;
        } else if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            lprintf(ss.str().c_str());
            lprintf("经过状态SYN_RCVD转移至ESTABLISHED\n");
            state_ = TCPState::State::ESTABLISHED;
        }

    } else if (state_ == TCPState::State::ESTABLISHED) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        fill_window(seg.length_in_sequence_space());
        ss << this_thread::get_id();
        lprintf(ss.str().c_str());
        lprintf("当前状态ESTABLISHED，已经确认了该报文\n");
        if (seg.header().fin) {
            _linger_after_streams_finish = false;
            lprintf(ss.str().c_str());
            lprintf("经过状态ESTABLISHED,转移至CLOSE_WAIT\n");
            state_ = TCPState::State::CLOSE_WAIT;
        }

    } else if (state_ == TCPState::State::FIN_WAIT_1) {
        // 收到对于fin的确认报文段
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().ack && _sender.bytes_in_flight() == 0 && seg.header().fin) {
            lprintf(ss.str().c_str());
            lprintf("经过状态FIN_WAIT_1,转移至TIME_WAIT\n");
            state_ = TCPState::State::TIME_WAIT;
            fill_window(seg.length_in_sequence_space());
        } else if (seg.header().ack && _sender.bytes_in_flight() == 0) {
            lprintf(ss.str().c_str());
            lprintf("经过状态FIN_WAIT_1,转移至FIN_WAIT_2\n");
            state_ = TCPState::State::FIN_WAIT_2;
        }  // 收到对方发来的fin，直接CLOSING
        else if (seg.header().fin) {
            lprintf(ss.str().c_str());
            lprintf("经过状态FIN_WAIT_1,转移至CLOSING\n");
            state_ = TCPState::State::CLOSING;
            fill_window(seg.length_in_sequence_space());
        }

    } else if (state_ == TCPState::State::FIN_WAIT_2) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().fin) {
            send_segments(seg.length_in_sequence_space());
            state_ = TCPState::State::TIME_WAIT;
        } else {
            _sender.send_empty_segment();
            TCPSegment segment = _sender.segments_out().front();
            if (_receiver.ackno().has_value()) {
                segment.header().ack = true;
                segment.header().ackno = _receiver.ackno().value();
            }
            segment.header().rst = rst_;
            segment.header().win = _receiver.window_size();
            _segments_out.push(segment);
            _sender.segments_out().pop();
        }

    } else if (state_ == TCPState::State::CLOSING) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().ack && _sender.bytes_in_flight() == 0) {
            state_ = TCPState::State::TIME_WAIT;
            time_wait_start_ = 0;
        }

    } else if (state_ == TCPState::State::CLOSE_WAIT) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        fill_window(seg.length_in_sequence_space());

    } else if (state_ == TCPState::State::LAST_ACK) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        
        if (seg.header().ack && _sender.bytes_in_flight() == 0) {
            active_ = false;
            _linger_after_streams_finish = false;
            state_ = TCPState::State::CLOSED;
        }

    } else if (state_ == TCPState::State::TIME_WAIT) {
        if (seg.header().fin) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _sender.send_empty_segment();
            TCPSegment segment = _sender.segments_out().front();
            if (_receiver.ackno().has_value()) {
                segment.header().ack = true;
                segment.header().ackno = _receiver.ackno().value();
            }
            segment.header().rst = rst_;
            segment.header().win = _receiver.window_size();
            _segments_out.push(segment);
            _sender.segments_out().pop();
        }
    }
    test_end();
}

bool TCPConnection::active() const {
    if (state_ == TCPState::State::CLOSED || rst_ ||
        (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
         TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
         time_since_last_segment_received() >= 10 * _cfg.rt_timeout)) {
        return false;
    }
    return active_;
}

size_t TCPConnection::write(const string &data) {
    // 先写入_sender里面的bytestream
    stringstream ss;
    size_t write_len = _sender.stream_in().write(std::move(data));
    fill_window(true);
    test_end();
    return write_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (state_ == TCPState::State::LISTEN) {
        return;
    }
    time_last_segment_received_ += ms_since_last_tick;

    stringstream ss;
    ss << this_thread::get_id();
    lprintf(ss.str().c_str());
    lprintf("调用tick(");
    lprintf(to_string(ms_since_last_tick).c_str());
    lprintf(")\n");

    if (state_ == TCPState::State::TIME_WAIT) {
        time_wait_start_ += ms_since_last_tick;
        if (time_wait_start_ >= TCPConfig::TIMEOUT_DFLT * 10) {
            lprintf(ss.str().c_str());
            lprintf("time_wait状态过渡到closed");
            active_ = false;
            _linger_after_streams_finish = false;
            state_ = TCPState::State::CLOSED;
            return;
        }
    }

    if (_sender.tick(ms_since_last_tick)) {
        if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
            lprintf(ss.str().c_str());
            lprintf("当前状态：");
            lprintf(state_.name().c_str());
            lprintf("超时重传次数过多，设置rst\n");
            set_rst();
            return;
        }
        send_segments(true);
    }

    test_end();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    /*存在一种情况，窗口已经满了，fin实际上是不能马上传出去的*/

    if (state_ == TCPState::State::ESTABLISHED || state_ == TCPState::State::CLOSE_WAIT)
        fill_window(false);
    // 主动关闭
    if (state_ == TCPState::State::ESTABLISHED && TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT) {
        state_ = TCPState::State::FIN_WAIT_1;
        return;
    }
    // 被动关闭
    if (state_ == TCPState::State::CLOSE_WAIT && TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT) {
        state_ = TCPState::State::LAST_ACK;
    }

    test_end();
}

void TCPConnection::connect() {
    // Initiate a connection by sending a SYN segment.
    if (_sender.next_seqno_absolute() != 0) {
        return;
    }
    stringstream ss;
    lprintf("\n\n\n\n\n");
    ss << this_thread::get_id();
    lprintf(ss.str().c_str());
    lprintf("请求connect\n");
    active_ = true;
    // 三次握手
    _sender.fill_window();
    // 发送syn报文段
    send_segments(true);
    // 当前状态转移
    state_ = TCPState::State::SYN_SENT;
}

void TCPConnection::test_end() {
    if (_receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof()) {
            _linger_after_streams_finish = false;
        }

        else if (_sender.bytes_in_flight() == 0) {
            if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                active_ = false;
            }
        }
    }

    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof() && _sender.next_seqno_absolute() > 0) {
        _linger_after_streams_finish = false;
    } else if (_receiver.stream_out().eof() && _sender.stream_in().eof() && unassembled_bytes() == 0 &&
               bytes_in_flight() == 0 && TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_SENT) {
        if (!_linger_after_streams_finish)
            active_ = false;
        else if (time_last_segment_received_ >= 10 * _cfg.rt_timeout)
            active_ = false;
    }
}

void TCPConnection::set_rst() {
    active_ = false;
    rst_ = true;
    _linger_after_streams_finish = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    lprintf("检测状态");
    lprintf(state().name().c_str());
    if (state_ != TCPState::State::LISTEN && state_ != TCPState::State::SYN_SENT &&
        state_ != TCPState::State::SYN_RCVD) {
        lprintf(state_.name().c_str());
        lprintf("rst状态，准备发送数据段\n");
        send_segments(true);
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            lprintf("unclean shutdown\n");
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            set_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
