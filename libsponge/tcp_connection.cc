#include "tcp_connection.hh"

#include <iostream>
#include <limits>
#include <sys/time.h>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _receiver.stream_out().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return time_last_segment_received_; }

void TCPConnection::fill_window() {
    _sender.fill_window();

    send_segments();
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

void TCPConnection::send_segments() {
    std::queue<TCPSegment> &segments_out = _sender.segments_out();
    if (segments_out.empty()) {
        _sender.send_empty_segment();
    }
    if (rst_) {
        fill_queue(segments_out);
        return;
    }
    while (!segments_out.empty()) {
        fill_queue(segments_out);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active()) {
        return;
    }

    // 初始化时间
    time_last_segment_received_ = 0;

    if (seg.header().rst) {
        set_rst();
    }

    _receiver.segment_received(seg);

    // 三次握手阶段
    if (state_ == TCPState::State::SYN_SENT) {
        // 状态转移            state_ = TCPState::State::FIN_WAIT_2;
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().syn && !seg.header().ack) {
            state_ = TCPState::State::SYN_RCVD;
            fill_window();
        } else {
            state_ = TCPState::State::ESTABLISHED;
            fill_window();
        }

    } else if (state_ == TCPState::State::LISTEN) {
        if (seg.header().syn) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            fill_window();
            // 状态转移
            state_ = TCPState::State::SYN_RCVD;
        }

    } else if (state_ == TCPState::State::SYN_RCVD) {
        if (seg.header().rst) {
            state_ = TCPState::State::LISTEN;
        }
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            state_ = TCPState::State::ESTABLISHED;
        }

    } else if (state_ == TCPState::State::ESTABLISHED) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        fill_window();
        if (seg.header().fin) {
            _linger_after_streams_finish = false;
            state_ = TCPState::State::CLOSE_WAIT;
        }

    } else if (state_ == TCPState::State::FIN_WAIT_1) {
        // 收到对于fin的确认报文段
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().ack && _sender.bytes_in_flight() == 0 && seg.header().fin) {
            state_ = TCPState::State::TIME_WAIT;
            fill_window();
        } else if (seg.header().ack && _sender.bytes_in_flight() == 0) {
            state_ = TCPState::State::FIN_WAIT_2;
        }  // 收到对方发来的fin，直接CLOSING
        else if (seg.header().fin) {
            state_ = TCPState::State::CLOSING;
            fill_window();
        }

    } else if (state_ == TCPState::State::FIN_WAIT_2) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().fin) {
            send_segments();
            state_ = TCPState::State::TIME_WAIT;
        } else {
            _sender.send_empty_segment();
            TCPSegment segment = _sender.segments_out().front();
            segment.header().win = _receiver.window_size();
            segment.header().ackno = _receiver.ackno().value();
            _segments_out.push(segment);
            _sender.segments_out().pop();
        }

    } else if (state_ == TCPState::State::CLOSING) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().ack && _sender.bytes_in_flight() == 0) {
            state_ = TCPState::State::TIME_WAIT;
            time_wait_start_ = 0;
        } else {
            fill_window();
        }

    } else if (state_ == TCPState::State::CLOSE_WAIT) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        fill_window();

    } else if (state_ == TCPState::State::LAST_ACK) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.header().ack && _sender.bytes_in_flight() == 0) {
            active_ = false;
            _linger_after_streams_finish = false;
            state_ = TCPState::State::CLOSED;
        } else {
            fill_window();
        }
    } else if (state_ == TCPState::State::TIME_WAIT) {
        if (seg.header().fin) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _sender.send_empty_segment();
            TCPSegment segment = _sender.segments_out().front();
            segment.header().win = _receiver.window_size();
            segment.header().ackno = _receiver.ackno().value();
            segment.header().ack = 1;
            _segments_out.push(_sender.segments_out().front());
            _sender.segments_out().pop();
        } else {
            fill_window();
        }
    }
    test_end();
}

bool TCPConnection::active() const { return active_; }

size_t TCPConnection::write(const string &data) {
    size_t len = data.size();
    // 先写入_sender里面的bytestream
    size_t write_len = _sender.stream_in().write(std::move(data));
    // 判断是否写完
    if (write_len == len) {
        _sender.stream_in().end_input();
    }
    fill_window();
    test_end();
    return write_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    time_last_segment_received_ += ms_since_last_tick;

    if (state_ == TCPState::State::TIME_WAIT) {
        time_wait_start_ += ms_since_last_tick;
        if (time_wait_start_ >= TCPConfig::TIMEOUT_DFLT * 10) {
            active_ = false;
            _linger_after_streams_finish = false;
            state_ = TCPState::State::CLOSED;
        }
    }

    if (_sender.tick(ms_since_last_tick)) {
        if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
            set_rst();
            return;
        }
        send_segments();
    }

    test_end();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();

    if (state_ == TCPState::State::ESTABLISHED) {
        fill_window();
        state_ = TCPState::State::FIN_WAIT_1;
    } else if (state_ == TCPState::State::CLOSE_WAIT) {
        fill_window();
        state_ = TCPState::State::LAST_ACK;
    } else if (state_ == TCPState::State::LISTEN) {
        state_ = TCPState::State::CLOSED;
    } else if (state_ == TCPState::State::SYN_SENT) {
        state_ = TCPState::State::CLOSED;
    }

    test_end();
}

void TCPConnection::connect() {
    // Initiate a connection by sending a SYN segment.
    active_ = true;
    // 三次握手
    _sender.fill_window();
    // 发送syn报文段
    send_segments();
    // 当前状态转移
    state_ = TCPState::State::SYN_SENT;
}

void TCPConnection::test_end() {
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof() && _sender.next_seqno_absolute() > 0) {
        _linger_after_streams_finish = false;
    } else if (_receiver.stream_out().eof() && _sender.stream_in().eof() && unassembled_bytes() == 0 &&
               bytes_in_flight() == 0 && state_ == TCPState::State::CLOSE_WAIT) {
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
    if (state_ == TCPState::State::ESTABLISHED) {
        send_segments();
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active() || state_ == TCPState::State::CLOSED) {
            set_rst();
            active_ = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
