#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), start_pos_(0), end_pos_(-1), unassembled_bytes_(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    //对空串特殊处理
    if (data.size() == 0 && eof) {
        _output.end_input();
        return;
    }

    if (eof && data.size() > 0) {
        end_pos_ = index + data.size() - 1;
    }

    size_t k = index;
    for (size_t i = 0; i < data.size(); i++, k++) {
        //超过了当前最大限制容量
        if (unassembled_bytes_ + _output.buffer_size() >= _capacity) {
            break;
        }

        if (k >= start_pos_) {
            str_map_[k] = data[i];
            index_set_.insert(k);
            //如果当前插入的就是需要找的start_pos_
            if (k == start_pos_) {
                if (_output.buffer_size() >= _capacity) {
                    continue;
                }

                string write_str = "";
                write_str.push_back(data[i]);
                //写入 缓冲区
                size_t write_size = _output.write(write_str);
                //起点+1
                start_pos_++;
                //删除key
                str_map_.erase(k);
                index_set_.erase(k);
                //达到终结符
                if (write_size > 0 && k == end_pos_) {
                    _output.end_input();
                    return;
                }
                continue;
            }
        }
    }

    size_t idx = start_pos_;
    //二次遍历map，放入ByteStream
    while (str_map_.count(idx)) {
        if (str_map_.size() + _output.buffer_size() >= _capacity) {
            size_t max_index = *index_set_.end();
            index_set_.erase(max_index);
            str_map_.erase(max_index);
        }

        size_t key = idx;
        char value = str_map_[idx];
        string write_str = "";
        write_str.push_back(value);
        //写入缓冲区
        size_t write_size = _output.write(write_str);

        //达到终结符
        if (write_size > 0 && key == end_pos_) {
            _output.end_input();
            return;
        }

        //起点+1
        start_pos_++;
        //删除此key
        str_map_.erase(idx++);
    }
}

size_t StreamReassembler::unassembled_bytes() const { return str_map_.size(); }

bool StreamReassembler::empty() const { return str_map_.size() == 0; }