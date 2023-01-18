#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity),_next_seqno(0)
    , _rto(retx_timeout)
    , _syn(false),_fin(false)
    , _window_size(0),_recv_ack_seqno(0)
    , buffer(),_byte_size_in_fly(0),_state(0)
    , _con_retrans_nums(0),_time_stmp(0)
    {}

uint64_t TCPSender::bytes_in_flight() const { return _byte_size_in_fly; }

void TCPSender::fill_window() {
    state_flow();// 先执行状态转移
    if(_state <= 1) return;
    fill_segment();
    // 发送完，启动计时器
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    state_flow();
    // 如果不对，说明有问题
    if(_state <= 1){
        _rto = _initial_retransmission_timeout;
        return;
    }
    // 如果是状态2，3，4，5,说明上一个已经接受，则需要将其从buffer中删除，然后将重置_rto,
    // 更新_recv_abs_seqno和_byte_size_in_fly，_con_retrans_nums
    // 收到的有可能是4种ack,第一种是带syn的，第二种是不带syn和fin，第三种是只带fin，第四种是带syn和fin的
    // 不管是哪一种，都可以判断是否是在buffer的头部，如果在，则可以弹出
    // 然后重置计时器和超时次数
    _window_size = window_size;
    _recv_ack_seqno = unwrap(ackno,_isn,_recv_ack_seqno);
    while(!buffer.empty()){
        TCPSegment segment = buffer.front();
        if(_recv_ack_seqno < unwrap(segment.header().seqno,_isn,_recv_ack_seqno) + segment.length_in_sequence_space()){
            return;
        }
        buffer.pop();
        _next_seqno = _recv_ack_seqno;
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
    if(_state == 1 || _state == 2){
        return ;
    }
    // 如果超时，则从buffer中从前面弹出
    if(_time_stmp >= _rto && !buffer.empty()){
        TCPSegment segment = buffer.front();
        _segments_out.push(segment);
        _time_stmp = 0;
        _con_retrans_nums += 1;
        _rto *= 2;
    }

 }

unsigned int TCPSender::consecutive_retransmissions() const { return _con_retrans_nums; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_recv_ack_seqno,_isn);
    _segments_out.push(segment);
}

void TCPSender::state_flow(){
    if(_state == 0){
        if(!_syn && not stream_in().eof()){
            _state = 2;
        }else if(stream_in().eof() && !_syn){
            _state = 5;
        }else{
            _state = _state;
        }
    }else if(_state == 2){
        if(!stream_in().eof() && _syn && !_fin && next_seqno_absolute() != bytes_in_flight()){
            _state = 3;
        }
        if(stream_in().eof() && _syn){
            _state = 4;
        }
    }else if(_state == 3){
        if(stream_in().eof() && _syn){
            _state = 4;
        }
    }
    else{
        _state = _state;
    }
}

void TCPSender::fill_segment(){
    // 根据不同的状态选择合适的segment
    if(_state <= 1){
        return ;
    }
    TCPSegment segment;
    if(_state == 2){
        if(_syn){
            return;
        }
        segment.header().syn = true;
        _next_seqno += 1; // 这里出了问题，即如果重复进入状态2这个就寄了
    }else if(_state == 4){
        segment.header().fin = true;
        _next_seqno += 1;
    }else if(_state == 5){
        if(_syn){
            return;
        }
        segment.header().syn = true;
        segment.header().fin = true;
        _next_seqno += 2;
    }else{
        segment = segment;
    }
    // 根据ackno和windows size 组装
    segment.header().seqno = wrap(_recv_ack_seqno,_isn);
    size_t win_size = _window_size <= 0 ? 1 : _window_size;
    // 不停的往里面塞segment，塞满win_size为止
    if(not stream_in().buffer_empty()){
        size_t send_size = min(TCPConfig::MAX_PAYLOAD_SIZE,win_size);
        segment.payload() = stream_in().read(send_size); // 这个读不会超过buffer的范围
        win_size -= segment.length_in_sequence_space();
    }
    // 有很多中情况不用发，
    // 只能发带长度的，不带长度的不用发
    if(_state == 3 && segment.length_in_sequence_space() == 0){
        return;
    }
    _segments_out.push(segment);
    buffer.push(segment);
    _byte_size_in_fly += segment.length_in_sequence_space();
    _time_stmp = 0;
    _con_retrans_nums = 0;
    // 更新_next_seq和byte in fly
    if(_state == 2 ){ // 带syn 和 fin中的一个
        _syn = true;
    }else if(_state == 4){
        _fin = true;
    }else if(_state == 5){
        _fin = true;
        _syn = true;
    }else{
        _byte_size_in_fly += 0;
    }
}
