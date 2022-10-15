#include "byte_stream.hh"

#include <cstring>
#include <iostream>
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    capacity_ = capacity;
    end_write_ = false;
    read_pointer_ = 0;
    write_pointer_ = 0;
    read_count_ = 0;
    write_count_ = 0;
    buffer_ = new char[capacity];
}

ByteStream::~ByteStream() { delete[] buffer_; }



size_t ByteStream::write(const string &data) {
    size_t have_write_size = 0;
    size_t i = 0;

    while (write_pointer_ < capacity_ && i < data.size()) {
        buffer_[write_pointer_] = data[i++];
        write_pointer_++;
        have_write_size++;
    }
    write_count_ += have_write_size;
    return have_write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t can_peek = min(len, write_pointer_);
    // copy len bytes from the output side of the buffer
    char *peek_str = new char[can_peek + 1];
    peek_str[can_peek] = '\0';
    strncpy(peek_str, buffer_, can_peek);

    return peek_str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > write_pointer_ + 1) {
        set_error();
        cerr << "Error! len must less than output buffer size" << endl;
    }
    //将内存左移动
    memmove(buffer_, buffer_ + len, capacity_ - len);
    //删除从capacity_-len后面所有的字符
    write_pointer_ -= len;
    memset(buffer_ + capacity_ - len, '4', len);
    read_count_ += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    char *read_str = new char[len + 1];
    read_str[len] = '\0';
    memcpy(read_str, buffer_, len);
    //删除读出的字符串
    pop_output(len);
    return read_str;
}

void ByteStream::end_input() { end_write_ = true; }

bool ByteStream::input_ended() const { return end_write_; }

size_t ByteStream::buffer_size() const { return write_pointer_; }

bool ByteStream::buffer_empty() const { return write_pointer_ == 0; }

bool ByteStream::eof() const { return end_write_ && buffer_size() == 0; }

size_t ByteStream::bytes_written() const { return write_count_; }

size_t ByteStream::bytes_read() const { return read_count_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - write_pointer_; }
