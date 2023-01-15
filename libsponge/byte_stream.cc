#include "byte_stream.hh"
#include<cstring>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : buffer(),t_capacity(capacity-1),read_size(0),writed_size(0),is_eof(false){}

size_t ByteStream::write(const string &data) {
    // 分情况讨论
    // 1. buffer已满，则直接返回0
    // 2. buffer还有空间，则依次放进去，每放进入一个字符，writed_size++，直到data放完，或者eof,writed_size >= t_capacity
    // 4. 在放的过程中，如果发现了eof，则说明到达结尾了，则直接丢弃剩下的不用放,但是eof是人为指定的，通过end_input函数
    if( !remaining_capacity() ){
        return 0;
    }
    size_t index = 0;
    while( remaining_capacity() > 0 && index < data.size()){
        char c = data[index];
        index++;
        buffer.push_back(c);
        writed_size++;
    }
    return index;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_size = min(len,buffer.size());
    return string(buffer.begin(),buffer.begin()+peek_size);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t pop_size = min(len,buffer.size());
    read_size += pop_size;
    for(size_t i =0;i < pop_size;i++){
        buffer.pop_front();
    }
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // 读取也要分情况考虑，
    // 1 是否有可读的，即writed_size > readed_size?
    // 2 如果有可读的，则可以读，但是需要考虑len和writed_size-readed_size之间的对比
    // 3. 读取过程中，每读取一个字符，则需要更新一次read_size
    if(writed_size <= read_size){
        return "";
    }
    std::string result = peek_output(len);
    pop_output(len);
    return result;
}

void ByteStream::end_input() { is_eof = true; }

bool ByteStream::input_ended() const { return is_eof; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return is_eof && buffer_empty(); }

size_t ByteStream::bytes_written() const { return writed_size; }

size_t ByteStream::bytes_read() const { return read_size; }

size_t ByteStream::remaining_capacity() const { return t_capacity+1 - buffer.size(); }
