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
    , _stream(capacity)
    , _next_seqno(0)
    , _rto(retx_timeout)
    , _syn(false)
    , _fin(false)
    , _window_size(0)
    , _recv_ack_seqno(0)
    , buffer()
    , _byte_size_in_fly(0)
    , _state(0)
    , _con_retrans_nums(0)
    , _time_stmp(0) 
    , _no_windows(false)
    {}

uint64_t TCPSender::bytes_in_flight() const { return _byte_size_in_fly; }

void TCPSender::fill_window() {
    state_flow();  // 先执行状态转移
    // 然后根据不同的发送状态决定发送什么样的数据
    if (_state == 0) {
        // 说明还未启动，则发送SYN,开启计时并终止
        TCPSegment segment;
        segment.header().syn = true;
        segment.header().seqno = _isn;
        _syn = true;
        _next_seqno = 1;
        _segments_out.push(segment);
        buffer.push(segment);
        _byte_size_in_fly = 1;
        _con_retrans_nums = 0;
        _time_stmp = 0;
        return;
    }
    if (_state == 1 || _state == 3 || _state == 4) {
        return;
    }
    // 只剩下最后一种_state == 2说明ack syn ，now can send more data
    // maybe with fin
    TCPSegment segment;

    // 这里的逻辑是错的，是要尽可能发送，发送很多,而不是只发一次，一直到填充满window_sizez或者buffer为空为止
    while ( _window_size > 0 && not _fin ) {
        segment.header().seqno = wrap(_next_seqno, _isn);
        size_t send_size = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);
        segment.payload() = stream_in().read(send_size);     // 这个读不会超过buffer的范围
        _window_size -= segment.length_in_sequence_space();  // 更新新的window_size
        if ( _window_size > _byte_size_in_fly && stream_in().eof()) {
            segment.header().fin = true;
            _fin = true;  
            _window_size -= 1;
        }
        if (not segment.header().fin and
            segment.length_in_sequence_space() == 0) {  // 如果没有需要发送的fin或者空白包，则直接返回
            return;
        }
        // 填充完毕数据
        _segments_out.push(segment);
        buffer.push(segment);
        _byte_size_in_fly += segment.length_in_sequence_space();
        _next_seqno += segment.length_in_sequence_space();
    }
    // 发送完，启动计时器
    _time_stmp = 0;
    _con_retrans_nums = 0;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    state_flow();
    // 如果不对，说明有问题
    if (_state < 1) {
        _rto = _initial_retransmission_timeout;
        return;
    }
    // 如果是状态1,2，3，4说明上一个已经接受，则需要将其从buffer中删除，然后将重置_rto,
    // 更新_recv_abs_seqno和_byte_size_in_fly，_con_retrans_nums
    // 收到的有可能是4种ack,第一种是带syn的，第二种是不带syn和fin，第三种是只带fin，第四种是带syn和fin的
    // 不管是哪一种，都可以判断是否是在buffer的头部，如果在，则可以弹出
    // 然后重置计时器和超时次数
    _window_size = window_size  == 0 ? 1 : window_size;
    _no_windows = window_size == 0;
    _recv_ack_seqno = unwrap(ackno, _isn, _next_seqno);
    if (_recv_ack_seqno > _next_seqno) {
        return;  // 如果收到的ack比准备发送的还大，说明收到的不对
    }
    while (!buffer.empty()) {
        TCPSegment segment = buffer.front();
        if (_recv_ack_seqno < unwrap(segment.header().seqno, _isn, _next_seqno) + segment.length_in_sequence_space()) {
            // 说明这个没有办法弹出，需要等待重发
            return;
        }
        buffer.pop();
        // _next_seqno = _recv_ack_seqno;
        _byte_size_in_fly -= segment.length_in_sequence_space();
        _rto = _initial_retransmission_timeout;
        _con_retrans_nums = 0;
        _time_stmp = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 记录当前时间戳
    _time_stmp += ms_since_last_tick;
    // 判断当前状态,如果不对则返回
    state_flow();
    if (_state == 0 || _state == 4) {
        return;
    }
    // 如果超时，则从buffer中从前面弹出,但是如果弹出的是windows=1的产物的时候，不动rto
    if (_time_stmp >= _rto && !buffer.empty()) {
        TCPSegment segment = buffer.front();
        _segments_out.push(segment);
        if(not _no_windows){
            _con_retrans_nums += 1;
            _rto *= 2;
        }
        _time_stmp = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _con_retrans_nums; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_recv_ack_seqno, _isn);
    _segments_out.push(segment);
}

void TCPSender::state_flow() {
    if (_state == 0 && next_seqno_absolute() != 0) {
        _state = 1;
    } else if (_state == 1 && next_seqno_absolute() != bytes_in_flight()) {
        _state = 2;
    } else if (_state == 2 && (stream_in().eof() && next_seqno_absolute() >= stream_in().bytes_written() + 2)) {
        _state = 3;
    } else if (_state == 3 && bytes_in_flight() == 0) {
        _state = 4;
    } else {
        _state = _state;
    }
}
