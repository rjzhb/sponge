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
        if (unassembled_bytes_ + _output.buffer_size() > _capacity) {
            break;
        }

        if (k >= start_pos_) {
            //如果当前插入的就是需要找的start_pos_
            if (k == start_pos_) {
                if (_output.buffer_size() >= _capacity) {
                    continue;
                }
                //如果之前map里存在这个key，那就删除原先map的key，这是一种假删除
                if (str_map_.find(k) != str_map_.end()) {
                    unassembled_bytes_--;
                }

                string write_str = "";
                write_str.push_back(data[i]);
                //写入 缓冲区
                size_t write_size = _output.write(write_str);
                //起点+1
                start_pos_++;
                //达到终结符
                if (write_size > 0 && k == end_pos_) {
                    _output.end_input();
                    return;
                }
                continue;
            }

            //否则插入进待组装列表里
            //如果之前的map找不到这个key
            if (str_map_.find(k) == str_map_.end()) {
                unassembled_bytes_++;
            }
            str_map_[k] = data[i];
        }
    }

    //二次遍历map，放入ByteStream
    for (auto iter = str_map_.begin(); iter != str_map_.end(); iter++) {
        size_t key = iter->first;
        char value = iter->second;
        if (key == start_pos_) {
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
            //删除此key,不真正删除，只是以后会忽略掉
            unassembled_bytes_--;
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_bytes_; }

bool StreamReassembler::empty() const { return str_map_.size() == 0; }