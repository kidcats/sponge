#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
#include<iostream>

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 主要做什么，通过接受的数据，然后交给内部的resamber，然后通知sender
    // 主要考虑两个因素 1. ackno，已经确认的序号，receiver希望得到的当前序号
    //                2. windows size,即当前还有多少空间可以使用，即unassembled --- unacceptable
    //  
    if (_isn == nullopt && !seg.header().syn){
        return;
    }
    TCPHeader _header = seg.header();
    _fin = _header.fin;
    _syn = _header.syn; 
    string data = seg.payload().copy();   
    // 1. set initial sequence,if syn  
    // 2. push any data  or end of stream to streamreassmebler stream_index
    if(_state == 0 && _syn){
        _state = 1;
        _isn = _header.seqno;// 第一次设置的地方
        _checkpoint = 0;
    }
    if(_state == 1 && _fin){
        _state = 2;
    }else{
        _state = _state;
    }
    // if _state == 1,now ，需要将数据写入，然后更新一些东西
    const uint64_t abs_seq = unwrap(_header.seqno,_isn.value(),_checkpoint);
    _checkpoint += seg.length_in_sequence_space();
    std::cout<<"abs_seq " << abs_seq << " _checkpoint" << _checkpoint << std::endl;
    _reassembler.push_substring(data,abs_seq+(_syn)-1,_fin);
    
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(_state == 0){
        return nullopt;
    }
    uint64_t written  = _reassembler.ackno() + 1; 
    if(stream_out().input_ended()){
        written+=1;
    }
    return wrap(written,_isn.value());
 }

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
