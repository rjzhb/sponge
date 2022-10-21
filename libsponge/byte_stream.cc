#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {

}

size_t ByteStream::write(const string &data) {
    size_t len = 0;
    if (_data_stream.size() + data.size() <= _capacity) {
        _data_stream += data;
        len = data.size();
    } else {
        len = _capacity - _data_stream.size();
        _data_stream += data.substr(0, len);
    }
    _write_cnt += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string res;
    if (len < _data_stream.size()) res += _data_stream.substr(0, len);
    else res += _data_stream.substr(0, _data_stream.size());
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {

    if(len < _data_stream.size()){
        _data_stream.erase(0, len);
        _read_cnt += len;
    }
    else {
        _read_cnt += _data_stream.size();
        _data_stream = "";
    }


}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string res = peek_output(len);
    pop_output(len);

    return res;
}
//结束输入
void ByteStream::end_input() {_is_eof = true;}
//判断是否结束输入
bool ByteStream::input_ended() const { return  _is_eof; }
//当前缓冲池大小
size_t ByteStream::buffer_size() const { return  _data_stream.size();}
//是否为空
bool ByteStream::buffer_empty() const { return _data_stream.empty(); }
//判断是否到达结尾
bool ByteStream::eof() const { return _data_stream.empty() && _is_eof; }
//返回写入的字节数量
size_t ByteStream::bytes_written() const { return _write_cnt; }
//返回读取的字节数量
size_t ByteStream::bytes_read() const { return _read_cnt; }
//返回剩余容量
size_t ByteStream::remaining_capacity() const { return _capacity - _data_stream.size(); }
