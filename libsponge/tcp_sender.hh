#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN. 初始化的序号
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent. TCPSender 准备发送的 TCP报文段
    std::queue<TCPSegment> _segments_out{};
    /*存储未确认的报文段*/
    std::queue<TCPSegment> _segments_wait{};
    /*表示当前是否发送了 _syn 和 _fin 确认号*/
    bool _syn_send = false;
    bool _fin_send = false;
    /*判断计时器是否启动*/
    bool _timer_running = false;
    /*当前收到的 接收方窗口大小的值*/
    uint16_t _cur_window_size = 0;
    /*发送但未确认的字节数量*/
    uint64_t _bytes_in_flight = 0;
    /*重传次数*/
    int _consecutive_retransmissions = 0;
    /*记录过去的时间*/
    size_t _time_passed = 0;
    //! retransmission timer for the connection . 初始化的报文重传时间
    unsigned int _initial_retransmission_timeout;
    /*当前的过期时间*/
    unsigned int _retransmission_timer;

    //! outgoing stream of bytes that have not yet been sent . 可靠的字节流
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent . 下一个需要发送的字节的 绝对序列号
    uint64_t _next_seqno{0};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received 接收方 发回的确认号和 发送发当前的窗口大小
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments) 发送一个数据载荷部分为空的报文段
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible 创造一个报文填充足够的窗口大小
    void fill_window();

    void _send_segment(TCPSegment& tcpSegment);

    //! \brief Notifies the TCPSender of the passage of time 通知TCPSender 当前的时间
    bool tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte 有多少个已经发送但是还没有被接收的字节的大小
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row 返回重传次数，具体针对哪一个数据段还需要测试
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
