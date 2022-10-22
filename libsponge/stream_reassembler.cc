#include "stream_reassembler.hh"

#include <algorithm>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _map(), _set(), _idx_ptr(0), _idx_end(1e9) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t end = index + data.size();

    if (eof)
        _idx_end = end;
    // 当前字符超过容量
    if (_idx_ptr + _capacity <= index)
        return;

    if (_idx_ptr >= _idx_end)
        _output.end_input();
    if (end <= _idx_ptr)
        return;
    /**/
    size_t t;
    if (_idx_ptr >= index)
        t = _idx_ptr - index;
    else
        t = 0;
    for (size_t i = t; i < data.size(); i++) {
        if (i + index >= _idx_end)
            break;
        _map[i + index] = data[i];
        _set.insert(i + index);
    }
    std::string res;
    size_t idx = _idx_ptr;
    while (_map.count(idx)) {
        if (idx >= _idx_end)
            break;
        // 超过 _capacity 容量了 ，不能直接返回， 需要判断map最后一个位置和当前位置哪一个比较大， 再判断是否需要删除
        /*if(_map.size() + _output.buffer_size() > _capacity){
            size_t temp = * _set.rbegin();
            if(temp > idx){
                _map.erase(temp);
                _set.erase(temp);
            } else break;
        }*/
        res += _map[idx++];
    }
    size_t ret = _output.write(res);
    for (size_t i = 0; i < ret; i++) {
        _map.erase(_idx_ptr + i);
        _set.erase(_idx_ptr + i);
    }
    _idx_ptr += ret;
    // 删除超过字符容量的字符
    idx = _idx_ptr;
    while (_map.count(idx + _capacity)) {
        _map.erase(idx + _capacity);
        _set.erase(idx + _capacity);
        idx++;
    }
    if (_idx_ptr >= _idx_end)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _map.size(); }

// 当没有输入状态等待的时候返回 true
bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }