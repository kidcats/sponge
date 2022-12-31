#include "stream_reassembler.hh"
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    unread_index(0),unreassembler_index(0),buffer(),buffer_index(0),_output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 接受data数据，初始byte的索引是index,然后eof用于指定是否达到结尾
    // 1. 如果来的字符长度+index>capacity,说明需要截断部分，只保留在capacity范围的部分
    // capacity包含以下几个部分
    //                      1. number of reassembled bytestream
    //                      2. maximum number of bytes that can be used unassembled

    //                            first unread               first unassembled           unacceptiable
    //     0                           |                       capacity   （remaining_capacity |
    //     |******* stream start ******|++++++  re-assembled  ++++++|------  unassembled ------|====>
    // 填充的步骤是先将截取后的data打成char数组，然后按照给定的index，按照顺序填充到buffer中
    for(size_t i=0;i<data.size();i++){
        if(buffer.find(index+i) != buffer.end()){
            buffer.insert(pair<size_t,char>(index+i,data[i]));
        }
    }
    // 然后根据buffer中数据填充到_output中，用try_reassembler函数 
    try_reassembler(index);
    // 最后再来处理eof的问题，只有当传来过的数据全部写入_output的时候eof才有效吧？
    if(eof && buffer.empty()){
        _output.end_input();
    }
}

void StreamReassembler::try_reassembler(const size_t index){
    // 尝试重组，直接比较buffer_index与新传进来的index的值，如果index > buffer_index,则无法重组，直接返回
    // 如果<= 说明覆盖到了，可以直接重组，则按照buffer_index开始，依次往_output里面填充，直到填满为止
    // 别忘了在填充的过程中更新buffer_index
    if(index>buffer_index) return;
    std::string result;
    while(true && _output.remaining_capacity() > 0){ // 不能超出_capacity的范围
        // 有该序列对应的字符，则组装
        auto it = buffer.find(buffer_index);
        if(it != buffer.end()){
            result += it->second;
            buffer.erase(it); // 删除对应的元素
        }else{
            break;
        }
        buffer_index++;
    }
    std::cout<<"sdfasdfsdfsd"<<result<<std::endl;
    size_t write_len = _output.write(result);
    // 要用写入的write_len更新内部的指标
    unreassembler_index += write_len;
}


size_t StreamReassembler::unassembled_bytes() const { return buffer.size(); }

bool StreamReassembler::empty() const { return buffer.empty(); }
