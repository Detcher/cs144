#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) { return write(string_view(data)); }

size_t ByteStream::write(const string_view &data) {
    size_t num = min(data.size(), remaining_capacity());
    // optimize point #0: use emplace_back
    _buffer.emplace_back(std::move(string().assign(data.begin(), data.begin() + num)));
    _wr_cnt += num;
    return num;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t num = min(len, buffer_size());
    string str;
    str.reserve(num);  // reserve some space in advance
    for (const auto &buffer : _buffer) {
        if (num >= buffer.size()) {
            str.append(buffer);
            num -= buffer.size();
            if (num <= 0)
                break;
        } else {
            // optimize point #1: no need to use 'buffer.copy'
            auto buffer_view = buffer.str();
            // string::append() dont have overload for rvalue-ref, use '+' instead
            // str += std::move(string().assign(buffer_view.begin(), buffer_view.begin() + num));
            str.append(buffer_view, 0, num);
            break;
        }
    }
    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t num = min(len, buffer_size());
    if (num <= 0)
        return;
    _rd_cnt += num;
    while (num > 0) {
        if (num >= _buffer.front().size()) {
            num -= _buffer.front().size();
            _buffer.pop_front();
        } else {
            _buffer.front().remove_prefix(num);
            break;
        }
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string str = peek_output(len);
    pop_output(len);
    return str;
}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return _wr_cnt - _rd_cnt; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_end && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _wr_cnt; }

size_t ByteStream::bytes_read() const { return _rd_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }