#include "stream_reassembler.hh"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <sys/types.h>
#include <utility>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _expected_index(0)
    , _eof_index(numeric_limits<uint64_t>::max())
    , _unassembled_strs()
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // the last substring
    if (eof)
        _eof_index = index + data.size();
    // compare with the expected index
    size_t head_offset = index < _expected_index ? _expected_index - index : 0;
    if (head_offset >= data.size()) {  // fully included => discard
        // in case the blanky data with EOF
        if (empty() && _expected_index == _eof_index)
            _output.end_input();
        return;
    }
    string trimmed_data = data.substr(head_offset);
    uint64_t head_index = index + head_offset;
    if (head_index > _expected_index) {
        checked_insert(trimmed_data, head_index);
    } else {  // should be equal
        trimmed_data = truncate_substring(trimmed_data, head_index);
        size_t wr_cnt = _output.write(trimmed_data);
        _expected_index += wr_cnt;
        // then update the map
        while (1) {
            update_map();
            auto iter = _unassembled_strs.find(_expected_index);
            if (iter == _unassembled_strs.end())
                break;
            wr_cnt = _output.write((*iter).second);
            _expected_index += wr_cnt;
            _unassembled_strs.erase(iter);
        }
    }

    if (empty() && _expected_index == _eof_index)
        _output.end_input();

    // auto pos_iter = _unassembled_strs.upper_bound(index);
    // if (pos_iter != _unassembled_strs.begin())
    //     pos_iter--;

    // uint64_t new_idx = index;
    // if (pos_iter != _unassembled_strs.end() && pos_iter->first <= index) {
    //     const uint64_t up_idx = pos_iter->first;

    //     if (index < up_idx + pos_iter->second.size())
    //         new_idx = up_idx + pos_iter->second.size();
    // } else if (index < _expected_index) {
    //     new_idx = _expected_index;
    // }

    // const uint64_t data_start_pos = new_idx - index;
    // ssize_t data_size = data.size() - data_start_pos;

    // pos_iter = _unassembled_strs.lower_bound(new_idx);
    // while (pos_iter != _unassembled_strs.end() && new_idx <= pos_iter->first) {
    //     const uint64_t data_end_pos = new_idx + data_size;
    // }

    // size_t trim_size;
    // if (index <= _expected_index) {
    //     trim_size = _expected_index - index;
    // } else {

    // }
    // auto upper_bound = _unassembled_strs.upper_bound(_expected_index);
}

string StreamReassembler::truncate_substring(const string &data, uint64_t index) {
    size_t trunc_size = min(data.size(), _capacity + _output.bytes_read() - index);
    trunc_size = min(trunc_size, _capacity - _output.buffer_size() - unassembled_bytes());
    return data.substr(0, trunc_size);
}

void StreamReassembler::update_map() {
    for (auto iter = _unassembled_strs.begin(); iter != _unassembled_strs.end();) {
        uint64_t index = (*iter).first;
        if (index < _expected_index) {
            string data = (*iter).second;
            iter = _unassembled_strs.erase(iter);
            if (index + data.size() > _expected_index) {
                data = data.substr(_expected_index - index);
                index = _expected_index;
                checked_insert(data, index);
            }
        } else {
            ++iter;
        }
    }
}

void StreamReassembler::checked_insert(string data, uint64_t index) {
    uint64_t start = index, end = index + data.size() - 1;
    for (auto iter = _unassembled_strs.begin(); iter != _unassembled_strs.end();) {
        const string &str = (*iter).second;
        uint64_t str_start = (*iter).first, str_end = str_start + str.size() - 1;
        // overlapped
        if (start <= str_end && end >= str_start) {
            if (start <= str_start && end >= str_end) {
                iter = _unassembled_strs.erase(iter);
            } else if (str_start <= start && end <= str_end) {
                data.clear();
                ++iter;
            } else {
                if (start <= str_start) {
                    data += str.substr(end + 1 - str_start);
                } else {
                    index = str_start;
                    data.insert(0, str.substr(0, start - str_start));
                }
                iter = _unassembled_strs.erase(iter);
            }
        } else {
            ++iter;
        }
    }
    if (!data.empty())
        _unassembled_strs.insert(make_pair(index, truncate_substring(data, index)));
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t cnt = 0;
    for (auto &str : _unassembled_strs)
        cnt += str.second.size();
    return cnt;
}

bool StreamReassembler::empty() const { return _unassembled_strs.empty(); }
