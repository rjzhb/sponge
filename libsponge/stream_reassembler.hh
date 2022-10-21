#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <set>

typedef std::pair<size_t, std::string> p;
//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
/*流重组*/

/*
 * 实现思路 ，区间插入问题， string 拼接问题
 * map 存放idx, string
 * vector<vector<int>> v 存放数组
 * 插入时候判断是否可以进行区间合并（可以使用二分）
 *
 *
 * */
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    /* 输入输出流 */
    ByteStream _output;  //!< The reassembled in-order byte stream
    /* 字节容量 */
    size_t _capacity;    //!< The maximum number of bytes
    std::unordered_map<size_t, char> _map;
    std::set<size_t> _set;
    size_t _idx_ptr;
    size_t _idx_end;
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring 子串
    //! \param index indicates the index (place in sequence) of the first byte in `data` 序号
    //! \param eof the last byte of `data` will be the last byte in the entire stream  标号当前片段是否是最后一个分片
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
