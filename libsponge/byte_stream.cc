#include "byte_stream.hh"
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _buf(capacity + 1), _capacity(capacity), head(0), tail(_capacity), wr_cnt(0), rd_cnt(0) {}

size_t ByteStream::write(const string &data) {
    auto num = min(data.size(), remaining_capacity());
    for (size_t i = 0; i < num; ++i) {
        tail = (tail + 1) % (_capacity + 1);
        _buf[tail] = data[i];
    }
    wr_cnt += num;
    return num;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string _str;
    auto num = min(len, buffer_size());
    for (size_t i = 0; i < num; ++i) {
        _str.push_back(_buf[(head + i) % (_capacity + 1)]);
    }
    return _str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto num = min(len, buffer_size());
    head = (head + num) % (_capacity + 1);
    rd_cnt += num;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string _str = peek_output(len);
    pop_output(len);
    return _str;
}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return (tail - head + 1 + _capacity + 1) % (_capacity + 1); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return (buffer_empty() && input_ended()); }

size_t ByteStream::bytes_written() const { return wr_cnt; }

size_t ByteStream::bytes_read() const { return rd_cnt; }

size_t ByteStream::remaining_capacity() const { return (_capacity - buffer_size()); }
